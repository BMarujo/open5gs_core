# Helm image publication and Kubernetes validation

Current project status: [final review](dynamic-dnn-final-review.md) and
[operation guide](dynamic-dnn.md). This dated report preserves the results and
limits at its execution date; later corrections/validation do not rewrite it.

Follow-up on 2026-11-05: the remaining nine core NFs were aligned to Gradiant
2.8.0 and revalidated. See [the version alignment report](dynamic-dnn-version-alignment.md).
The mixed-version state below records the original 2026-11-04 campaign.

2026-11-04. The user explicitly authorized committing/pushing the implementation,
publishing a public Docker Hub image and updating the existing SMF/UPF Helm
workloads. This supersedes earlier publication/deployment restrictions. Stage 4B
remains excluded. No PR was opened.

## Source, image and chart

The implementation was committed as `9c1ed5ec55a9ead21b80a29b636e2e791528e033`.
The final packaging correction is commit
`055eb4a770d0b5796e280e67598bc2f806a407cd`.
Both commits were pushed to `BMarujo/open5gs_core`, branch `main`.
The compiled core input SHA-256 remains
`0d6a194893d68e363f0a1f2ead860eef75a93c6e075fa92b34b4272b4ce5f642`.

Public image: `docker.io/bmarujo/open5gs:dnn-055eb4a770d0`.
Registry index digest:
`sha256:1e125d1f016fa9f3df6354588ebdaee2e281c637157f923003cf0570546d37fa`.
Anonymous registry manifest retrieval returned HTTP 200. Kubernetes main and init
containers' imageIDs were checked against that digest. The runtime reports
Open5GS 2.8.0; chart appVersion metadata is not used as binary evidence.

`docker/helm/Dockerfile` produces one image with all NF executables, compiling
inside a build stage, testing core/crypt/unit/selector suites, then installing the
binaries and shared libraries into a compatible Ubuntu 24.04 runtime. Bash,
iproute2, iptables and capture tools satisfy the existing UPF chart's init/probe
contract. The chart creates TUN/NAT; the image entrypoint only forwards commands
and signals. Build source and pinned vendors are recorded in the evidence.
Runtime base is pinned; resolved package versions are captured. The immutable
compiler image is the previously validated 7e59045bafdd image.

The installed Gradiant umbrella chart stays at **2.3.4**, using its recorded
cached archive, not the newer local chart clone. Only SMF/UPF images/configuration
were updated. Other NF images stay at 2.7.7. No custom chart modification was
needed: complete `customOpen5gsConfig` values carry learning, advertisement and
persistent pool IDs. Operator DNNs, pools, interfaces and placement were preserved.
Temporary validation pools were supplied separately and removed afterward.

## Live evidence and results

Evidence directory:
`/home/marujo/open5gs_cluster_results_20261104T154429Z`.
It includes pre/post Helm values/manifests/history, build attempts and exits,
public-registry verification, live imageIDs, effective YAML, timestamped command
records, unique synthetic subscriber ownership, captures, decoder version,
per-session evidence, cleanup and targeted rollback. Packet captures and operator
configuration stay outside Git. `SHA256SUMS` protects the final evidence files.

| Result | Executed evidence |
|---|---|
| PASS | Before rollout: synthetic UE registration, internet PDU accept and gateway echo on the original 2.7.7 core |
| PASS | Full image build and four selected Meson suites; runtime daemon versions/tools/library closure; public push and anonymous manifest retrieval |
| PASS | Seven Helm releases updated; desired image digest on eight SMF/UPF Deployments; actual running imageIDs verified on available workers |
| PASS | Main SMF plus three existing UPFs Ready on the new image; other NF images unchanged |
| PASS | Twelve positive dynamic-validation sessions: first PFCP requests independently decoded, expected UPF each time, zero transport retransmissions |
| PASS | UP-originated Setup Requests from B/C contain IE 233, configured identities, DNN and IPv4; independent TShark 4.2.2 PDML matches effective configuration |
| PASS | B serves the new test DNN without static peer mappings; adding C produces C/B/C/B rotation, while existing A/B data continues |
| PASS | C lost, SMF deassociation observed, next new session selects B; C returns with unchanged pool ID/new recovery timestamp and becomes selectable automatically |
| PASS | Four post-return first requests select C/B/C/B; NAS accepts and GTP-U replies persist across subsequent periodic Heartbeats |
| PASS | N4 CP/UP SEIDs and allocated UE IPs correlated with UL/DL TEIDs and captured GTP/ICMP on B/C; A continuity verified by bound-interface UE echo |
| PASS | Original SMF Pod UID/process unchanged throughout dynamic addition, loss/return and no-match tests |
| PASS characterization | No compatible DNN: legacy RR fallback selects a peer, two retransmissions occur, then NAS reject; this is not strict rejection-before-selection |
| PASS | Final normal operator configuration restored; a thirteenth positive session has first PFCP request to an appropriate learned internet peer, zero retries and data replies |
| PASS | Temporary UPF releases, test namespace and 16 uniquely tagged synthetic subscribers removed; temporary pools removed; final capture container terminated |
| NOT RUN (environment) | Edge-1 SMF and three UPFs cannot run on their pre-existing unavailable worker; desired image/config updated, actual runtime not validated there |
| NOT RUN (optional) | Strict stage 4B, T09/T10 |
| NOT RUN | Cluster ASan, learned IPv6/slice NAS, incompatible-pool negative routing, other vendors, arbitrary external N6 and cluster heartbeat-only loss injection |

A is an existing internet UPF. B/C are temporary chart releases serving a
pre-provisioned validation DNN with compatible pools. The SMF has no static
per-peer DNN/location mappings in this test. Each positive selection log states
`source[learned] reason[dnn]`; SMF service-selection TAI information is separate
from UPF location matching. Advertisement neither provisions nor authorizes DNNs.

| Attempt | First N4 frame | Selected peer | Retransmissions |
|---|---|---|---|
| Internet A | 305 | A | 0 |
| B only | 327 | B | 0 |
| After adding C | 387, 413, 443, 461 | C, B, C, B | 0 each |
| C unavailable | 841 | B | 0 |
| C returned | 882, 906, 930, 952 | C, B, C, B | 0 each |
| No DNN match | 1080 | C through RR fallback | 2 |
| Final operator-config smoke, separate capture | 1 | Existing internet peer | 0 |

The additional pre-loss C probe also passed without retransmission (frame 789).
The first post-return correct session request occurred 5.373 seconds after the
accepted Setup response. This is an observed first-use upper bound including test
sequencing, capture setup and UE creation; it is not Pod creation time, an exact
internal publication measurement or a recovery SLA.
IE 233 is observed in UP Setup Request frames 315, 377 and 858. C's configured
identity stays the same across restart while its recovery timestamp changes.
`sessions-first-n4.json` contains attempt IDs, IMSIs, PSI from UE logs,
requested DNN, UE IP, first frame/time/SQN/destination, response Cause, CP/UP SEIDs,
TEID sets, source/reason logs and matched GTP echo frames. The final smoke has its
own capture/result. No success is inferred merely from connectivity after retries.

## Failures encountered and resolution

1. First runtime assembly lacked libtins. The build-time loader check failed;
   the runtime dependency was added and a complete fresh build/tests passed.
   The failed image was not published.
2. First post-rollout UE registered but session forwarding still targeted the
   removed SMF's old Pod IP. The unchanged 2.7.7 SCP also reported exhausted
   `client_pool`. NRF had already removed the old registration. The failed UE
   attempt/logs are retained. SCP and AMF were restarted, keeping their images;
   NRF, MongoDB and the remaining control-plane NFs were not restarted. Fresh
   registration, sessions and data then passed. No unrelated SBI code was changed.
3. Initial in-UPF packet capture lacked permissions under the image's non-root
   user. Dedicated root ephemeral capture containers with network capabilities
   corrected the harness. They did not change daemon images or selection.
4. The no-match fallback still exposes the previously isolated baseline Session
   Establishment error without Node ID. Its NAS failure is recorded separately
   from passing compatible-session tests; no parser relaxation or strict policy.

Session continuity claims apply to unaffected peers during the dynamic tests,
not across the SMF rollout. No preservation/migration of sessions on a restarted
UPF is claimed. This campaign validates the named topology and mixed-version SBI
path, not every feature of an arbitrary 2.7.7/2.8.0 deployment.

## Final deployment and rollback

The central Helm release is revision 3; the edge release revisions and all
original revisions are in the evidence. Deployment repository values now pin the
public digest, enable independent controls and retain stable pool IDs. An explicit
values file was added for the already-existing `upf-edge-1c` release. Deployment
repository edits remain local; this task's Git pushes target `open5gs_core` only.

Four edge-1 workloads remain unavailable because their original worker is
NotReady and old Pods remain Terminating. This was present before the task;
node repair, force-deletion and hardware-placement changes were not attempted.
It is not an image startup failure. Do not describe the entire cluster as healthy.

`rollback.sh` in the evidence restores only these existing releases to their
recorded original Helm revisions. It does not clean networking or reset unrelated
NFs. Rollback restarts the affected workloads and loses sessions accordingly.
Test-only namespace deletion used ownership labels and a UID precondition; test
subscriber deletion required both exact IDs and the run's ownership tag.

The authorized build, public publication and live validation are complete for
the available workers. Remaining edge-1 runtime validation needs that worker or
an explicitly revised placement. At this campaign's date, final review had not
started. It is now closed in the [final report](dynamic-dnn-final-review.md);
the user subsequently withdrew the PR deliverable.
