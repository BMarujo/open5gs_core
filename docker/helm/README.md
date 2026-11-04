# Open5GS image for Gradiant Helm charts

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
