# Correction: preserve newly learned DNNs across late Heartbeat restart detection

Current project status: [final review](dynamic-dnn-final-review.md) and
[operation guide](dynamic-dnn.md). This dated report preserves the results and
limits at its execution date; later corrections/validation do not rewrite it.

Subsequent authorized work adds recovery after asymmetric Heartbeat loss and
fixes pooled-event ownership; see the [follow-up report](dynamic-dnn-pfcp-recovery.md).
This report and its sealed image bundle describe the earlier correction only.

Date: 2026-11-04, Europe/Lisbon. Authorized scope: fix defects introduced by
the dynamic-DNN implementation; leave independent baseline defects alone.
HEAD remains `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, branch `main`, Open5GS
2.8.0. No checkout, commit, push, PR, image publication or deployment change.
The original stage 5 evidence bundle is preserved without modification.

## Problem and result

The feature added unconditional learned-pool invalidation when Heartbeat
detects a remote restart. That also erased a replacement snapshot already
received in a successful Setup from the returned UPF. In the passive SMF
path, the association remained live without another Setup, leaving the peer
without its learned DNN match. This was introduced by our metadata lifecycle
code; it is distinct from baseline PFCP session error responses or SQN caching.

**Fixed and tested:** C is lost, its old metadata is cleared, C returns with
the same N4 identity, and its replacement advertisement remains usable after
Heartbeat detects the restart. Four new edge1 sessions go first to **C, B, C,
B**, without retransmissions, in both UP- and CP-initiated association tests.
The original A and B sessions keep passing verified GTP-U echo traffic. C's
new sessions and metadata also survive the next periodic Heartbeat.

Choosing B once was never sufficient evidence of this bug: it could be a
normal rotation. The original failure additionally showed snapshot publication
followed by invalidation. The corrected test explicitly observes confirmation,
multiple consecutive first N4 destinations, retained data and later Heartbeat.

## Decision and implementation

Each owned snapshot starts with no Heartbeat confirmation. A reply to a
Heartbeat request sent **after that snapshot's accepted Setup** binds it to
the peer's Heartbeat Recovery Time Stamp. The existing association epoch fences
these probes, just as it already fenced outgoing Setup transactions. Capture
the epoch before transaction commit, which can free the transaction; failed
commit cannot confirm metadata.

A late restart indication does not erase a fresh, not-yet-confirmed replacement
Setup. Once confirmed, a higher Heartbeat Recovery Time Stamp invalidates the
old snapshot. Lower delayed timestamps cannot unlearn an already confirmed
newer snapshot. An unsolicited Heartbeat request cannot confirm a replacement;
a reply to a pre-Setup probe cannot confirm it either. Timeout, deassociation,
replacement and destruction still release snapshots. Rejected Setup does not
reset confirmation of existing data. `ABSENT` remains a separate accepted
snapshot rather than a NULL invalidated pointer.

Metadata is still published atomically at successful Setup, with the existing
selection semantics; confirmation is local lifetime bookkeeping, not a new
strict selection gate, deployment wait or wire capability. There is no extra
Setup exchange and no change to session restoration, allocation or selection.
The baseline `remote_recovery` history and `restoration_required` decisions
remain in their existing Heartbeat paths.

The pinned standard remains [TS 29.244 V19.5.0, Release 19](https://www.etsi.org/deliver/etsi_ts/129200_129299/129244/19.05.00_60/ts_129244v190500p.pdf).
The notes to tables 7.4.4.1-1 and 7.4.4.2-1 require ignoring the Setup Recovery
Timestamp. Therefore the fix never seeds Heartbeat history or derives a peer
boot from that Setup value. This rule was rechecked in the checked-in normative
`lib/pfcp/support/r19.5.0/29244-j50.docx`. A Firecrawl retrieval of the official
495-page PDF returned an irrelevant table-of-contents fragment with a partial
coverage warning; it was not used as evidence for the decision.

## Change map

| File | Correction |
|---|---|
| `lib/pfcp/association.[ch]` | Snapshot-specific Heartbeat confirmation and recovery value; lifecycle helper preserves replacement and invalidates confirmed old information |
| `lib/pfcp/handler.c` | Feed valid Heartbeat evidence into metadata handling; capture current-probe epoch before commit; retain baseline restart/restoration behavior |
| `lib/pfcp/xact.[ch]` | Capture association epoch for Heartbeat requests as well as Setup requests |
| `tests/unit/pfcp-association-test.c` | Reproducer and regression subcases for both initiators, request/response ordering, stale probes, delayed old requests, later restart and rejected replacement |
| `tests/pfcp/dynamic-dnn-integration.py` | Repeated real NAS sessions after restart, both initiators, stable confirmation and next-periodic-Heartbeat data checks |
| `tests/pfcp/dynamic-pools-smoke.py` | Confirm the current advertisement via a real probe before testing invalidation of old metadata |
| `tests/pfcp/run-dnn-integration.sh` | Document the additional `recovery-cp` test mode |
| Plan, ADR, stage reports and runbook | Superseding results and current image/source identities |

## Executed evidence

Durable directory: `/home/marujo/open5gs_dnn_restart_results_20261104`.
Scratch: `/tmp/open5gs-dnn-restart-20261104`. Every run uses a new Docker
`--network none` namespace, lab-only TUNs/routes and disposable MongoDB tmpfs.
No existing subscriber database, cluster or deployment is contacted.

The compiler/tools image is the retained immutable
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`:
Ubuntu 24.04, GCC 13.3.0, Meson 1.3.2, Ninja 1.11.1, Python 3.12 and TShark
4.2.2. MongoDB is pinned to
`mongo@sha256:8c3ce64d1a433bf57ea79035b48a38ea3e532997a1328fe3266e2e2e8bfb41b6`.
Pinned vendor sources are retained from the earlier source archive. The
original scratch build was gone; a fresh complete 4,707-step pre-fix build
passed before the reproduction, followed by incremental full builds of the fix.

| Check | Result / evidence |
|---|---|
| Fresh inherited build and core/crypt/unit/smf-selection | PASS; `build-before.log`, `tests-before.log` |
| New regression against pre-fix core | **FAIL expected**, reproduces a NULL snapshot after accepted Setup; `red2-test.log`, line 148 |
| Final full build and core/crypt/unit/smf-selection | PASS, 4/4; `build-final.log`, `tests-final.log`; association suite has eight cases with subcases |
| Directed ASan/UBSan with leak detection | PASS; codec and association lifetime tests, including the new regression; `asan-codec.log`, `asan-association-commit-checked.log` |
| Seven actual-association scenarios | PASS; `association-regression-final/results.json` |
| Fourteen selector-boundary scenarios | PASS; `selection-regression-final/results.json`; legacy policy retained |
| Real NAS UP-initiated loss/return | PASS; `recovery-up-final`: replacement generation 2, four first requests C/B/C/B, existing sessions and later Heartbeat data |
| Real NAS CP-initiated loss/return | PASS; `recovery-cp-final`: baseline active restoration negotiates again, replacement generation 3; C/B/C/B and retained data |
| Existing feature-off 5GC tests | PASS; registration/auth-test and slice/different-dnn-test in `existing-final/results.json` |
| Existing feature-off EPC test | PASS; attach/simple-test in `epc-final/results.json` |
| Independent external decoding | PASS; TShark 4.2.2 plus YAML-to-PDML verifier checks ID/DNN/family in 4 UP-initiated and 5 CP-initiated groups |
| New local image loader/help/startup/association | PASS; inspections, dependency manifest, archive and `image-smoke/result.txt` |
| Baseline Node ID/SQN defects | Excluded from correction; no parser weakening or generic duplicate-cache change |
| Cluster, complete runtime-image NAS, external N6, same-UPF old-session restoration | NOT RUN; local data proof is gateway echo, not deployment certification |
| Stage 4B strict selection | NOT RUN (optional); not implemented |

Synthetic SUPI is `imsi-9997000007487`, SST 1, IPv4. Neither run has peer
location rules or static DNN mappings. CP-initiated fixtures contain peer
addresses only. Seven accepted sessions have unique UE IPs. Each final run
also sends provisioned `nomatch`: the legacy fallback goes to C, receives
baseline Cause 75 responses without Node ID, retries twice and ends in NAS
rejection. This is retained negative characterization, not a repaired error path.

| Run | PSI after C returns | UE IP | First N4 destination | CP/UP SEID | UL/DL TEID | Retransmissions |
|---|---|---|---|---|---|---|
| UP initiated | 4 | 10.48.0.4 | C / 127.0.0.9 | 47 / 14 | 369 / 4 | 0 |
| UP initiated | 5 | 10.48.0.5 | B / 127.0.0.8 | 50 / 35 | 465 / 5 | 0 |
| UP initiated | 7 | 10.48.0.6 | C / 127.0.0.9 | 27 / 40 | 689 / 6 | 0 |
| UP initiated | 8 | 10.48.0.7 | B / 127.0.0.8 | 24 / 28 | 153 / 7 | 0 |
| CP initiated | 4 | 10.48.0.4 | C / 127.0.0.9 | 29 / 26 | 542 / 4 | 0 |
| CP initiated | 5 | 10.48.0.5 | B / 127.0.0.8 | 40 / 13 | 286 / 5 | 0 |
| CP initiated | 7 | 10.48.0.6 | C / 127.0.0.9 | 38 / 19 | 950 / 6 | 0 |
| CP initiated | 8 | 10.48.0.7 | B / 127.0.0.8 | 5 / 2 | 209 / 7 | 0 |

The first N4 destination comes from packet capture, not eventual connectivity.
`sessions.json`, `n4.json`, pcaps, GTP-U echo correlations, confirmation logs
and effective YAML are retained per run. A has nine verified echoes and the
original B session five in each run. Returned C's two sessions each have an
additional verified echo after the 11-second periodic-Heartbeat interval.
The SMF PID remains unchanged. C had no sessions before restart; this test
does not claim restoration of sessions that were hosted on a failed UPF.

Intermediate failures are retained: the first new unit harness omitted its
Heartbeat timer duration and aborted; the corrected pre-fix reproducer then
failed on the intended snapshot assertion. An incremental build initially
reused a test object because copied source mtimes predated that object's build;
restaging with current mtimes recompiled it and all suites passed. The first
CP recovery harness incorrectly waited specifically for generation 2; existing
active restoration produced valid generation 3, so it timed out. The fixture
now waits for actual post-return confirmation and checks subsequent stability.
No production behavior was changed to satisfy a fixed generation counter.

## Local images and reproduction

Core source identity:
`769ebcded9fe9eb40c5ff193b61c0b2fc58774f10c5728aa61d69faf8af15285`.

| Local image | Docker inspection ID |
|---|---|
| `open5gs-smf:2c80f8f2ba9d-dirty-769ebcded9fe` | `sha256:8db1cbd019d0d8c9ae4f4238c532664d2f3e87156754d54b505849f256bc644f` |
| `open5gs-upf:2c80f8f2ba9d-dirty-769ebcded9fe` | `sha256:3f31cda3a6669cb29bc5826cf93ee8adc84fafb9d6c01c6526f9ede4a90aa1d8` |

Saved image archive SHA-256:
`033a7b3fedc3c541899487c916690f8624a01e4cac32280e489bfd8f2bfbee38`.
These images include the correction. The previous `dirty-e62810f8908e` images
remain historical artifacts and do not include it. No image was published.

See the updated [runbook](dynamic-dnn-runbook.md) for fresh source extraction,
build and isolated execution. Main commands, inside the immutable tools
container where indicated, were:

```sh
meson test -C /artifacts/build core crypt unit smf-selection --print-errorlogs
meson setup /artifacts/sanitize-build --prefix=/artifacts/sanitize-install -Dbuildtype=debug -Db_sanitize=address,undefined -Dwerror=false --wrap-mode=nodownload
ninja -C /artifacts/sanitize-build -j4 lib/pfcp/libogspfcp.so.2.8.0
python3 tests/unit/pfcp-pool-sanitizer.py --association /artifacts/sanitize-build /artifacts/asan-association-commit-checked
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-dnn-restart-20261104 recovery-up-final recovery
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-dnn-restart-20261104 recovery-cp-final recovery-cp
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-dnn-restart-20261104 existing-final regression
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-dnn-restart-20261104 epc-final epc
bash docker/dynamic-dnn/build-images.sh . /tmp/open5gs-dnn-restart-20261104
bash tests/pfcp/image-smoke.sh /tmp/open5gs-dnn-restart-20261104 image-smoke
```

The bundle records command exit statuses, initial/final source identities,
the complete dirty patch, this correction's separate diff, build metadata,
source snapshot, decoder version, image identities and checksums. This fixes
the confirmed feature regression; it does not claim exhaustive correctness,
eliminate baseline defects, enable strict policy or authorize cluster execution.
