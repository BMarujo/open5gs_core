# Helm image and Kubernetes validation

2026-11-04. The user explicitly authorized committing/pushing the implementation,
publishing a public Docker Hub image and updating the existing SMF/UPF Helm
workloads. This authorization supersedes the earlier stage-5 restriction on
publication and existing deployment changes. Strict stage 4B stays excluded.

The deployment repository uses Gradiant umbrella chart 2.3.4 with Open5GS 2.7.7
images; do not silently upgrade the chart or unrelated NFs. The local newer chart
clone is a reference only. A pre-existing unavailable worker leaves one edge SMF
and three edge UPFs unschedulable. Preserve placement and classify this separately
from implementation/image regressions. Record original Helm values, manifests,
history, workload identities and a targeted rollback before upgrades.

`docker/helm/Dockerfile` provides one image containing all NF executables. It
compiles the recorded source and pinned vendors in a build stage, runs the four
selected suites, then installs the result and compatible libraries into an Ubuntu
runtime with Bash, networking/capture tools and the standard chart config paths.
Both main and UPF init containers can use this image. The chart owns TUN/NAT setup;
the image entrypoint only forwards the command and signals.

For the feature, update only SMF/UPF image references and complete custom configs:
SMF learning enabled, no per-peer static mappings; UPF advertisement enabled with
stable explicit pool identities and an SMF PFCP destination. Preserve operator
DNNs, pools, slices, N4 interfaces and N3 settings. Additional test resources and
subscriber records must have unique identities and ownership; never bulk-delete
subscriber data or clean cluster networking.

Status: packaging and cluster validation in progress. Build/push results, exact
commit/image digest, first PFCP session requests and PASS/FAIL/NOT RUN boundaries
will be recorded after execution. No cluster success is claimed by this draft.
