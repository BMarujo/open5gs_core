# Open5GS NF version alignment and live compatibility checks

Current project status: [final review](dynamic-dnn-final-review.md) and
[operation guide](dynamic-dnn.md). This dated report preserves the results and
limits at its execution date; later corrections/validation do not rewrite it.

2026-11-05. The user asked to check the existing 2.7.7/2.8.0 combination,
review Gradiant's current image and align the remaining core NFs where viable.
This report supplements the historical 2026-11-04 cluster report.

## Versions and scope

Before this task, AMF, AUSF, BSF, NRF, NSSF, PCF, SCP, UDM and UDR used
`docker.io/gradiant/open5gs:2.7.7`. SMF/UPF already used the published custom
dynamic-DNN image. A fresh mixed-version baseline registration, PDU session and
gateway echo passed. Earlier passing cases also established interoperability for
their tested paths; they were not a general mixed-version compatibility claim.

The latest stable upstream release and the newest Open5GS runtime tag listed by
Gradiant at inspection were **2.8.0**. The image pulled during this task is pinned
as `docker.io/gradiant/open5gs@sha256:012d47b034b3ace4fd2fa41e861cd2453bead9a50b12bb60f31c2c1be2209bdb`.
All nine runtime executables reported `Open5GS v2.8.0`; actual Kubernetes imageIDs
were checked against this digest. The current registry response, rather than an
older web-indexed digest, defines the deployed artifact.

SMF/UPF retain `docker.io/bmarujo/open5gs:dnn-055eb4a770d0`, digest
`sha256:1e125d1f016fa9f3df6354588ebdaee2e281c637157f923003cf0570546d37fa`.
Replacing these with unmodified Gradiant images would remove the feature.
The custom source baseline `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41` declares
2.8.0 but is **205 commits after** upstream tag
`v2.8.0` (`157f611a530e292e40ec50f9d23f0ef5d4fcd6a6`). Equal version strings
therefore do not mean identical source snapshots. No source rebase, version
switch, core behavior change, image build, publication, commit or push occurred
during this task.

The installed umbrella chart remains **2.3.4**. Gradiant's current source chart
advertises 2.8.0, but updating chart templates/dependencies was unnecessary for
this image alignment. The saved 2.3.4 chart rendered successfully, and a semantic
comparison against the previous Helm manifest proved that only nine Deployment
container image references changed. No ConfigMap, Service, placement, pool,
slice, port or SMF/UPF image changed in that Helm revision.

MongoDB remains 4.4 and WebUI remains its existing separate 2.7.5 image. Their
Pod UIDs and all three available operator UPF Pod UIDs were preserved. WebUI
versioning is independent of the C NF image; it was not part of this rollout.

## Source review

The upstream v2.7.7-to-v2.8.0 comparison includes Release 19 NAS, GTP and PFCP
definitions, regenerated SBI models, NRF discovery slice authorization,
validation/robustness fixes and optional PCF AF QoS reference profiles.
The reviewed DB changes do not introduce a required subscriber schema migration
for this lab's subscription/session fields. Actual tests used the unchanged
cluster MongoDB and synthetic subscribers with its existing schema.

Among the nine deployed NF configuration templates, only PCF changed between
those tags: the added QoS profile mapping is optional and this lab does not use
AF-supplied qosReference. Existing PLMN 001/01, SST 1, TACs, DNNs, DB URI and SBI
endpoints were retained. The stricter NRF checks are relevant to malformed
profiles and slice-restricted discovery; they are not a reason to disable
validation. Discovery and SST 1 selection were checked live.

Containers exchange network protocols and do not share in-process library ABIs.
The image's library closure, runtime user, entrypoint, configuration paths and
chart probe tools were checked. Compatibility of the exercised SBI/NAS/PFCP paths
is established by the final integration results below, not by version labels.

Primary sources:

- [Open5GS v2.8.0 release](https://github.com/open5gs/open5gs/releases/tag/v2.8.0).
- [Upstream changes](https://github.com/open5gs/open5gs/compare/v2.7.7...v2.8.0).
- [Gradiant image tags](https://hub.docker.com/r/gradiant/open5gs/tags).
- [Gradiant image recipe](https://github.com/Gradiant/5g-images/blob/main/images/open5gs/Dockerfile)
  and [entrypoint](https://github.com/Gradiant/5g-images/blob/main/images/open5gs/entrypoint.sh).
- [Gradiant umbrella chart](https://github.com/Gradiant/5g-charts/blob/main/charts/open5gs/Chart.yaml).

## Execution, failures and operational correction

Evidence: `/home/marujo/open5gs_version_audit_20261105T142126Z`.
It contains the original Helm values/manifest/history, effective configurations,
source comparisons, image identity/history, isolated startup logs, command logs,
failed and passing UE logs, N4 captures, independent decoder results, final
verification, ownership-scoped cleanup and rollback instructions.

Nine daemons started using unchanged effective YAML in an isolated internal
Docker network with a disposable MongoDB 7.0.26; no host ports or cluster DB
connections were used by this configuration smoke test. This is a startup check,
not the subsequent integration test against the real MongoDB 4.4 deployment.
The temporary containers/network were removed with ownership checks.

Helm upgraded the existing `open5gs` release from revision 3 to **4**, using only
the image changes. All nine NFs became Ready, but initial fresh UE registration
**failed**: SCP attempted the removed AUSF IP `10.244.0.191`. Refreshing SCP/AMF
alone was insufficient: a second fresh identity **failed**, with AUSF attempting
the removed UDM IP `10.244.0.192`. Both failed before PDU session establishment.
Logs and identities are retained; neither attempt counts as a passing session.

The evidence demonstrates stale peer discovery during simultaneous NRF/provider
replacement, including after switching to 2.8.0. It does not establish a protocol
version incompatibility, nor prove an endpoint-cache bug fixed in upstream 2.8.0.
With NRF left stable, clients were renewed in order:
**UDM → AUSF → PCF → SMF → SCP → AMF**, waiting for each Deployment rollout.
Fresh tests then passed. This was an operational refresh; no SBI code was changed.
SMF was deliberately restarted for this reason. No preservation of sessions
across AMF/SMF upgrades is claimed. Operator UPF processes were not restarted.

An initial offline decoder attempt could not read the private evidence directory
under the image's non-root UID. Running only the isolated decoder as root fixed
the harness. An initial UDR discovery probe used requester type AMF and returned
an empty result; UDR's permitted requester UDM returned the registered profile.
These harness issues and raw outputs are recorded separately.

## Results

| Status | Evidence |
|---|---|
| PASS | Fresh mixed-version baseline: registration, PDU accept and bound-interface gateway echo |
| PASS | Nine unchanged-config isolated startups, executable versions, libraries and chart runtime tools |
| PASS | Semantic Helm comparison: exactly nine image changes; release revision 4 deployed |
| FAIL, resolved operationally | Two initial post-rollout registrations targeted removed SBI endpoints; scoped ordered refresh above |
| PASS | Six distinct final UEs: registration, PDU accept and three gateway echo replies per UE |
| PASS | Independent N4 decoding: zero retransmissions, Request Accepted, learned/DNN selection, expected UE addresses and UPF destinations |
| PASS | Final first PFCP destinations: `.40, .20, .30, .40, .20, .30` on `10.250.4.0/24`; each existing UPF serves two sessions |
| PASS | NRF discovers registered AMF, SMF, AUSF, UDM, UDR, PCF, BSF and NSSF using suitable requester NF types; one profile each |
| PASS | NSSF SST 1 network-slice selection returns HTTP 200 and the configured NRF/NSI |
| PASS | Nine final NF imageIDs and readiness; custom SMF/UPF digests; MongoDB/WebUI/available UPF Pod UIDs preserved |
| PASS | All test UEs released; nine exact run-tagged subscribers removed; owned namespace deleted with UID precondition; capture stopped |
| NOT RUN | Roaming, handover, AF QoS, IPv6, arbitrary external N6, load/soak, loss injection and complete regressions across all NFs |
| NOT RUN (environment) | Existing edge-1 SMF/three UPFs: their original worker remains unavailable |
| NOT RUN (optional) | Stage 4B; no strict policy or new orchestration |

The final capture's first requests are frames **49, 67, 85, 95, 109, 119**.
TShark 4.2.2 plus an independent raw PFCP TLV parser verifies destinations,
transaction/SEID correlation, response Cause and retry counts. SMF logs identify
`source[learned] reason[dnn]` per attempt. Passing connectivity after retries
was not substituted for first-request evidence. Data verification here is
bound-interface ICMP to the UE subnet gateway; this task did not add a separate
GTP capture or certify arbitrary Internet connectivity.

## Reproduction and rollback

Permanent local values are in
`open5gs_k8s/deployments/open5gs/values/values-lab.yaml`, with explicit registry,
repository, version tag and immutable digest on each of the nine images.
The exact executed values, chart identity and commands are in the evidence.
The comparison with local values differs only by explicit `IfNotPresent` on the
two custom images; this matches the chart defaults and rendered images.

The principal deployment command was:

```sh
helm upgrade open5gs \
  /home/marujo/open5gs_cluster_results_20261104T154429Z/chart/open5gs \
  -n open5gs \
  -f /home/marujo/open5gs_version_audit_20261105T142126Z/helm-values-upgrade.yaml \
  --wait --timeout 240s
```

For this rollout, the ordered refresh used `kubectl rollout restart` followed by
`kubectl rollout status --timeout=150s` for each named Deployment. Do not present
the additional restart as a core feature fix or as a session-preserving upgrade.

Targeted image rollback is `helm rollback open5gs 3 -n open5gs --wait --timeout 240s`.
It returns these nine images to their recorded 2.7.7 configuration while retaining
the previously deployed custom SMF/UPF. Endpoint discovery must be verified again
after rollback; a reverse image change can require the same scoped cache refresh.
Rollback was prepared, not executed. Do not force-delete unavailable edge Pods,
alter placement or prune networking as part of this rollback.

Deployment repository edits remain local; existing work was preserved. No new
image publication, Git push or PR was performed in this task.
