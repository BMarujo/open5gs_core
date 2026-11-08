# Prompt 6: isolated Containernet/K3s validation

Current project status: [final review](dynamic-dnn-final-review.md) and
[operation guide](dynamic-dnn.md). This dated report preserves the results and
limits at its execution date; later corrections/validation do not rewrite it.

2026-11-08. The user designated `open5gs_k8s/deployments-containernet` and the
`experiments/containernet-k3s-topology` emulation for image integration and the
remaining MVP validation. The full dynamic matrix was executed with the other
NFs on Gradiant 2.8.0, with independent discovery/database and actual N6 return
traffic. The normal Containernet profile was installed separately afterward.
No core behavior, selection policy, allocator or session migration was added.
Stage 4B and subsequent stages were not started.

## Target, images and configuration changes

Only the topology's checksum-pinned kubectl/Helm wrappers were used. The explicit
API is `https://10.200.0.11:6443`, context `containernet-k3s-topology`, K3s
`v1.36.3+k3s1`, Kubernetes `v1.36.3`, Linux/amd64. The initially stopped topology
was started through its supported lifecycle script. All four cloud/edge nodes
became Ready. Topology verification and K3s verification passed; the latter
reported 69 PASS / 0 FAIL. The physical deployment tree's existing tracked diff
was verified unchanged; no physical-cluster API was used.

SMF/UPF retain the existing public build:
`docker.io/bmarujo/open5gs:dnn-055eb4a770d0`, digest
`sha256:1e125d1f016fa9f3df6354588ebdaee2e281c637157f923003cf0570546d37fa`.
The other nine NFs use `docker.io/gradiant/open5gs:2.8.0`, digest
`sha256:012d47b034b3ace4fd2fa41e861cd2453bead9a50b12bb60f31c2c1be2209bdb`.
Actual imageIDs and daemon versions were verified. No new build/publication was
needed because the compiled core input was unchanged. The chart stays at 2.3.4;
there are no custom chart-template edits.

In `open5gs_k8s`, the main values, optional edge SMF/UPF values and TAC4 benchmark
values now pin these images. SMFs explicitly enable `learn_upf_dnn`; UPFs enable
`advertise_ue_ip_pools` and have literal persistent pool IDs. Benchmark slots
override their own pool identity independently of Pod IP. Existing DNNs, CIDRs,
Multus N4 addresses and placement policy were preserved in normal profiles.
The benchmark ownership filter recognizes recorded legacy releases and the exact
new image; the existing static suite, including wrong-digest rejection, passed.
Historical placement measurements were not relabelled as new-image results.

Reproducible fixtures and operator instructions are in
`deployments-containernet/open5gs/dynamic-dnn/{run.py,analyze.py,README.md}`.
These are test sequencing and evidence tools; no deployment waiting/orchestration
was introduced into the core.

## Isolation and data path

Successful namespace: `open5gs-dnn-lab-cn-20261108-075712`.
It contained its own NRF, SCP, AMF, AUSF, UDM, UDR, PCF, NSSF, BSF, MongoDB 4.4
emptyDir database, SMF, gNB/UEs and UPFs. No existing discovery/database was reused.
The namespace, all rendered namespaced resources and probe resources had explicit
namespace/ownership. Probe and lab namespace deletion used immutable UID
preconditions. Synthetic subscribers existed only in this disposable database.

NetworkPolicy allowed intra-namespace traffic, cluster DNS and the three DN IPs.
Real SBI probes to/from a separately owned namespace failed in both directions,
while local and same-namespace positive controls passed. K3s rejected these
connections immediately (curl exit 7); policy rules/counters were recorded.
This tests NF discovery isolation, not containment of hostile privileged NFs.

The isolated fixture uses overlay Pod addresses for N4 and N3, avoiding normal
Multus N4 collisions and secondary-interface policy bypass. UPF A was on cloud-2,
B on edge-1 and C on edge-2; gNB/UEs were on edge-2. These paths traverse the
emulated underlay/TC. SMF/MongoDB and ancillary NFs were balanced across the
actual outer Docker memory budgets; Kubernetes advertises the host's capacity.

Configured pools were internet `10.253.40.0/24`, dnn-lab `10.253.41.0/24` on
both B/C, and SMF-only dnn-nomatch `10.253.42.0/24`. IPv4/SST 1 and TAC 3 were
pre-provisioned. The mounted SMF YAML was compared byte-for-byte with its ConfigMap
and parsed to confirm learning and no static per-peer UPF list/location mappings.
Peer location could not explain the positive DNN selection results.

N6 shares UPF eth0 with N3, using Pod-local MASQUERADE and K3s egress NAT. Each
successful check used a distinct ICMP payload pattern, bound to the UE tunnel,
to all three actual DNs: `10.200.0.21`, `10.201.0.21`, `10.201.0.22`.
UPF and DN captures correlate the original UE IP, UL/DL TEIDs, plain N6 packets,
payload, replies and the observed translated source. DN sources were the owning
nodes `10.200.0.12`, `10.201.0.11`, `10.201.0.12`. This closes the earlier gap
where data had only reached the UPF's own gateway.

## Results and first-request evidence

Evidence root: `/home/marujo/open5gs_containernet_dnn_results_20261108T073927Z`.
The passing campaign is `attempt-04`; final independent analysis is
`attempt-04/analysis-04`. Raw captures, earlier failed preparation/analysis
attempts, commands, image imports, effective mounts, versions and cleanup remain
available. Results are protected by the root SHA256 manifest.

| Status | Result |
|---|---|
| PASS | Separate complete core/database, bidirectional namespace isolation and collision checks |
| PASS | UP-originated Setup advertisements contain IE 233, configured pool IDs, DNN and IPv4 |
| PASS | A/internet, add B/dnn-lab without static peer mappings, add C/dnn-lab, C/B/C/B legacy rotation |
| PASS | Existing A/B owner/IP and GTP TEIDs continue serving data during addition, C loss and return |
| PASS | Real C deassociation; new session selects B while C is unavailable |
| PASS | Recreated C has a new Pod UID/IP/recovery timestamp, same pool ID, and rejoins C/B/C/B automatically |
| PASS | Returned C is selected after confirmed periodic Heartbeats |
| PASS | Twelve positive sessions, each first N4 request correctly targeted, Request Accepted and zero PFCP retransmissions |
| PASS | Fifty-seven bound-UE payload checks: three echoes each, UL/DL GTP plus plain N6 and DN request/reply correlation |
| PASS | SMF UID `1402386e-325b-45ae-ba9c-01028861b643`, PID 12, restart count 0 at all six checkpoints |
| PASS | TShark 4.2.2 plus independent raw TLV parsing; eight captures with zero kernel drops |
| PASS characterization | Provisioned no-match DNN uses legacy fallback, returns Cause 75 and NAS NETWORK_FAILURE; automatic NAS retry recorded separately |
| PASS | Owned probe/lab namespaces removed with UID checks; DN capture containers stopped/removed; captures flushed |
| PASS | Separate normal profile install: 11 NFs Ready, zero restarts, correct digests/versions/mounts and Multus N4 PFCP association |
| NOT RUN (optional) | Stage 4B and strict rejection/rotation |
| NOT RUN | IPv6, slice restrictions, roaming, handover, load/soak, other vendors, heartbeat-only loss injection and incompatible-pool N6 negative case |
| NOT RUN | Live updated TAC4/optional edge Helm campaign; those values/ownership paths passed static validation |

| Attempt | First SMF N4 frame | Destination | PFCP retries |
|---|---:|---|---:|
| A internet | 8 | A, 10.42.2.17 | 0 |
| B only | 30 | B, 10.42.3.5 | 0 |
| With C, four attempts | 88, 114, 144, 170 | C/B/C/B | 0 each |
| C unavailable | 222 | B | 0 |
| C returned, four attempts | 290, 329, 370, 399 | New C/B/new C/B | 0 each |
| After periodic Heartbeat | 455 | New C, 10.42.1.15 | 0 |
| No-match initial establishment | 518 | A through RR fallback | 2 |
| No-match automatic new NAS establishment | 539 | B through RR fallback | 2 |

The final row is a distinct NAS establishment with a different PFCP transaction
and CP SEID, not a retransmission of frame 518. It was interrupted by test UE
deregistration and is not counted as another successful session. The original
no-match attempt has NAS rejection; the subsequent automatic attempt and its
pending N4 retries are explicitly retained. The pre-existing error response
without Node ID still causes this negative-path limitation and was not changed.

`sessions.csv` and `sessions-first-n4.json` contain SUPI, PSI, DNN/SST, NAS-assigned
and SMF-allocated IPs, Node ID/N4 identity, first frame/time/SQN, CP/UP SEIDs,
TEIDs, Cause, retries and metadata source/reason. Rejected-session PSI is verified
against the SMF removal log containing the same SUPI/DNN/allocated IP. Every
positive selection is `source[learned] reason[dnn]`. `n6-correlation.json` links
each distinct payload to GTP, plain N6 and DN frame numbers. No success is inferred
from connectivity after a wrong initial selection.

The first successful returned-C use occurred 7.621 seconds after the accepted
association. This is an observed upper bound including fixture/UE sequencing,
not exact internal publication latency, Pod creation time or a recovery SLA.
No survival/migration of sessions on failed C is claimed.

## Failed fixture attempts and corrections

Failures were retained rather than replaced: optional missing CoreDNS ConfigMap
handling; chart resources lacking namespace when applied outside Helm; an
overly narrow expectation that isolation must time out instead of REJECT; and
an analyser glob that included an empty sidecar stdout log as capture statistics.
The namespace issue created owned ConfigMaps/HPAs in `default`; each was audited
and deleted using its label and UID before starting a fresh attempt. No existing
objects were adopted/deleted. All feature/NAS tests took place in attempt-04.

Image preloading also recorded failed `k3s ctr`/nested overlayfs/fuse unpack
variants. The supported standalone ctr import with `--no-unpack` succeeded on
all four nodes; CRI used the preconfigured fuse-overlayfs snapshotter. Original
registry references were then checked against actual running imageIDs. No
snapshotter, host firewall or security policy was changed to hide these failures.

## Reproduction, cleanup and final state

Run the fixture's documented prepare/provision/campaign/collect/analyze/cleanup
steps against a new result directory. A verified extracted chart can be supplied
with `--chart`; this run used the recorded 2.3.4 archive. Failed attempts require
fresh directories/namespaces; reanalysis uses a new output directory. Credentials,
packet captures and operator data stay outside Git.

Lab cleanup deleted only its owned namespace with a UID precondition, after
flushing/retrieving evidence. The lab MongoDB/subscribers were disposable;
normal persistent MongoDB was not used for these tests. The normal installation
afterward used the existing `deployments-containernet/open5gs/scripts/install.sh`
with `CHART` set to the verified local 2.3.4 chart. It created Multus, its N4 NADs,
MongoDB/WebUI and normal release `open5gs` revision 1. It passed the existing
verifier. Normal SMF/UPF N4 is `10.250.0.10` ↔ `10.250.0.20`. This separate
normal-profile check is startup/configuration/PFCP evidence; the full NAS/N6
dynamic matrix belongs to the isolated namespace above.

The emulation and normal core were intentionally left running for the user's
existing wrapper/deployment scripts. The lab namespace/probe namespace and all
owned DN sniffers are gone. Normal-profile teardown remains its existing
uninstaller with dependency removal opt-in; persistent MongoDB must be retained.
No global Docker/Kubernetes/network cleanup, physical deployment changes, commit,
image publication, Git push or PR was performed. Prompt 6 is complete for the
documented IPv4/SST 1 MVP scope. Stage 7 requires a separate instruction.
