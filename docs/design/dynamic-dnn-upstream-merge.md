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

Image build, publication and live verification are being recorded separately;
local PASS is not yet a claim of live rollout success. The prior deployed image
is `bmarujo/open5gs:dnn-384a54f4d9a0` at
`sha256:a05e25b5cc4e39defa42b87fed82bcd0ace632ce477a53059f6ebee6fdffdb8b`.
It remains the rollback image. Do not delete it or rewrite historical evidence.

## Limits

No complete 3GPP conformance claim. No strict 4B, orchestrator, API/SUPI selection,
UPF allocator change or session migration. Legacy DNN OR location and fallbacks
remain intentional. Full cross-vendor PFCP, selective retention, IPv6 external
N6 throughput, real radio, long soak and every upstream error branch are NOT RUN.
Earlier performance numbers retain their original commits and are not silently
attributed to this merged build. Live-cluster results will identify actual image
IDs and first PFCP attempts rather than infer success from image tags alone.
