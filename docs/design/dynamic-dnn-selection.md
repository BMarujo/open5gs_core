# Stage 4A: learned DNN mappings in the legacy SMF selector

Current project status: [final review](dynamic-dnn-final-review.md) and
[operation guide](dynamic-dnn.md). This dated report preserves the results and
limits at its execution date; later corrections/validation do not rewrite it.

Executed on **2026-10-11**, user timezone Europe/Lisbon. Prompt 4A in
[the plan](../../OPEN5GS_DYNAMIC_DNN_PLAN.md) is complete for the scope below.
Contract: [ADR-001](ADR-001-dynamic-dnn-learning.md). Previous evidence:
[codec](dynamic-dnn-codec.md), [association/owned learning](dynamic-dnn-association.md).
**Stage 4B and subsequent stages have not started.**

## Outcome and evidence boundary

UPF B initiated PFCP Setup and advertised `edge1`. The SMF had **no `client.upf`
entries at all** and no configured peer location criteria. Its first new
allocated session object for `edge1` selected B using `source[learned] reason[dnn]`.
A, advertising only `internet`, preceded B in the peer list, so first-peer or
RR fallback would have selected A. This controls the principal alternative
explanation for the result. Later addition of C for `edge1` produced B/C/B.
The SMF PID remained 39 throughout that test; it was not restarted/reconfigured.

The integration test executable links **actual libsmf**, uses normal SMF
initialization, PFCP receive handlers/FSM and the NF event loop. A test-only
stdin poll callback creates real SMF session objects, runs
`smf_sess_set_ue_ip()` **before** `smf_sess_select_upf()` and records the result.
Registration occurs before the NF thread starts, through a linker wrapper of
`smf_pfcp_open`; commands execute on that same poll thread. No production
endpoint, thread synchronization change or runtime test hook was added.

This proves selection at the real allocation/selection boundary. **It is not a
complete UE PDU Session establishment test:** NAS, UDM/PCF authorization, N4
Session Establishment, GTP-U/N6 and an established UE session are not exercised.
The retained session below is a real allocated SMF object, not a claim of UE
session continuity. These distinctions also apply to T07/T08/T15/T22 results.

## Checkout, sources and isolated environment

HEAD remained `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, branch `main`, declared
Open5GS version `2.8.0`, with no HEAD tag. All 53 inherited modified/untracked
files were backed up before editing. There was no checkout, version change,
commit, push, install, cluster action or deployment. No applicable repository
AGENTS instructions were present; the home Codex AGENTS file was empty.

The reference remains **TS 23.501 V19.5.0 and TS 29.244 V19.5.0, Release 19**,
with PFCP header version 1. The official
[TS 23.501](https://www.etsi.org/deliver/etsi_ts/123500_123599/123501/19.05.00_60/ts_123501v190500p.pdf)
§6.3.3.1–3 was rechecked: provisioning available UPFs and selecting a UPF for
a session are distinct. The learned DNN is one input to the existing selector;
it does not implement every required-UPF-functionality constraint from UDM or
certify full Release 19 conformance. The pinned
[TS 29.244](https://www.etsi.org/deliver/etsi_ts/129200_129299/129244/19.05.00_60/ts_129244v190500p.pdf)
codec/association contract from stages 2/3 is unchanged.

Sources and builds were outside the checkout at `/tmp/open5gs-stage4a`.
The baseline was **HEAD plus inherited stages 2/3**, not a new clean-HEAD claim.
The existing image `open5gs-pfcp-codec-build:20261007` had ID
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`:
Ubuntu 24.04, GCC 13.3.0, Meson 1.3.2, Ninja 1.11.1, TShark 4.2.2.
Initial setup fetched Meson subprojects into that external source tree:
freeDiameter `14725af3ba0edbf9ff61c4e3239ed42464423b2e`, prometheus-client-c
`a58ba25bf87a9b1b7c6be4e6f4c62047d620f402`. Compilation and execution then used
Docker `--network none`. Runtime tests additionally used container-local
`NET_ADMIN` and `/dev/net/tun`; lab interfaces were `poola`, `poolb`, `poolc`.
No host dependencies, interfaces or production services were changed. Expected
NRF connection/Diameter diagnostics in the isolated SMF do not block PFCP.

## Effective DNN mapping and preserved policy

The selector reads the owned snapshot directly; it does not modify or borrow
new strings into `node->dnn[]` and retains no pointer into a snapshot.

| Local/accepted state | Effective DNN source and match |
|---|---|
| `learn_upf_dnn: false` | Existing static list, irrespective of any snapshot |
| Learning on, accepted `ABSENT` | Existing static list; old learned list is already replaced |
| Learning on, NULL after invalidation or before accepted evidence | `learned-unavailable`; no DNN match, no inferred static fallback |
| `VALID` / `DEGRADED` | Usable learned groups only; no union with static |
| `UNUSABLE` / `UNSUPPORTED` | Learned source with no DNN match; static is not revived |

Case-insensitive DNN comparison is retained. Any group with S-NSSAI restrictions
is excluded from this MVP's DNN arm, even if a session's slice happens to match;
the selector cannot safely flatten that context. Explicit IP Version flags must
cover the **effective** `sess->session.session_type` at selection. Unrestricted
groups for the same DNN may collectively cover IPv4v6. A missing IP Version is
unknown and permits static-like matching for IPv4/IPv6/IPv4v6; it is not proof of
dual-stack support. Unknown/type 0, unstructured and Ethernet produce no learned
DNN match. Owned pool identities/context remain untouched, rather than being
flattened into strings or used as routing/prefix evidence.

The session type is the effective value, including any earlier SMF allocator
downgrade from IPv4v6. The existing S5-C caller can invoke selection **before**
setting that type; a type-0 session therefore gets no learned DNN match there.
The caller/order was deliberately preserved; location and fallback remain
possible. Static matching with the feature off/accepted absence retains its
previous behavior regardless of session type. Full EPC/roaming runtime
compatibility is not claimed from the builds or this boundary test.

Only the DNN arm changes. The location checks keep their exact E-UTRAN cell,
NR cell and TAC conditions/order, including legacy NR checks. DNN **OR** location
can match. Associated matching peers are searched in the same cyclic order.
With no match, associated RR remains enabled unless `no_pfcp_rr_select` is set;
the final return is still the first peer, including an unavailable or DNN-
incompatible peer. RR-disabled mode still permits matching peers. No strict
eligibility filter, no-candidate API or new NAS/PFCP error/cleanup path was added.

## Change map and observability

| File | Stage 4A change |
|---|---|
| `src/smf/context.c` | Effective static/learned DNN helper, context-aware comparison and source/reason logging in the existing selection path |
| `configs/open5gs/smf.yaml.in` | Update opt-in comments to describe the now-active legacy selection integration; default remains false |
| `tests/unit/smf-selection-test.c`, `smf-selection-main.c` | Separate ABTS executable linked to actual libsmf; six cases including frozen pre-4A policy comparisons |
| `tests/unit/smf-selector-driver.c` | Test-only executable with actual SMF initialization and poll-thread allocation/selection commands |
| `tests/unit/meson.build` | Register `smf-selection`; build the driver only with Linux GCC/Clang linker wrapping |
| `tests/pfcp/dynamic-dnn-selection.py` | Disposable real-NF and independent-wire-fixture scenarios, session observations and PCAP |
| Plan, ADR and design reports | Record implemented policy, executed evidence, previous-stage provenance and remaining limits |

No PFCP library, codec, generator/cache, UPF or shared SGW behavior changed in
4A. Source comparison verifies all inherited implementation files unchanged
except the SMF YAML comments and unit Meson registration. In `context.c`, changes
are confined to the selector block. Allocator/session-management function bodies
and `nudm-handler.c`, `npcf-handler.c`, `gn-handler.c`, `s5c-handler.c`,
`nsmf-handler.c` and `pfcp-sm.c` remain unchanged.

Selection INFO logs identify the requested DNN, metadata source, reason,
association state, last accepted generation and selected endpoint, for example:

```text
UPF selected DNN[edge1] source[learned] reason[dnn] associated[1] generation[1] peer[[127.0.0.8]:8805]
UPF selected DNN[missing] source[learned] reason[rr-fallback] associated[1] generation[1] peer[[127.0.0.8]:8805]
UPF selected DNN[edge1] source[learned-unavailable] reason[first-peer-fallback] associated[0] generation[0] peer[[127.0.0.7]:8805]
```

The source labels describe the metadata view, **not** a claim that its DNN
matched. Reasons distinguish `dnn`, `e-cell-id`, `nr-cell-id`, `tac`,
`rr-fallback` and `first-peer-fallback`. DEBUG precedence diagnostics identify
the requested DNN, peer, static/effective match difference, status and generation;
the existing publication logs expose snapshot contents/status.

## Results and practical limits

Durable evidence: `/home/marujo/open5gs_stage4a_results_20261011`.

| Check | Result | Evidence / boundary |
|---|---|---|
| Inherited baseline: core/unit and SMF/UPF/SGW-C/SGW-U builds | PASS | `baseline-build.log`, `baseline-tests.log`, 2/2 tests |
| Final four NF builds, core/unit and selector/driver targets | PASS | `final-build.log`; no final diagnostics in changed code |
| Final Meson `core unit smf-selection` | PASS | `final-tests.log`, `meson-testlog.txt`, 3/3, no failures/skips |
| Six selector ABTS cases | PASS | Actual public selector: learned rotation, precedence/NULL/ABSENT, family/slice/unknown types, replacement/lifetime, location/fallback/loss and baseline differential |
| Frozen pre-4A policy oracle | PASS | 6,144 combinations: 16 association masks × 2 RR modes × 4 cursors × 4 DNNs × 6 location cases × feature-off/accepted-ABSENT; actual selector compared to original static/location policy |
| ASan + UBSan selector | PASS | Full instrumented libsmf/PFCP dependencies; same six cases; leak detection and halt-on-error enabled, `sanitize-selection.log` |
| Runtime scenarios | PASS | 14/14, `runtime-complete/results.json`; 36 driver observations, including 32 first selections of newly allocated objects, `observations.json` |
| UP-initiated first DNN selection | PASS, boundary | Real A/B announcements, no SMF `client.upf`, no location; first `edge1` goes to B; first `internet` goes to A |
| Multiple same-DNN UPFs and allocator | PASS, boundary | Real C added live; B/C/B; live SMF-assigned edge1 addresses remain distinct; internet/edge1 use different configured SMF subnets |
| CP-initiated mapping / feature-off / conflict | PASS | Actual SMF transactions/FSM, independent mock UPF responses; source 6; static wins when off, learned replaces static when on |
| Reassociation and owned lifetime | PASS | Accepted replacement, absent, invalid, unsupported, degraded, duplicate SQN and rejected replacement; old allocated object's owner/IP unchanged |
| Actual peer loss and return | PASS, scoped | Real C stopped, actual Heartbeat timeout/invalidation; B serves new selections; explicit Setup from returning independent fixture at C's same Node ID/endpoint restores C. This does not claim automatic passive renegotiation |
| DNN OR location | PASS | Runtime TAC-matching A wins despite incompatible DNN; nonmatching TAC lets B win by learned DNN; E/NR cell and both TAC arms also covered by ABTS |
| No DNN match | PASS, characterization | Separate runtime RR fallback; with RR disabled first unavailable A still returned despite a live nonmatching B; all-unavailable case covered by ABTS |
| Independent decoder | PASS, scoped | TShark 4.2.2 plus `verify-first-upf-pools.py`: first real A/B/C requests, exact IDs/lengths, internet/edge1 and IPv4 flags; `decoder-verified.json`, `selection-pools.tsv`, `selection.pdml` |
| Allocator/callers/previous-stage preservation | PASS | `preservation.json`, entry hashes and `stage4a-only.diff` |
| Established UE continuity, full T15/T22 data plane | NOT RUN | Retained allocated object is tested; no NAS/SBI/N4 session establishment, UE traffic, return routing/NAT or incompatible-pool forwarding test |
| Live EPC/roaming interoperability; full R19 conformity | NOT RUN | Four NF builds and shared regressions do not establish these claims |
| 4B, API/SUPI, orchestrator, deployment wait, migration, cluster/later stages | NOT RUN | Outside authorized scope |

Relevant matrix coverage: T01/T05/T06/T07/T08/T11/T12/T13/T16/T17/T19/T21/T26
at the described selection boundary; T15 is scoped to retained allocated objects
and T22 to one SMF allocator/address uniqueness, **not** N6/UE continuity.
The detailed stage 3 T18/T24/T25/T27 evidence is retained and not relabelled as
new stage 4A runtime certification.

Intermediate FAIL evidence was retained and resolved: test targets initially
lacked the generated SMF-header include directory; the driver passed a string
literal to a legacy non-const SUPI argument. The first loss/return fixture
incorrectly assumed SIGCONT would initiate a new Setup; the final fixture
explicitly negotiates return. An IPv6 `/64` SMF lab subnet provided no allocatable
UE `/64` prefixes; changing only the test configuration to `/48` resolved that
allocator precondition. No core workaround, allocator change or new automatic
Setup was introduced. Final checks have **no unresolved FAIL**.

## Reproduction and artifact provenance

Materialize an external copy of HEAD plus inherited and current working sources,
as in previous reports. Commands below run inside the image, with the external
directory mounted at `/artifacts`, working directory `/artifacts/source`:

```sh
meson setup /artifacts/build /artifacts/source --prefix=/artifacts/install -Dbuildtype=debug -Dwerror=false
ninja -C /artifacts/build -j6 tests/core/core tests/unit/unit tests/unit/smf-selection tests/unit/smf-selector-driver src/smf/open5gs-smfd src/upf/open5gs-upfd src/sgwc/open5gs-sgwcd src/sgwu/open5gs-sgwud
meson test -C /artifacts/build core unit smf-selection --print-errorlogs
meson setup /artifacts/sanitize-build /artifacts/source --prefix=/artifacts/sanitize-install --wrap-mode=nofallback -Db_sanitize=address,undefined
ninja -C /artifacts/sanitize-build -j6 tests/unit/smf-selection
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 meson test -C /artifacts/sanitize-build smf-selection --print-errorlogs
python3 tests/pfcp/dynamic-dnn-selection.py /artifacts/build /artifacts/runtime-complete
tshark -r /artifacts/runtime-complete/selection.pcapng -T pdml > /artifacts/selection.pdml
python3 /artifacts/verify-first-upf-pools.py /artifacts/selection.pdml /artifacts/decoder-verified.json
```

Compilation/tests use `--network none`; runtime additionally needs
`--cap-add NET_ADMIN --device /dev/net/tun`. The source setup may fetch the
documented Meson subprojects. The lab Python script should run only in that
disposable network namespace. The decoder verifier is included in the evidence
directory. No `ninja install` is required. Test commands do not require MongoDB,
subscribers, external NRF or deployment changes.

Evidence includes `initial/` with all entry files and inherited diff;
`stage4a-only.diff` isolates this stage from existing work. `tested-source-sha256.json`
checks the exact non-documentation working sources against the isolated source
copy, and `preservation.json` verifies the untouched preceding implementation,
allocator bodies and callers. `versions.json` records checkout/toolchain/image
and subproject identities. Runtime YAMLs, NF logs, synthetic-session observations,
PCAP and independent decoding are retained. Intermediate failed runs remain
separate from `runtime-complete`. `artifact-sha256.json` hashes the durable bundle.

## Remaining limits and stopping point

Preserved OR/fallback can select a DNN-incompatible or unavailable peer; logging
makes that visible and does not turn it into strict eligibility. NULL
invalidation intentionally yields no DNN match but cannot prevent location/RR/
first-peer fallback. Slice exclusion and explicit-family checks affect only the
DNN arm. Same-DNN UPFs still need operator-verified pool/routing compatibility:
pool IDs do not encode subnets/NAT/ranges, and this selector does not select
allocators or prove N6 reachability.

Stage 3's SQN holding-window and late Heartbeat restart invalidation limits
remain. A passive SMF can stay associated with invalidated metadata until
another accepted Setup; resuming a suspended UPF does not guarantee that Setup.
The existing session restoration/release logic was preserved, so this stage
cannot promise established-session continuity when a peer itself fails/restarts.

Stage 4A ends here. Any 4B hardening or later UE/N6/integration/deployment work
requires a new explicit instruction; neither is started automatically.

Historical follow-up, 2026-10-12: stage 5 was subsequently authorized. The
[integration report](dynamic-dnn-integration.md) adds actual NAS/N4/GTP-U,
image and regression evidence. It also reproduces the ordinary peer-return
FAIL and isolates the baseline session error response defect. The NOT RUN
statements above describe the stage 4A boundary at its completion, not the
later stage 5 results. No stage 4B or cluster execution followed.
