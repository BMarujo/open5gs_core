#!/usr/bin/env bash
# Package the tested build; do not push or contact a registry.
set -euo pipefail
repo=$(realpath "${1:?Usage: $0 REPOSITORY ARTIFACTS}")
artifacts=$(realpath "${2:?artifacts directory required}")
build_image=${BUILD_IMAGE:-sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551}
[[ ! -e $artifacts/runtime-context ]] || { echo 'Use a fresh runtime-context' >&2; exit 2; }
code_sha=$(python3 "$repo/docker/dynamic-dnn/source-identity.py" \
    "$repo" "$artifacts/source" "$artifacts/image-provenance")
revision=$(git -C "$repo" rev-parse HEAD)
tag="${revision:0:12}-dirty-${code_sha:0:12}"
docker image inspect "$build_image" > "$artifacts/image-provenance/compiler-image.json"
docker run --rm --network none -v "$artifacts:/artifacts" -v "$repo:/checkout:ro" \
    "$build_image" python3 /checkout/docker/dynamic-dnn/package-runtime.py \
    /artifacts/build /artifacts/runtime-context
for role in smf upf; do
    image="open5gs-$role:$tag"
    docker build --network none --pull=false --target "$role" \
        --build-arg "REVISION=$revision" --build-arg "CODE_SHA256=$code_sha" \
        --build-arg "BUILD_IMAGE_ID=$build_image" -t "$image" \
        -f "$repo/docker/dynamic-dnn/Dockerfile" "$artifacts/runtime-context"
    docker image inspect "$image" > "$artifacts/image-provenance/$role-image.json"
    docker run --rm --network none --entrypoint /lib64/ld-linux-x86-64.so.2 "$image" \
        --list "/opt/open5gs/bin/open5gs-${role}d" \
        > "$artifacts/image-provenance/$role-runtime-libraries.txt"
    docker run --rm --network none "$image" -h \
        > "$artifacts/image-provenance/$role-help.txt" 2>&1
done
printf '%s\n' "open5gs-smf:$tag" "open5gs-upf:$tag" > "$artifacts/image-provenance/tags.txt"
docker save -o "$artifacts/open5gs-dynamic-dnn-images.tar" \
    "open5gs-smf:$tag" "open5gs-upf:$tag"
sha256sum "$artifacts/open5gs-dynamic-dnn-images.tar" \
    > "$artifacts/image-provenance/image-archive.sha256"
