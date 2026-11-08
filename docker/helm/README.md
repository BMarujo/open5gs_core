# Open5GS image for Gradiant Helm charts

Current publication: `docker.io/bmarujo/open5gs:dnn-384a54f4d9a0`, digest
`sha256:a05e25b5cc4e39defa42b87fed82bcd0ace632ce477a53059f6ebee6fdffdb8b`.
It contains the final extendable-IE correction and IPv6/slice test updates from
commit `384a54f4d9a0096e12fd1d231df8a2e2b524dd4a`. Build tests and anonymous
registry retrieval passed. Rollout/performance evidence is recorded separately
in the project documentation; publication alone is not deployment validation.

This image contains all Open5GS NF executables from one source snapshot. The
chart chooses the daemon per Pod. It supplies Bash, iproute2, iptables and the
capture/probe tools used by Gradiant's UPF init container and probes. Configuration
lives under `/opt/open5gs/etc/open5gs`, matching the charts. The entrypoint forwards
arguments/signals without creating TUN devices; the chart owns that setup.

The multi-stage Dockerfile compiles with Meson/Ninja, runs core/crypt/unit/selector
tests, installs into `/opt/open5gs`, then copies that installation into a compatible
Ubuntu 24.04 runtime. Do not copy binaries built against newer libraries into an
older Gradiant Debian runtime. Both modified daemons and shared libraries must
come from this build. No subscriber or registry credentials belong in the image.

Build context contains `source/` (recorded Git revision plus the pinned Meson
vendors), this Dockerfile and `entrypoint.sh`. The validated toolchain is
`open5gs-pfcp-codec-build:20261007`, ID
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`.
A public image's consumers do not need the builder. Pass a pinned runtime digest,
REVISION and CODE_SHA256 build arguments, and record the resulting registry digest.
The source snapshot includes the complete local implementation; the upstream
Gradiant recipe's unmodified upstream Git clone would omit it.

Only SMF/UPF workloads need this image for the dynamic-DNN feature. Enable learning
and advertisement separately in complete Helm `customOpen5gsConfig` objects.
Set explicit persistent pool IDs, preserve compatible SMF/UPF pools and verify N4,
N3 and return routing. Legacy OR/location and fallback are unchanged.

Packaging is adapted to the runtime contract of Gradiant 5g-charts; their original
multi-stage image recipe is at
https://github.com/Gradiant/5g-images/blob/0e5a69cf83c120ce817a7ded5fa8d35b46cbeab6/images/open5gs/Dockerfile.

Validated public build (2026-11-04):
`docker.io/bmarujo/open5gs:dnn-055eb4a770d0`, pinned registry digest
`sha256:1e125d1f016fa9f3df6354588ebdaee2e281c637157f923003cf0570546d37fa`.
Anonymous manifest access and actual Kubernetes imageIDs were verified.
Gradiant umbrella chart 2.3.4 required no template modifications. See
[the cluster report](../../docs/design/dynamic-dnn-cluster.md) for exact scope,
first-request evidence, mixed-version SBI refresh and the unavailable edge worker.

To build a fresh local Helm image, use a new context with `source/` from the
verified final source archive (or `prepare-build.py` with its pinned Git vendors).
Copy this Dockerfile and `entrypoint.sh` to that context root. Read the full
core hash and revision from its recorded `source-identity.json`; include the
dirty patch in the source archive when the revision is uncommitted.

```sh
docker tag sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551 \
  open5gs-pfcp-toolchain:sha256-7e59045bafdd
test "$(docker image inspect --format '{{.Id}}' open5gs-pfcp-toolchain:sha256-7e59045bafdd)" \
  = sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551
docker build --pull=false \
  --build-arg TOOLCHAIN_IMAGE=open5gs-pfcp-toolchain:sha256-7e59045bafdd \
  --build-arg REVISION=384a54f4d9a0096e12fd1d231df8a2e2b524dd4a \
  --build-arg CODE_SHA256=d8e528dfc3abe4c66856b8f75e364d8bac1758eebbff2b61ab9c52781bf5242c \
  -t bmarujo/open5gs:dnn-384a54f4d9a0 /path/to/fresh-context
```

This form was executed for the current publication, using a verified fresh
source/vendor context. A bare `sha256:<config-id>` in BuildKit FROM is interpreted
as an image repository, so the verified local tag is required. Compilation/tests
execute in the Dockerfile; runtime packages need network access and are not
version-locked. Record the resulting image ID and verify its
tools/configuration/privileges before choosing a tag/digest for a rollout.
Publication and Helm upgrade are separate actions; no push is included above.
