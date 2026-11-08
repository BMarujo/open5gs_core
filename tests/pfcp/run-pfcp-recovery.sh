#!/usr/bin/env bash
# Real NF recovery and namespace-local packet loss. Never touches host rules.
set -euo pipefail
artifacts=$(realpath "${1:?Usage: $0 ARTIFACTS RUN_NAME DIRECTION INITIATOR [stalled|recover|brief]}")
name=${2:?run name}; direction=${3:?smf|upf|both}; initiator=${4:?up|cp}; expectation=${5:-recover}
scenario=${6:-pair}
[[ $name =~ ^[a-zA-Z0-9_-]+$ ]] || exit 2
repo=$(cd "$(dirname "$0")/../.." && pwd)
mkdir "$artifacts/$name"
build_image=${BUILD_IMAGE:-sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551}
net_image=$(docker image inspect -f '{{.Id}}' "${NET_IMAGE:-open5gs-pfcp-nettools:nft-1.1.0-1370dbd7f467}")
lab= net= controller= mongo=
cleanup() {
    if [[ -n $controller ]]; then kill "$controller" 2>/dev/null || true; wait "$controller" || true; fi
    if [[ -n $mongo ]]; then docker logs "$mongo" > "$artifacts/$name/mongo.log" 2>&1 || true; docker rm -f "$mongo" >/dev/null || true; fi
    if [[ -n $net ]]; then docker rm -f "$net" >/dev/null || true; fi
    if [[ -n $lab ]]; then docker rm -f "$lab" >/dev/null || true; fi
}
trap cleanup EXIT INT TERM
lab=$(docker run -d --network none --cap-add NET_ADMIN --device /dev/net/tun \
    --label open5gs.dynamic-dnn.lab=true -v "$artifacts:/artifacts" "$build_image" sleep infinity)
net=$(docker run -d --network "container:$lab" --cap-add NET_ADMIN \
    --label open5gs.dynamic-dnn.lab=true "$net_image" sleep infinity)
docker inspect "$lab" "$net" > "$artifacts/$name/containers.json"
docker image inspect "$net_image" > "$artifacts/$name/net-image.json"
docker exec "$net" nft --version > "$artifacts/$name/nft-version.txt"
upf_ip=127.0.0.7; [[ $scenario == nas ]] && upf_ip=127.0.0.9
python3 "$repo/tests/pfcp/pfcp-netfilter-control.py" "$net" "$artifacts/$name/control.sock" "$upf_ip" \
    > "$artifacts/$name/control.log" 2>&1 &
controller=$!
for _ in {1..100}; do
    [[ -S $artifacts/$name/control.sock ]] && break
    kill -0 "$controller"; sleep .05
done
[[ -S $artifacts/$name/control.sock ]]
if [[ $scenario == nas ]]; then
    [[ $initiator == up && $direction != upf || $initiator == cp && $direction == upf ]]
    mongo=$(docker run -d --network "container:$lab" --tmpfs /data/db \
        --label open5gs.dynamic-dnn.lab=true \
        mongo@sha256:8c3ce64d1a433bf57ea79035b48a38ea3e532997a1328fe3266e2e2e8bfb41b6 \
        mongod --bind_ip 127.0.0.1)
    docker inspect "$mongo" > "$artifacts/$name/mongo-container.json"
    docker exec "$lab" python3 -c 'import socket,time
for _ in range(100):
 try:
  with socket.create_connection(("127.0.0.1",27017),.2): break
 except OSError: time.sleep(.1)
else: raise SystemExit("MongoDB not ready")'
    docker exec "$lab" python3 /artifacts/source/tests/pfcp/dynamic-dnn-integration.py \
        /artifacts/build "/artifacts/$name" "asymmetric-$direction"
else
    [[ $scenario == pair ]]
    docker exec "$lab" env "ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1}" \
        "UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}" \
        "LSAN_OPTIONS=${LSAN_OPTIONS:-}" \
        "LD_PRELOAD=${PFCP_DIAG_PRELOAD:-}" "PFCP_RETRY_MS=${PFCP_RETRY_MS:-1000}" \
        python3 /artifacts/source/tests/pfcp/pfcp-asymmetric-recovery.py \
        "${RECOVERY_BUILD:-/artifacts/build}" "/artifacts/$name" "$direction" "$initiator" "$expectation"
fi
