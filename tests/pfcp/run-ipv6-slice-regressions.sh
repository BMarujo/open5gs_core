#!/usr/bin/env bash
# All networking and databases belong to a fresh, unconnected Docker namespace.
set -euo pipefail
artifacts=$(realpath "${1:?ARTIFACTS RUN_NAME off|cp|up}")
name=${2:?run name}; mode=${3:?off|cp|up}
scenario=${4:-existing}
[[ $name =~ ^[a-zA-Z0-9_-]+$ && $mode =~ ^(off|cp|up)$ ]] || exit 2
[[ $scenario == existing || $scenario == dynamic && $mode != off ]] || exit 2
build_image=${BUILD_IMAGE:-sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551}
mongo_image=${MONGO_IMAGE:-mongo@sha256:8c3ce64d1a433bf57ea79035b48a38ea3e532997a1328fe3266e2e2e8bfb41b6}
lab= mongo=
cleanup() {
    if [[ -n $mongo ]]; then
        docker logs "$mongo" > "$artifacts/$name-mongo.log" 2>&1 || true
        docker rm -f "$mongo" >/dev/null || true
    fi
    if [[ -n $lab ]]; then docker rm -f "$lab" >/dev/null || true; fi
}
trap cleanup EXIT INT TERM
lab=$(docker run -d --network none --cap-add NET_ADMIN --device /dev/net/tun \
    --label open5gs.ipv6-slice.lab=true -v "$artifacts:/artifacts" \
    -w /artifacts/source "$build_image" sleep infinity)
mongo=$(docker run -d --network "container:$lab" --tmpfs /data/db \
    --label open5gs.ipv6-slice.lab=true "$mongo_image" mongod --bind_ip 127.0.0.1)
docker inspect "$lab" "$mongo" > "$artifacts/$name-containers.json"
docker exec "$lab" python3 -c 'import socket,time
for _ in range(100):
 try:
  with socket.create_connection(("127.0.0.1",27017),.2): break
 except OSError: time.sleep(.1)
else: raise SystemExit("MongoDB not ready")'
if [[ $scenario == dynamic ]]; then
    docker exec "$lab" ninja -C /artifacts/build tests/pfcp/dnn-session-driver
    docker exec "$lab" python3 tests/pfcp/ipv6-dnn-integration.py \
        /artifacts/build "/artifacts/$name" "$mode"
else
    docker exec "$lab" python3 tests/pfcp/ipv6-slice-regressions.py \
        /artifacts/build "/artifacts/$name" "$mode"
fi
