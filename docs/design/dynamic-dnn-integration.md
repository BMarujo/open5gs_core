# Stage 5: local integration, images and cluster preparation

Current project status: [final review](dynamic-dnn-final-review.md) and
[operation guide](dynamic-dnn.md). This dated report preserves the results and
limits at its execution date; later corrections/validation do not rewrite it.

Historical stage 5 report. **2026-11-04 follow-up:** the introduced ordinary
peer-return metadata regression was subsequently fixed and tested with repeated
real NAS sessions, both initiators and later Heartbeat data. See the
[correction report](dynamic-dnn-restart-fix.md). The FAIL and image identities
below describe the original stage 5 delivery; its evidence is unchanged.

Date: 2026-10-11–12 (Europe/Lisbon). Scope: the stage 4A MVP only.
Baseline: `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, branch `main`, Open5GS
`2.8.0`, no HEAD tag. No checkout, commit, push, PR, image publication or
existing deployment change was performed. All inherited changes were retained.
Stage 5 adds test/build infrastructure and documentation, with no production
core behavior changes. Stage 4B and stage 6 were not executed.

The protocol reference remains TS 23.501 and TS 29.244 **V19.5.0, Release 19**,
as pinned in [ADR-001](ADR-001-dynamic-dnn-learning.md) and the
[clause review](dynamic-dnn-release19-review.md). These results validate a scoped
implementation, not the complete release or 3GPP certification.

## Outcome and evidence boundary

**PASS:** real synthetic UE registration and PDU sessions through NGAP/NAS,
AMF, NRF, AUSF, UDM, UDR, PCF, NSSF, BSF, the actual SMF and actual UPFs.
UP-originated Setup learns A/internet and B/edge1 without any static SMF peer
entry. C/edge1 is added while A has an established session. The first N4
Session Establishment Requests go to A, B, C, B, C respectively, with no
retransmission in those five successful attempts and no SMF restart.
Verified GTP-U ICMP echo replies continue on A after adding B and C.

**PASS, characterized limitations:** no advertised DNN match still invokes the
legacy fallback. Same-DNN, incompatible CIDRs are not detected by IE 233 or the
selector. A session can even be accepted with an out-of-subnet CP-allocated IP;
the negative routing test demonstrates failure of the data path.

**FAIL:** ordinary UPF restart does not guarantee immediate DNN eligibility
after Setup. A subsequent Heartbeat detects the new recovery timestamp and
invalidates the newly learned snapshot. In the real NAS recovery run, the
first new edge1 request goes to B, although the test expected returned C.
This reproduces the explicitly documented stage 3 lifetime limitation; no
automatic renegotiation or restoration mechanism was added to conceal it.

**FAIL, baseline defect:** UPF Session Establishment error responses omit Node
ID. SMF rejects them, retransmits twice and eventually returns a NAS session
reject after its N4 timeout. An independent minimal wire reproducer returns
the same defective response with unmodified HEAD and with this feature.
The parser was not relaxed and the independent protocol fix was not mixed in.

The data proof is ICMP to the selected UPF's lab TUN gateway, not Internet
access, external N6 routing/NAT, deployment readiness, or survival of sessions
on a restarted UPF. Learned IPv6 selection and slice restrictions are covered
at the codec/association/selector boundary; the new 5GC UE scenarios use IPv4
and one unrestricted SST 1. The unchanged EPC regression also exercises its
IPv4/IPv6 path with the feature off.

## Environment and provenance

Durable evidence directory:
`/home/marujo/open5gs_stage5_results_20261011`.
Scratch execution directory: `/tmp/open5gs-stage5`.
Evidence contains pcaps, effective/generated YAML, binary debug logs, attempt
tables, image inspection, commands, source manifests, patch and untracked
inputs. Only public synthetic subscriber test vectors were used.

Docker client/server: 29.8.2, linux/amd64. The compiler image is the existing
immutable local `open5gs-pfcp-codec-build:20261007`, ID
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`.
It supplies Ubuntu 24.04, GCC 13.3.0, Meson 1.3.2, Ninja 1.11.1, Python 3.12
and TShark 4.2.2. Exact packages/architectures are in
`runtime-context/build-packages.tsv`. Dependencies were checked against this
commit's `docs/_docs/guide/02-building-open5gs-from-sources.md` and Meson files.
No unversioned Open5GS distribution package was substituted for the source.

MongoDB: `mongo:7.0.26`, pulled and used as
`mongo@sha256:8c3ce64d1a433bf57ea79035b48a38ea3e532997a1328fe3266e2e2e8bfb41b6`.
Each UE test creates a fresh MongoDB tmpfs and a disposable Docker network
namespace (`--network none`). Only that namespace receives TUNs and routes;
no host networking, published ports or production database are used.

Pinned vendored subprojects:

| Subproject | Commit |
|---|---|
| freeDiameter | `14725af3ba0edbf9ff61c4e3239ed42464423b2e` |
| prometheus-client-c | `a58ba25bf87a9b1b7c6be4e6f4c62047d620f402` |

A clean external staging/build was also performed, rather than depending on
the initial copied Meson cache. All 4,707 Ninja steps and the four selected
Meson suites pass. Both NF executables and all 26 built shared libraries in
the image payload are byte-identical between the initial tested build and
the fresh build (`fresh-payload-match.json`). System library hashes are also
recorded in `runtime-context/payload.json`.

Core input identity:
`e62810f8908e18cf8bc3cca4f5598c9b33ea8408c37eb6497b785059fba6956b`.
This hashes source/configuration/build inputs; tests and documentation have
their own complete manifest and do not change the identity of the NFs.
Image-time and final working-tree manifests are retained separately.

## Attempt-level integration evidence

`integration-08` is the successful main run. Every row is one NAS attempt from
synthetic SUPI `imsi-9997000007487`, SST 1. No peer has any TAC/cell selection
list. The SMF has no `pfcp.client.upf` list. Thus neither static DNN mappings
nor location matches explain the positive results.

| Attempt / PSI | DNN | UE IPv4 | First N4 destination | CP / UP SEID | UL / DL TEID | N4 requests | Result |
|---|---|---|---|---|---|---|---|
| 1 / 1 | internet | 10.45.0.2 | A, 127.0.0.7 | 52 / 11 | 389 / 1 | 1 | PASS; 3 echo replies, including after B/C addition |
| 2 / 2 | edge1 | 10.48.0.2 | B, 127.0.0.8 | 61 / 15 | 352 / 2 | 1 | PASS |
| 3 / 3 | edge1 | 10.48.0.3 | C, 127.0.0.9 | 17 / 24 | 358 / 3 | 1 | PASS |
| 4 / 4 | edge1 | 10.48.0.4 | B, 127.0.0.8 | 12 / 24 | 523 / 4 | 1 | PASS |
| 5 / 5 | edge1 | 10.48.0.5 | C, 127.0.0.9 | 50 / 26 | 372 / 5 | 1 | PASS |
| 6 / 6 | nomatch | allocated 10.50.0.2; no NAS accept | A, 127.0.0.7 | 10 / absent | absent | 3, same transaction | Legacy RR fallback; three Cause 75 responses; NAS reject after timeout |

SEIDs are scoped to their PFCP endpoint, so identical values on B and C are
not a collision. `sessions.json` identifies timestamps, sequence numbers,
frames, requested DNN/PSI, selected metadata source/reason, SEIDs and TEIDs.
The decoder reads the captured UDP payload's PFCP header and top-level IEs
independently of the Open5GS encoder. `n4.tsv`, `n4.json`, `gtpu-icmp.tsv` and
TShark's full dissection permit review of every destination and response.

For each ping, the C driver verifies GPDU, DL TEID, inner IPv4 source/destination
and ICMP echo reply. The external TShark check also matches request/reply ICMP
identifier/sequence and outer UPF address. It checks that no PFCP deletion for
that session occurred before its last verified ping. In the single shared
local namespace, explicit per-UE /32 return routes distinguish B and C's
compatible overlapping pools. These routes are test provisioning, not feature
behavior; `return-routes.jsonl` records them.

`integration-11` verifies CP-initiated Setup Response learning. A and B start
before the SMF, without UPF `client.smf`; the SMF's static DNN values are
deliberately reversed (A=edge1, B=internet). Learned mappings take precedence:
first requests still go to A/internet and B/edge1, with real NAS accepts and
three echo replies. Its unmatched attempt falls back to A and exposes the
same baseline response defect. C is not part of this secondary run.

`integration-10` gives C an edge1 pool of **10.49.0.0/24**, while the SMF still
allocates edge1 from 10.48.0.0/24 and B serves that compatible pool. The first
request for PSI 3 goes to C; Cause 1 and NAS accept allocate 10.48.0.3. Without
an artificial /32 override, normal connected return routing chooses B for
that address. The capture proves the uplink ICMP reached C, with no echo
reply during the bounded 1.5-second probe. A subsequent new edge1 session
goes to B and passes data. IE 233 does not carry CIDRs, and this result does
not establish an implemented compatibility detector or strict rejection.

`integration-12` preserves a real failing recovery expectation. C initially
associates but has no UE sessions. After C is stopped and the SMF reports
de-association, PSI 3 goes to live B and passes data; A also passes data.
Returned C publishes generation 2, then its first Heartbeat invalidates it
because the recovery timestamp changed. PSI 4 consequently goes first to B,
not expected C. No retry or eventual connectivity is counted as C success.
The boundary regression's explicit subsequent Setup case still passes; that
does not replace this failing ordinary-restart test.

## Independent baseline error reproducer

`tests/pfcp/session-error-reproducer.py` independently constructs a valid CP
association and a Session Establishment Request missing mandatory CP F-SEID.
The UPF sends type 51, Cause 66, without Node ID:

```text
213300110000000000000000000002000013000142
```

That byte string is identical in `session-error-modified-02` and
`baseline-03/session-error-baseline`, the latter built from clean real HEAD.
The normal no-match session also exposes the same error-builder path with
Cause 75. The generic Session Establishment response branch in
`lib/pfcp/path.c:ogs_pfcp_send_error_message()` sets Cause but not Node ID;
SMF's existing extraction path rejects it. The stage 3 Setup-specific error
response correction does not fix this separate session-message branch.
The reproducer exits successfully when the known defect is observed; that
is a **PASS characterization of a FAIL protocol behavior**, not a protocol
conformance pass. A separate protocol correction remains required.

## Test matrix and classifications

| Matrix | Result and exact scope |
|---|---|
| T01 | PASS: prior original baseline retained; clean HEAD UPF rebuilt for error comparison; fresh modified full build and core/crypt/unit/smf-selection pass |
| T02–T04 | PASS: codec ABTS and independent fixtures retained; stage 5 unit and ASan/UBSan reruns, including nesting/repetitions/limits/malformed input |
| T05–T06 | PASS: actual UP/CP initiated associations, plus real NAS sessions in integration-08/11 |
| T07–T08 | PASS: first-request A/B split and B/C rotation; established sessions and GTP-U evidence |
| T09–T10 | **NOT RUN (optional)**: stage 4B not authorized or implemented |
| T11 | PASS: replacement/absence/invalid/degraded/rejected reassociation in actual SMF association and selector-boundary regressions; directed sanitizer lifetime tests |
| T12 | PASS: heartbeat loss skips C with live B; explicit subsequent Setup restores metadata in boundary test. **FAIL:** immediate ordinary-restart eligibility in integration-12 |
| T13 | PASS: feature-off selected existing 5GC/EPC tests; learned/static conflict in integration-11 and selector-boundary regression |
| T14 | PASS at capability boundary: unknown advertised DNN does not provision allocator/subscriptions/NRF. NOT RUN: new NAS attempt for unprovisioned DNN |
| T15 | PASS: established A session retains its owner/IP and passes GTP-U after B and C additions; no early deletion |
| T16 | PASS: V4/V6 wire and family/slice projection at codec/association/selector boundary. NOT RUN: learned IPv6 or slice-restricted 5GC NAS/data scenarios |
| T17 | PASS: full build includes shared SGW-C/SGW-U paths; existing registration/auth-test, slice/different-dnn-test, attach/simple-test; core/crypt/unit/smf-selection |
| T18–T19 | PASS in association regression: handler/send/Cause results, invalid optional information, absent/static semantics and replacement paths; no invalid partial snapshot publication |
| T20 | PASS: real UPF config test skips generic subnet without advertising wildcard |
| T21 | PASS: repeated pool/context preservation in codec and actual association tests; no flattening into fabricated wildcard |
| T22 | PASS characterization: unique CP addresses, compatible B/C return routes and actual data; incompatible CIDR case is accepted but fails ordinary return path |
| T23 | NOT RUN: editing configuration of a live process. No Association Update/hot reload was implemented or claimed; changed advertisement is tested after actual restart/Setup |
| T24 | PASS: actual UPF restart/reorder/N4 change preserves explicit pool_id bytes; explicit identity edit replaces bytes |
| T25 | PASS for scoped Setup error taxonomy, matched erroneous-response transaction termination and notification; **FAIL outside that correction:** baseline Session Establishment error Node ID omission, independently reproduced |
| T26 | PASS characterization: location OR DNN, no match, no-RR/all-unavailable fallbacks in selector-boundary regression; real no-match NAS/N4 outcome recorded separately |
| T27 | PASS for implemented R19 codec/Setup profile in unit/association tests. NOT RUN for the broader R19 conditions already excluded in the clause review; no global compliance claim |

Additional results: 7 actual-association regression scenarios, 15 configuration
cases, 14 selector-boundary scenarios and directed ASan/UBSan codec,
association/reassociation/cleanup and selector tests pass. The new real NAS
stack was not run entirely under sanitizers. TShark 4.2.2 is the external
decoder; unsupported future/R19 fields retain the limitations from stage 2.
An additional PDML check compares decoded ID/DNN/family directly with the
effective YAML: 12 captured association groups (V4/V6, Request/Response) and
all three dynamic A/B/C groups pass. It imposes no retransmission-dependent
packet count. The original stage 3 verifier remains unchanged.
Kubernetes API/admission, CNI enforcement and cluster session tests are
**NOT RUN**; only generated fixture structure and init rendering were verified.
Offline rollback simulation verifies label/UID mismatch refusal and the delete
UID precondition with a fake kubectl; it is not a Kubernetes API test.

Intermediate failed attempts are preserved, not replaced by final results:

| Attempt | Failure/classification and resolution |
|---|---|
| Initial copied build | Broken unused symlinks while copying a Meson cache; fresh external full build later passes and reproduces the packaged payload |
| integration-01 | Test reader decoded binary NAS debug text as strict UTF-8; reader fixed, binary logs preserved |
| integration-02/03 | Lab TUN down/unaddressed; one read even contained an ICMP error. Explicit lab network setup and echo-content assertions added; these runs are not data proof |
| integration-04 | New test driver double-freed BSON consumed by test_db_insert_ue; fixed in test only |
| integration-05/06/07 | PASS precursors; integration-08/11 add complete independent packet correlation |
| integration-09 | Test incorrectly expected CIDR mismatch to cause session rejection. Actual acceptance preserved; integration-10 validates the resulting data-path failure instead |
| integration-12 | Genuine inherited stage 3 recovery limitation; remains FAIL, with actual first request to B |
| First minimal error repro | Independent peer failed to handle UPF Heartbeat before type 51; fixture now answers it |
| baseline/baseline-02 staging | Vendor repositories had container UID ownership; fixed with per-command safe.directory for the explicitly supplied vendor paths; baseline-03 succeeds |
| First config regression | One-subnet input supplied to a two-subnet fixture; correct association fixture yields 15 PASS |
| First existing regression | Wrong generated sample filename; corrected to sample.yaml; selected 5GC tests pass |
| existing-epc | Timeout without Linux TUN address provisioning; existing-epc-02 passes with isolated IPv4/IPv6 TUN setup |
| image-smoke | Actual NF startup/learning passed; dumpcap could not write the host-owned evidence directory. image-smoke-02 fixes lab directory permissions/capabilities and captures successfully |
| Additional external-decoder check | Original stage 3 helper expected at least 14 groups; this run captured 12 valid groups. A new YAML-based verifier checks all configured pools without assuming retransmission count. Its first invocation also included separate raw-hex identity-test YAMLs outside this pcap; restricting association input to the actual captured UPF fixture passes. Dynamic A/B/C YAML-to-wire verification passes |

## Built image identities

Both local images contain the tested SMF/UPF ELF payload and its exact library
closure, packaged `FROM scratch`. No apt resolution, remote download or core
rebuild occurs in packaging. Both binaries are included in the shared layer;
the role-specific entrypoint selects the daemon. OCI labels include real
HEAD, dirty state, core checksum, toolchain ID and scoped protocol profile.

| Image | Docker inspection ID / local OCI index digest |
|---|---|
| `open5gs-smf:2c80f8f2ba9d-dirty-e62810f8908e` | `sha256:0bea521d056af6fb9887602443f6459b1b936676de2951f484c35a8de0178477` |
| `open5gs-upf:2c80f8f2ba9d-dirty-e62810f8908e` | `sha256:c7402db727b725831e408c6b43de01789d60cba826c24b10f1e444a6bbcbcd68` |

Image config digests in the saved archive are respectively
`7e0638883e2ac3d409525a3d1e30c8b88a24f0714f2683411d19e93a4dbf3196` and
`4d0722c59c9fb63a113178d9cabb2e635574fb735ba89464186ee86797e925f9`.
The inspection reports local RepoDigests under Docker 29's image store; this
does **not** mean these images were pushed or exist in a remote registry.

`open5gs-dynamic-dnn-images.tar` (about 25 MiB) has SHA-256
`85ff47e3e8cc7de0f5630994159c74493ba38aa329355007c526b04bc13b5942`.
The compiler/tools image is also exported locally as
`open5gs-dnn-toolchain-image.tar` (about 295 MiB); its hash is in the bundle.
No registry publication occurred.

The runtime loader resolves every packaged dependency; both image entrypoints
pass `-h`. `image-smoke-02` starts the actual containers, observes successful
UPF-initiated Setup and owned edge1 learning in the SMF, captures PFCP, and
records daemon exit status on termination. Full NAS scenarios use build
binaries byte-identical to the image payload, not the runtime image containers.
No image-backed full NAS test is claimed.

Images target linux/amd64 and this pure 5GC configuration. Optional dynamically
loaded Diameter extensions/configuration are not part of this runtime closure;
EPC regression uses the complete build tree. Other architectures, TLS/client
certificate deployment, external N6 and cluster device/admission behavior
remain unverified.

## Reproduction and change map

The [runbook](dynamic-dnn-runbook.md) contains full local/cluster commands and
rollback. Relevant executable commands used here include:

```sh
python3 docker/dynamic-dnn/prepare-build.py . /tmp/open5gs-stage4a/source /tmp/open5gs-stage5/fresh
docker run --rm --network none -v /tmp/open5gs-stage5/fresh:/artifacts \
  -w /artifacts/source open5gs-pfcp-codec-build:20261007 sh -c \
  'meson setup /artifacts/build --prefix=/artifacts/install -Dbuildtype=debug -Dwerror=false --wrap-mode=nodownload && ninja -C /artifacts/build -j6 && meson test -C /artifacts/build core crypt unit smf-selection --print-errorlogs'
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-stage5 integration-08 dynamic
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-stage5 integration-10 incompatible
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-stage5 integration-11 static
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-stage5 integration-12 recovery
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-stage5 existing-regressions-02 regression
bash tests/pfcp/run-dnn-integration.sh /tmp/open5gs-stage5 existing-epc-02 epc
bash docker/dynamic-dnn/build-images.sh . /tmp/open5gs-stage5
bash tests/pfcp/image-smoke.sh /tmp/open5gs-stage5 image-smoke-02
```

Run names/context directories must be fresh; the evidence paths above already
exist. The archive includes command logs and sanitizer/association/config
commands with their original results. ASan/UBSan reruns use the retained stage
4A sanitized build with unchanged feature source inputs.

| Files | Stage 5 purpose |
|---|---|
| tests/meson.build; tests/pfcp/meson.build; dnn-session-driver.c | Real UE/gNB test executable using existing public test helpers, externally managed NFs |
| dynamic-dnn-integration.py; run-dnn-integration.sh | Fresh Mongo/network isolation, actual NFs, attempts/first requests/data/recovery and cleanup |
| existing-regressions.py | Existing 5GC and EPC tests with explicit lab TUN setup |
| session-error-reproducer.py | Independent wire characterization against modified and clean baseline UPF |
| docker/dynamic-dnn/* | External staging, code/diff identity, immutable build inputs, ELF packaging and local image export |
| image-smoke.sh | Runtime image library/startup/association evidence |
| tests/pfcp/cluster/* | Render-only isolated fixtures, lab TUN setup, evidence collection and UID-guarded rollback |
| verify-integration-pdml.py | External decoder ID/DNN/family comparison against effective YAML, independent of packet/retry count |
| Plan, ADR and stage reports | Results, limitations and explicit stopping point |

## Stopping point and concrete gates

The first-session mapping and compatible-pool MVP integration pass. Stage 5
does not close every matrix gate: automatic return eligibility fails in the
ordinary restart case, and baseline session error responses remain defective.
Those need separately reviewed decisions/corrections and reruns before claiming
complete recovery/error handling. No guarantees apply to established sessions
on the failed UPF. Broader IPv6/slice and image-backed NAS scenarios remain
explicitly unexecuted.

Cluster work requires a newly authorized stage 6, explicit context/dedicated
namespace, compatible isolated ancillary NFs/subscriptions/UE, imported images
on scheduled nodes, working SCTP/N3/N4/CNI isolation and TUN/device/capture
permissions. External N6 needs an independently verified owner return path.
The prepared runbook is not deployment evidence. No automatic stage 4B, stage
6 or session migration follows this report.
