#!/usr/bin/env bash
# ARTIFACTS contains source/ and build/ configured at /artifacts/{source,build}.
# Docker creates a new network namespace and a temporary MongoDB every run.
set -euo pipefail
artifacts=$(realpath "${1:?Usage: $0 ARTIFACTS RUN_NAME [dynamic|static|incompatible|recovery|recovery-cp|regression|epc]}")
run_name=${2:?run name required}
mode=${3:-dynamic}
[[ $run_name =~ ^[a-zA-Z0-9_-]+$ ]] || exit 2
build_image=${BUILD_IMAGE:-sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551}
mongo_image=${MONGO_IMAGE:-mongo@sha256:8c3ce64d1a433bf57ea79035b48a38ea3e532997a1328fe3266e2e2e8bfb41b6}
lab_id= mongo_id=
cleanup() {
    if [[ -n $mongo_id ]]; then
        docker logs "$mongo_id" > "$artifacts/$run_name-mongo.log" 2>&1 || true
        docker rm -f "$mongo_id" >/dev/null || true
    fi
    if [[ -n $lab_id ]]; then docker rm -f "$lab_id" >/dev/null || true; fi
}
trap cleanup EXIT INT TERM
lab_id=$(docker run -d --network none --cap-add NET_ADMIN --device /dev/net/tun \
    --label open5gs.dynamic-dnn.lab=true -v "$artifacts:/artifacts" \
    -w /artifacts/source "$build_image" sleep infinity)
mongo_id=$(docker run -d --network "container:$lab_id" --tmpfs /data/db \
    --label open5gs.dynamic-dnn.lab=true "$mongo_image" mongod --bind_ip 127.0.0.1)
docker inspect "$lab_id" "$mongo_id" > "$artifacts/$run_name-containers.json"
docker exec "$lab_id" python3 -c 'import socket,time
for _ in range(100):
 try:
  with socket.create_connection(("127.0.0.1",27017),.2): break
 except OSError: time.sleep(.1)
else: raise SystemExit("MongoDB not ready")'
docker exec "$lab_id" ninja -C /artifacts/build tests/pfcp/dnn-session-driver
if [[ $mode == regression || $mode == epc ]]; then
    docker exec "$lab_id" python3 tests/pfcp/existing-regressions.py \
        /artifacts/build "/artifacts/$run_name" "$mode"
else
    docker exec "$lab_id" python3 tests/pfcp/dynamic-dnn-integration.py \
        /artifacts/build "/artifacts/$run_name" "$mode"
fi
