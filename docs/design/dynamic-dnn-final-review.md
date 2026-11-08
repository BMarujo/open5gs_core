# Dynamic DNN MVP: final review and project closure

Date: 2026-11-08. The MVP is implemented and its scoped validation is complete.

Subsequent authorized publication and rollout are now complete; the historical
stage-7 image boundary below is retained. The [performance/publication follow-up](dynamic-dnn-performance.md)
records current source `384a54f4d9a`, the new public digest, five original/new
pairs (10 sessions / 40 measurements), identical QoS and passing NAS/data smoke
in Containernet and the physical cluster. Later commits changing documentation
or Dockerfile comments do not change this image's NF source identity.
UPF advertises configured pools/DNNs using IE 233, SMF owns the learned metadata
and its existing selector uses that metadata. Both association initiators,
multiple same-DNN UPFs and automatic return after loss are covered. DNN OR
location and legacy fallbacks are deliberate retained behavior. Stage 4B is
not implemented. The user withdrew the upstream PR deliverable; no PR draft,
history rewrite, commit, push or publication is part of this closure.

The final review found and fixed one remaining protocol compatibility defect:
unknown trailing octets of extendable IEs were rejected or discarded. It also
made packet capture work in private evidence directories. The corrected source
and local images pass the checks below. **The public Helm image and previous
cluster captures predate this last correction.** Publishing and rolling out
the final revision remain unperformed; no live cluster was changed in this review.

Start with the [user guide](dynamic-dnn.md),
[ADR](ADR-001-dynamic-dnn-learning.md) and
[clause → behavior → test → evidence matrix](dynamic-dnn-conformance-matrix.md).

Subsequent authorized local validation on the same day:
[IPv6 and existing-slice regressions](dynamic-dnn-ipv6-slices.md) passed, without
any core changes or Kubernetes access. It adds real IPv6-only/dual-stack NAS and
gateway GTP-U data with both initiators, plus existing slice tests with learning
off/on. The original review's IPv6 NOT RUN entries below retain their earlier
scope; the current remaining boundary is external IPv6 DN and slice-restricted
PFCP advertisement NAS validation, not all IPv6 NAS/data.

## Source identity and preservation

| Item | Reviewed identity |
|---|---|
| Comparison base | `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41` |
| Entry/final Git HEAD | `78f91ae6e5674503c766991e0d8cd10c515869b6` |
| Branch / declared software | `main` / Open5GS 2.8.0; no checkout/version change |
| Normative profile / wire header | TS 29.244 and TS 23.501 V19.5.0, Release 19 / PFCP version 1 |
| Entry core-input SHA-256 | `0d6a194893d68e363f0a1f2ead860eef75a93c6e075fa92b34b4272b4ce5f642` |
| Final core-input SHA-256 | `d8e528dfc3abe4c66856b8f75e364d8bac1758eebbff2b61ab9c52781bf5242c` |

The entry worktree already contained plan, runbook and cluster report edits,
plus the untracked version-alignment and Containernet reports. Those were
preserved. Existing deployment-repository work and running emulation were not
modified. The final source remains uncommitted on the same HEAD; the dirty
patch and source hash are therefore necessary identifiers. The original base
is 205 commits beyond the upstream v2.8.0 tag; tag, base and current HEAD are
not interchangeable.

Fresh evidence is in
`/home/marujo/open5gs_final_review_20261108T154719Z` (**F** below).
It records entry status/diff, final patch, current tracked/untracked inputs,
pinned vendor trees, commands, image inspections, logs, captures and exit codes.
Earlier result bundles were not rewritten. No operator credentials, large
captures or real subscriber data were added to Git.

## Review findings and corrections

**Extendable IE handling — fixed.** TS 29.244 §8.1.2's type table and individual
figures classify identity 177, IP Version 258, Node ID 60 and Recovery Timestamp
96 as extendable. The new validation had incorrectly required exact lengths.
An otherwise valid extended IP Version/identity could lose its optional pool;
an IP-form Node ID or timestamp with extensions could reject Setup before
learning. The corrected receiver consumes the complete supported prefix and
ignores unknown extension octets. Identity's internal length must fit the
available value and delimits the exact retained opaque ID. Timestamp's numeric
descriptor receives exactly its first four bytes. IPv4/IPv6 Node ID requires
at least a complete address. Fixed Cause and S-NSSAI keep exact lengths;
truncated prefixes and structural overruns remain errors.

Independent literal-byte tests failed against the pre-fix library
(`extension-before-fix.log`, expected FAIL), then passed with the correction.
The final cases include both Setup directions, extended IPv4/IPv6 Node ID,
timestamp, identity, family and owned learned fields; a five-byte fixed S-NSSAI
remains unusable. The pinned DOCX was used directly; a fresh PDF download
returned HTTP 403. TShark 4.2.2 independently decodes the same supported prefixes
and marks trailing octets as undecoded notes, without malformed-packet findings.
No generator change is needed for this correction.

**Capture harness permissions — fixed.** Dumpcap drops privileges and could
not create a capture underneath the private mounted evidence root. The first
integration rerun failed its evidence check for this reason. Association,
selection, NAS integration and loss-injection harnesses now open the PCAP in
the parent process and stream `tshark -w -` to that descriptor. Private directory
permissions can remain intact; the recovery wrapper no longer makes its output
world-writable. These are test harness changes, not NF behavior changes.

Other reviewed areas:

| Area | Finding and validation |
|---|---|
| Codec/generator/caches | Exact type-name aliasing keeps 233/177 distinct from 93. Group nesting, per-context multiplicity, fixed Create PDR pair and Usage Information singleton agree with generator/cache outputs. Fresh/cached regeneration agrees; all 28 caches equal checked-in bytes. Only generator provenance header lines are excluded from generated-file comparisons. |
| Ownership | Builder stack buffers are encoded synchronously. Filtered body belongs to `ogs_pfcp_message_t`; snapshots copy retained bytes. Replacement/destruction free owned snapshots; receive discard paths return pooled UPF/SGW events through their NF allocator. Directed ASan/UBSan and real-NF rejected-response injection pass. |
| Setup acceptance and errors | Raw validation precedes Node ID lookup/merge; optional capability usability is distinct from protocol acceptance. Cause, handler results and send/transaction failures gate initial state and snapshot publication; rejected replacement does not replace live metadata. Correlated erroneous replies terminate retries and notify the callback. |
| Reassociation/recovery | Local epochs reject obsolete transactions; post-Setup Heartbeat confirmation protects replacement metadata. Timeout/peer destruction invalidate data. Previously associated passive SMF/UPF retries Setup; CP recovery retain-all 183/184 handling is scoped and tested. Setup timestamps do not seed recovery history. |
| Selector and sessions | Static/learned remain separate; accepted ABSENT differs from invalidated NULL. Pool context, family and slices remain owned. Only the DNN arm changes. Six actual-selector cases and 6,144 baseline-policy comparisons retain OR/location, rotation and fallback. Allocator/call-site ordering and established ownership are unchanged. |
| Shared PFCP consumers | Complete build includes SMF, UPF, SGW-C and SGW-U. EPC attach/simple exercises SGW PFCP and Diameter; feature-off 5GC auth and different-DNN slice regressions pass. This is scoped regression coverage, not exhaustive malformed-Setup interoperability for SGWs. |

## Executed final tests

All final commands use isolated containers/network namespaces. No host routing,
firewall or live deployment was changed. Complete build uses the immutable
toolchain image
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`
(Ubuntu 24.04, GCC 13.3, Meson 1.3.2, Ninja 1.11.1, Python 3.12, TShark 4.2.2).
Pinned vendor revisions are freeDiameter
`14725af3ba0edbf9ff61c4e3239ed42464423b2e` and prometheus-client-c
`a58ba25bf87a9b1b7c6be4e6f4c62047d620f402`. Their sealed source trees were
staged outside the checkout and hashed. Build/test commands and dependencies
are retained in F; generation uses a separate `python-docx==1.2.0` environment.

| Check | Final result | Evidence relative to F |
|---|---|---|
| Fresh full build before final fix | PASS; 4,707 build steps, four selected suites | `build-tests.log`, `build-tests-exit.txt` |
| Corrected complete build and core/crypt/unit/smf-selection | PASS, 4/4; no skipped/failed suites | `final-build-tests.log`, `final-build-tests-exit.txt` |
| Nine codec, ten association, six selector ABTS cases | PASS, including malformed input, boundaries, repetitions, ownership and 6,144 policy comparisons | Meson logs; `ipv6-extension.log` includes final IPv6 Node ID subcases |
| ASan/UBSan codec, association and actual selector | PASS; leak detection enabled, no suppressions | `verified-sanitize.log`; `ipv6-extension.log` for expanded association vectors |
| Empty-cache / checked-in-cache generation | PASS; both generated bodies agree with checked-in output; 28 exact caches | `generator-results.json`, commands/logs/versions |
| Independent bytes / external decoder | PASS, scoped; fixture header reproduced, normal and extended byte PCAPs decoded | `fixtures/`, `extensions.pcap`, `extensions-dissection.txt`, `decoder-version.txt` |
| Real association/config/selector boundaries | PASS: 7 / 15 / 14 scenarios | `verified-association/results.json`, `verified-config/results.json`, `verified-selection/results.json` |
| UP-initiated NAS sessions and live B/C addition | PASS: first requests A/B/C/B/C, five accepted sessions, zero PFCP retransmissions | `verified-dynamic/sessions.json`, N4/N3 capture and logs |
| CP-initiated learned precedence over conflicting static mappings | PASS: A/B, two accepted sessions, zero retransmissions | `verified-static/sessions.json` |
| Ordinary restart/loss-return, both initiators | PASS: new first requests C/B/C/B, accepted sessions and data after periodic Heartbeats, same SMF process | `verified-up-recovery/`, `verified-cp/`; seven positives per run |
| Directional and simultaneous Heartbeat loss | PASS: real SMF/UPF, no NF restart; automatic Setup and relearning; 128 unmatched responses per NF per case | `verified-loss-smf/`, `verified-loss-upf/`, `verified-loss-both/` results/counters/captures |
| Brief Heartbeat loss | PASS: exactly one lost response, no deassociation | `verified-brief/results.json`, `fault-counter.json` |
| Feature-off existing 5GC/EPC | PASS: registration/auth-test, slice/different-dnn-test, attach/simple-test | `verified-5gc-regression/results.json`, `verified-epc-regression/results.json` |
| Final local image loaders/help/startup/IE 233 learning | PASS | `image-provenance/`, `verified-image-smoke/result.txt` and capture |
| No-DNN-match legacy behavior | PASS characterization; session failure remains a baseline protocol FAIL | Four NAS runs' separate no-match attempts: two PFCP retransmissions each, no NAS acceptance |
| Final correction deployed/published, new live cluster run | NOT RUN | Local-only review; existing public deployment remains on preceding source |
| Strict 4B tests T09/T10 | NOT RUN (optional) | Excluded by design |

The four NAS campaigns above contain **21 accepted sessions**, each independently
correlated with its initial PFCP request and zero retransmissions. Their four
separate no-match attempts are negative evidence, not counted as positives.
Data in these current local runs is GTP-U gateway echo with controlled return
routes; external N6/DN proof belongs to the earlier Containernet campaign below.
Recovery-pair timing is not an availability SLA: Setup packets may pass while
only Heartbeat Responses are filtered, so relearning can precede rule removal.

Intermediate failures remain available: missing execute permission when a shell
wrapper was first invoked directly (use `bash`); first integration capture lacked
directory access; expected new extension tests failed against the old library;
the first sanitizer attempt mixed a library built before the correction with
updated tests and was discarded. A dependency rebuild and all verified reruns
passed. Compiler/parser diagnostics from intentional negative vectors are not
additional failing tests.

## Images and historical cluster evidence

Current local images package the tested SMF/UPF binaries and ELF closure:

| Image | Docker image ID |
|---|---|
| `open5gs-smf:78f91ae6e567-dirty-d8e528dfc3ab` | `sha256:8516ee3414c06a13bf05400ad9ec95ad38c9b9e294f86842c01750fb129d3328` |
| `open5gs-upf:78f91ae6e567-dirty-d8e528dfc3ab` | `sha256:8c8d92cc83b72a8b941542ca7b5bf320bcf0c7a7ca8e718dd3fc71edabd93c17` |

F's `open5gs-dynamic-dnn-images.tar` SHA-256 is
`bf8fb40a23beb9f73d5be3d54397f1943160d778b70669bcd87950d7115fbca8`.
These are local runtime test images; they do not replace the all-NF Helm image's
init/probe tools. They were not pushed.

At the stage-7 review, the previously published Helm image was
`docker.io/bmarujo/open5gs:dnn-055eb4a770d0`, digest
`sha256:1e125d1f016fa9f3df6354588ebdaee2e281c637157f923003cf0570546d37fa`,
entry core hash `0d6a194893d6…`. Remaining core NFs were aligned to Gradiant
2.8.0, digest
`sha256:012d47b034b3ace4fd2fa41e861cd2453bead9a50b12bb60f31c2c1be2209bdb`.
One Open5GS image can contain all daemons; using it only for SMF/UPF was the
chosen deployment boundary. Gradiant chart 2.3.4 required no template fork.

Reviewed historical bundles and results, not relabelled as new executions:

| Report / bundle | Established result and remaining boundary |
|---|---|
| [Stage 1 audit](dynamic-dnn-audit.md), [experiments](dynamic-dnn-audit-experiments.md), [R19 review](dynamic-dnn-release19-review.md) | Baseline code path, identity contract and source clauses; initial missing talloc blocker resolved in stage 2. Early R17 references superseded by pinned R19. |
| [Codec](dynamic-dnn-codec.md), [association](dynamic-dnn-association.md), [selection](dynamic-dnn-selection.md) | Independent wire fixtures, owned learning, T24 real identity persistence, effective selector and optional/error distinctions. Then-open later-stage limits are historical. |
| [Stage 5](dynamic-dnn-integration.md), `/home/marujo/open5gs_stage5_results_20261011` | First N4 requests and GTP, baseline error and incompatible-CIDR characterization. Original returned-C invalidation FAIL is retained, subsequently fixed. |
| [Restart fix](dynamic-dnn-restart-fix.md), `/home/marujo/open5gs_dnn_restart_results_20261104` | Expected failing regression followed by C/B/C/B return and directed sanitizers. |
| [Asymmetric recovery](dynamic-dnn-pfcp-recovery.md), `/home/marujo/open5gs_pfcp_recovery_results_20261104` | Both initiators, one-sided loss, retention, pooled-event ownership fix; raw 96-byte talloc startup tracking leak reproduced independently and narrowly suppressed only in full-NF checks. Current directed checks use no suppression. |
| [Physical cluster](dynamic-dnn-cluster.md), `/home/marujo/open5gs_cluster_results_20261104T154429Z` | Public image, first-request dynamic selection, restart return without SMF restart in campaign; existing edge worker unavailable. |
| [Version alignment](dynamic-dnn-version-alignment.md), `/home/marujo/open5gs_version_audit_20261105T142126Z` | Nine other NFs at 2.8.0; six fresh sessions after ordered SBI refresh. SMF restarted for that upgrade, separately from the dynamic campaign. |
| [Containernet/K3s](dynamic-dnn-containernet.md), `/home/marujo/open5gs_containernet_dnn_results_20261108T073927Z` | `attempt-04/analysis-04`: 12 positives, zero N4 retransmissions, 57 payload checks through UL/DL GTP, N6 and three actual DNs; C new UID/IP, same ID, returns C/B/C/B; SMF UID/PID unchanged. Owned lab cleaned. Separate normal profile: 11 NFs Ready and Multus N4 association; full normal-profile NAS/N6 matrix not run. |

## Reproduction, logical change groups and rollback

The [runbook](dynamic-dnn-runbook.md) provides the fresh staging/build procedure,
pinned tool loading, isolated NAS/recovery commands, Helm context checks,
evidence collection and UID-scoped cleanup. F additionally stores the exact
commands used here in `*-command.json`, `generator-commands.json`,
`verified-integration-results.json`, `verified-up-recovery-results.json` and
`recovery-results.json`. Use fresh output names; do not overwrite sealed results.
Builds use `--network=none` and `--wrap-mode=nodownload` once sources/tools exist.
Generation needs its recorded Python environment; exact binary/image
reproducibility across unlocked package downloads is not claimed.

Internal logical change groups, without proposing new commits or upstream PRs:

1. **PFCP codec and generation:** type alias correction, nested/repeated
   descriptors, caches, owned decode adapter and independent fixtures. The final
   extendable-IE correction belongs here with raw Setup validation coverage.
2. **Setup protocol handling:** prevalidation, Cause, successful-send/result
   gating, matching transaction completion and the four NF callers. These are
   independently useful protocol corrections; they do not define DNN policy.
3. **Advertisement and owned capabilities:** independent configuration switches,
   explicit pool IDs, UP builders, deep-owned snapshots and invalidation.
4. **Legacy selector integration:** static/learned precedence and context-aware
   DNN arm, existing OR/fallback unchanged, allocator/session ownership retained.
5. **Recovery and ownership corrections:** post-Setup Heartbeat epoch guards,
   previously passive peer recovery, scoped retention 183/184 and NF pooled-event
   cleanup. These are separate from strict selection and from baseline defects.
6. **Validation/packaging/documentation:** isolated C/NF/NAS/EPC tests, image
   recipes, cluster fixtures and dated evidence, current capture FD handling,
   guide/ADR/matrix. Deployment values remain in the separate deployment repo.

Rollback is operationally scoped. Restore that deployment's recorded Helm
revision, image digests and full configuration together; preserve MongoDB and
subscriber data. Disabling learning requires restoring static mappings if they
are wanted, otherwise fallback remains. Startup switches require process
restart; rollback can lose sessions. No persistent learned database or migration
needs rollback. Do not use lab cleanup to remove the normal running core,
network definitions, volumes or unrelated Docker objects.

## Final implemented / tested / pending boundary

Implemented: scoped R19 codec, advertisement, owned learning, legacy selection,
loss/return handling, diagnostics and isolated reproducible validation fixtures.
Tested: the current-source checks above and the separately identified prior
IPv4/SST 1 physical/Containernet campaigns. No known introduced regression remains
open in the executed scope. This is a scoped engineering conclusion, not a claim
that no undiscovered bugs exist.

Operational delivery subsequently completed at the user's request: corrected
source committed/pushed, Helm-compatible image built/tested/published, deployment
pins updated, and exact-digest rollouts plus normal NAS/data smoke passed in both
clusters. The paired comparison also passed. See the follow-up for immutable
provenance, retained failures, unavailable-node workaround and the narrower
performance claim.

Remaining validation: external IPv6 DN and slice-restricted advertisement NAS/N6, other vendors,
load/soak, roaming/handover, exhaustive SGW malformed-Setup cases, cluster
heartbeat-only loss, normal-profile full NAS/N6, optional updated TAC4/edge
campaign and the unavailable physical worker. These are NOT RUN, with no implied
success. Baseline no-match Session Establishment error, transaction holding-window
SQN reuse and the independent talloc startup tracker remain documented and
outside the requested introduced-bug fixes.

Strict 4B, API/SUPI policy, orchestration, dynamic service/IPAM provisioning,
Association Update/hot reload and session migration remain excluded. No further
development stage starts automatically. The MVP project is closed within this
scope; final-correction publication/rollout is complete in the named follow-up.
