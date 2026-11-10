# Official upstream integration, 2026-11-10

## Source and decisions

Entry was clean `main` at `246d52fe22bff74ce5ea837d26e612e401ddc6af`.
The official remote is `upstream https://github.com/open5gs/open5gs.git`.
`git fetch --no-tags upstream main` fetched
`5cb35ead227d25a5ed2161d66faade56a9fd12db`: 157 official commits after the common
base `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`. The source still declares 2.8.0;
this is not a switch to another release tag or whole-core Release 19 certification.

An isolated worktree on `integration/upstream-20261110t031011z` preserved the
original checkout during validation. Merge `89e47d60631ad9eff0d4a3a3c9e4671acbc37d83`
retains all official changes. The only textual conflict, `tests/unit/meson.build`,
was resolved by keeping both the dynamic-DNN selector tests/driver and upstream's
EIR FSM suite. Generated PFCP descriptors retain our scoped pool codec and the
upstream expansion to fifteen SDF filters. No upstream source change was dropped
to make a previous test pass. Fourteen trailing-whitespace lines inherited from
upstream's new freeDiameter EIR template are recorded in the evidence; focused
checks on our edits pass. No unrelated reformatting is included.

The upstream changes include defensive NAS/SBI/GTP validation, identity/session
lifetime fixes, EIR/S13 support, PFCP restoration and parser bounds. In particular,
`6a56d9bfe` bounds UE-requested GFBR/MFBR in session modification to representable
NGAP values using checked arithmetic. This is not a new 50-Mbit/s rate limiter.
It is also not evidence of identical maximum throughput: performance remains a
measured property, with the earlier one-UE comparison explicitly inconclusive.

## Heartbeat expiry and retention

Official change `926550203` calls UPF restoration on heartbeat expiry, deleting
sessions belonging to that unreachable SMF. This agrees with the configured
failure treatment in [TS 23.527 V19.5.0, §4.4.3.1](https://www.etsi.org/deliver/etsi_ts/123500_123599/123527/19.05.00_60/ts_123527v190500p.pdf).
The old asymmetric-loss test expected a C session to survive UPF-only expiry;
that expectation is no longer valid. Its failed execution is preserved as
`nas-upf-old-expectation-01`, exit 1. The corrected test requires the exact old
UE context's deletion, absence of data on its old TEID, successful fresh Setup,
relearned DNN selection and accepted new sessions on C. Both initiators pass.
Unaffected A/B data is also checked. Retention IE 183/PSREI 184 applies when the
UP endpoint still has its existing association/context; no false retention
acknowledgement is added after expiry. No session migration is implemented.

Test changes are limited to the NAS/GTP driver, recovery orchestration and its
expectations. An explicit `absent` probe accepts silence or GTP Error Indication,
never a data response for the removed tunnel. Existing ping/probe commands retain
their semantics. Association initiator is independently configurable for these
recovery tests. The Helm image build now also runs `eir-fsm` and checks dynamic
library closure for every installed NF daemon, including the new EIR executable.

## Executed local verification

Evidence root: `/home/marujo/open5gs_upstream_merge_results_20261110T031011Z`.
Commands, exit codes, logs, configuration, literal fixtures and packet captures
are retained outside Git. The pinned toolchain is
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`.
Builds/tests use isolated Docker networking; no physical deployment is needed.
Vendor inputs remain freeDiameter `14725af3ba0edbf9ff61c4e3239ed42464423b2e` and
prometheus-client-c `a58ba25bf87a9b1b7c6be4e6f4c62047d620f402`.

| Behavior / clause scope | Test | Result and evidence |
|---|---|---|
| Full compilation and shared-library consumers | 4,728 Ninja steps; all NFs | PASS, `initial-build.log` |
| Core, crypto, PFCP unit, legacy selector, EIR FSM | Five Meson suites | PASS, `build/meson-logs/testlog.*` |
| Memory/ownership; pool and association malformed input | ASan/UBSan, leak detection enabled, no suppression; directed campaigns | PASS, `sanitize-01.log`, `sanitize-pool`, `sanitize-association` |
| TS 29.244 R19 pool types/nesting/multiplicity | Fresh and cached generator outputs, 28 caches | PASS, `generator-results.json`; timestamp-normalized descriptors match |
| Dynamic vs static, both restart initiators, first N4, legacy fallback | Real NAS/GTP integration | PASS, six cases in `local-integration-results.json` |
| TS 29.244 heartbeat/reassociation; owned replacement | Five pair-level selective loss cases | PASS, `recovery-results.json` |
| TS 23.527 UPF expiry removes old contexts, then new use | Four full NAS selective-loss cases, CP/UP initiators | PASS, `nas-loss-results.json`; old test failure retained separately |
| Existing IPv4/IPv6/slice handling and learned selection | Five existing/dynamic CP/UP matrix cases | PASS, `ipv6-results.json` |

`meson setup ... --buildtype=debugoptimized -Dwerror=false --wrap-mode=nodownload`,
`ninja -C ... -j4`, and `meson test ... core crypt unit smf-selection eir-fsm
--print-errorlogs` are the baseline. Sanitized builds add
`-Db_sanitize=address,undefined`, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Exact complete argv and
wrapper invocations are in the named evidence files, rather than reconstructed
from prose.

## Publication, deployed verification and rollback

Executable source `8826ffb868a25235b746b674805a562e11a825aa` is committed and pushed to the fork's
`main`. Public image **`bmarujo/open5gs:dnn-8826ffb868a2`**, OCI index
`sha256:4a4767caa0277315d6b7a0b7e1929cb73b955479583e460d2d35d720733e1cc1`; core-input SHA256
`a66c3d32823b8f1bd428b7514508cb05ea89f645b859f10ae2975dbca0736c05`. Public amd64 manifest
`sha256:13e6c331afc0fd15a4247de6787d2b81355a135e7e3eef66b3f4f7c28deacc0b`
and config `sha256:6b65b6fe97a720db386cdeb2513da2a4d9d1a0240c5559e4044265e265e6081b`
are hash-verified and bind the source labels; `public-oci-chain.json` records the
chain. The Docker build runs the five suites again and verifies every installed
NF's dynamic-library dependencies. Runtime version checks include SMF/UPF/EIR.
No official-upstream push or PR is created.

Both normal deployments use the new SMF/UPF image: physical explicit context
`kubernetes-admin@kubernetes`, namespace `open5gs`; Containernet's pinned wrapper
API `https://10.200.0.11:6443`, namespace `open5gs`. The remaining nine NFs retain
Gradiant 2.8.0. Actual running imageIDs, not tags/appVersion, are checked. Rendered
diffs change three image fields in the emulation. Physical additionally preserves
the NRF/SMF/BSF SBI Service guard and temporarily uses SMF RollingUpdate with
maxSurge 0/maxUnavailable 1, restoring Recreate via a second Helm upgrade. Its
old terminating Pods on unavailable nodes remain untouched. No broad node/network
cleanup or cache-restart sequence was needed for this update.

**Live PASS:** normal NAS registration, accepted PDU session, learned-DNN selection
and bound-TUN gateway ICMP in each installation. Physical canonical SMF-network
capture confirms one initial request to `10.250.4.20`, Cause Accepted, zero
retransmissions and zero kernel capture drops. Capturing inside the ordinary
non-root NF processes failed for lack of effective capture permission. A bounded
root debugging child with only NET_RAW and `tcpdump -Z root` captured the existing
SMF network namespace. The 60-second capture wrapper exits 124 as expected after SIGINT/flush; the
recorded packet/drop statistics are complete. NF Pod UID/restart count are
preserved. Kubernetes retains
the stopped child metadata until that Pod is replaced; no active capture workload
is left. Raw failed collection attempts 01–04 are preserved separately from the
successful NAS/data tests and final capture attempt 05.

**Full isolated Containernet matrix PASS:** 12 positive sessions, zero positive
PFCP retransmissions, 57 correlated payload checks to the three DNs, eight
zero-drop captures, B/C rotation and use after C is recreated with the same pool
identity. Six SMF UID/PID/restart checks confirm the fixture SMF stayed running.
The legacy no-match case still falls back and rejects NAS, with one separate
automatic new NAS attempt; negative retries are not counted as positive success.
Both owned namespaces and all DN sniffers are removed through scoped cleanup.
Evidence: `containernet-matrix-01/analysis/validation-result.json`, per-attempt
first-N4 rows, N6 correlation, mounted configs and final Pod imageIDs.

At entry the emulation was stopped. All seven owned stopped node/DN filesystems
were privately checkpointed before supported stale cleanup/start; operator MongoDB
was backed up offline and reused. This necessary cold recovery changes Pod UIDs
relative to the stopped cluster. Preservation during subsequent testing is checked
against the recovered normal deployment. No checkpoint/database archive is pushed.

The previous image `bmarujo/open5gs:dnn-384a54f4d9a0` at
`sha256:a05e25b5cc4e39defa42b87fed82bcd0ace632ce477a53059f6ebee6fdffdb8b`
remains available for rollback. Restore that exact tag/digest pair only in the
saved actual SMF/UPF values; render/review and upgrade the explicit target release.
Keep the physical SBI post-renderer on rollback while unavailable-node endpoints
remain. Blindly rolling back Helm to an older manifest can reset its guard.
Rollback restarts NFs and cannot restore old process memory or PDU sessions.
The deployment repository's `deployments/open5gs/UPSTREAM_UPDATE_20261110.md`
contains current commands and the scoped rollback procedure.

## Bounded throughput follow-up

A fresh three-pair single-UE TCP canary compares the previous deployed fork
`384a54f4d9a` with static mappings against the merged image with learned mappings.
It is a merge regression screen, **not** another comparison against the official
pre-feature `2c80f8f2ba9d` base. Six owned TC stacks are temporarily detached under
the existing lock/journal and restored in finally; common QoS is non-binding
10 Gbit/s, without a TCP application cap. Same Pods, placement and outer CPU/
memory budgets apply to both variants. Each trial measures UL/DL for ten seconds
following warm-up/omit. Measured results are recorded below;
previous original/feature results remain historical. No exact-throughput or
universal capacity claim follows from this small virtual test.

## Measured merge canary and final state

**Control/protocol PASS:** six accepted sessions and twelve TCP measurements,
one UE/one flow, three alternating pairs, zero first-PFCP retries. Independently
checked PFCP QER and NGAP QoS are equal, with common UL/DL AMBR/MBR of 10 Gbit/s,
5QI 9 and ARP priority 8. All three new Setup Requests advertise IE 233 and use
learned DNN selection; the previous fork uses static mappings. No HTB/netem or
application TCP rate cap is active during measurement. Saved TC profiles are
restored with the native journal cleared. Placement and resource budgets match.

**Throughput equivalence: INCONCLUSIVE overall.** This small virtual canary does
not establish a source-caused regression, nor establish unchanged UL throughput.
No sample is discarded and no further trial is silently added to obtain PASS.

| TCP receiver mean, Mbit/s | Previous deployed fork | Merged fork | 90% paired ratio interval | ±10% screen |
|---|---:|---:|---:|---|
| UL | 372.779 | 342.763 | 0.794–1.071 | INCONCLUSIVE |
| DL | 350.260 | 362.250 | 1.008–1.061 | PASS |

UL samples, previous/new Mbit/s: 416.585/351.948, 353.985/325.425,
347.766/350.915. DL: 340.056/345.613, 353.897/367.739,
356.829/373.399. Exact samples and intervals remain in
`merge-perf-1ue-analysis-02/{result.json,samples.csv,samples.json}`. Results refer
to the **pre-merge fork**, not the official pre-feature original core. Resolving
the UL uncertainty or proving capacity needs a separate controlled campaign;
there is no universal/exact performance guarantee from this test.

The first analysis failed at runtime image identity: cloud-1 CRI returns a
`docker.io/library/import-2026-11-10@sha256:96e02f…` collection alias for the old
image. The independently hashed image archive and node-specific `crictl inspecti`
confirm the actual config is exactly old `sha256:5d51ae7b…`, not the new or the
official-baseline config. The analyzer now optionally requires that exact
requested reference, node, CRI repoDigest and archived config ID together for
an alias. Merely finding an image in a multi-image index does not pass. Seven
positive/negative verifier checks pass. Failed analysis-01 is preserved;
analysis-02 reuses the unchanged measurements with the strict CRI evidence.
This is a test-evidence correction, not a core behavior change or image rebuild.

**Final verification PASS:** 13 active normal Pods Ready in each installation,
both SMF/UPF imageIDs verified against the public OCI chain, actual mounted
advertisement/learning configuration and daemon versions checked. All 13 normal
Containernet Pod UIDs/imageIDs/restart counts match the post-rollout baseline
through both isolated campaigns; all four emulated nodes are Ready. Physical
other-NF/MongoDB/WebUI fingerprints are unchanged from entry. Its two pre-existing
unreachable nodes and four old terminating Pods remain, with the SBI guard safe.
SMF strategy is Recreate. Both temporary capture children have terminated without
an NF restart. Owned lab/probe/smoke namespaces, synthetic subscribers and DN
sniffers are removed. See `final-verification.json` and supporting snapshots.

Executed failures retained: old session-survival expectation after the upstream
UPF-expiry change; capture-tool attempts 01–04; initial import-alias analysis.
All corrected functional paths passed. Pending/NOT RUN: full equivalence of
throughput, merged multi-UE capacity/soak, cross-vendor retention, full new EIR/S13
end-to-end coverage and whole-core Release 19 conformance. Stage 4B is NOT RUN
(optional) and remains unimplemented. No PR or official-upstream push was made.

## Limits

No complete 3GPP conformance claim. No strict 4B, orchestrator, API/SUPI selection,
UPF allocator change or session migration. Legacy DNN OR location and fallbacks
remain intentional. Full cross-vendor PFCP, selective retention, IPv6 external
N6 throughput, real radio, long soak and every upstream error branch are NOT RUN.
Earlier performance numbers retain their original commits and are not silently
attributed to this merged build. Live-cluster results will identify actual image
IDs and first PFCP attempts rather than infer success from image tags alone.
