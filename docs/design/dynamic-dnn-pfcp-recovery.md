# PFCP recovery after asymmetric Heartbeat loss

2026-11-04. Authorized follow-up to the dynamic DNN MVP and
[restart correction](dynamic-dnn-restart-fix.md). No stage 4B or cluster work.
HEAD remains `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, branch `main`,
Open5GS 2.8.0. Existing dirty work was captured before these edits and preserved.
The runtime profile targets TS 29.244 V19.5.0, Release 19; this is not whole-core
Release 19 certification.

## Finding and resulting behavior

The reported case is real. With actual SMF and UPF processes, selectively
losing only Heartbeat Responses in one direction made one endpoint time out
while its peer remained associated. Heartbeat Requests and other PFCP traffic
could still pass. After removing the loss, a previously passive endpoint had
no association retry timer: it stayed deassociated and did not recover the
learned mapping. The old behavior was reproduced in both directions without
restarting either process (`red-smf-02`, `red-upf-02`). This recovery limitation
already existed in the FSM; the latest user instruction authorizes its fix.

A peer that previously completed an association now acquires a retry timer
when entering `will_associate`, even if it originally waited passively.
Both SMF and UPF can therefore initiate Setup recovery. A never-associated
passive peer remains passive. Existing periodic retries continue while the
remote endpoint is unavailable; Heartbeat alone does not publish capabilities
or turn the association state back into `associated`.

On SMF timeout, deassociation clears owned learned metadata. A subsequently
accepted Setup republishes the UPF's advertised IE 233 snapshot and restores
its DNN match. If only the UPF times out, the still-associated SMF accepts the
new UP-originated Setup and replaces its snapshot. The prior epoch/Heartbeat
confirmation correction remains in force: delayed probes cannot erase or
confirm replacement data. Setup Recovery Timestamp still does not seed the
Heartbeat recovery history. No selector, allocator or session migration changes.
Legacy DNN OR location, rotation and fallback remain possible.

## Release 19 contract and session retention

Source: [TS 29.244 V19.5.0](https://www.etsi.org/deliver/etsi_ts/129200_129299/129244/19.05.00_60/ts_129244v190500p.pdf),
verified against the existing `lib/pfcp/support/r19.5.0/29244-j50.docx`:

- §6.2.2.2: reply to Heartbeats even without an association. Withholding replies
  to force a different recovery path would be incorrect.
- §6.2.6.2.1–2 and table 7.4.4.1-2: CP-originated Setup may request retention;
  empty PFCP Session Retention Information (IE 183) requests all sessions.
- §8.2.134: PFCPASRsp-Flags (IE 184), PSREI bit 1, acknowledges retention when
  an existing association is retained. Initial association does not claim this.
- §7.6.8–10: unsupported optional information is ignored; the first instance of
  an unexpected singleton repetition governs.

Recovery CP Setup explicitly requests all-session retention with empty IE 183.
The UP side recognizes this and acknowledges PSREI only when already associated.
The shared TLV encoder cannot emit an empty compound: a small PFCP builder
adapter appends the literal empty IE, and the PFCP decoder recognizes it before
calling the generic compound parser. The generator, caches and descriptors
already describe 183/184 correctly; no regeneration or generic GTP/TLV change
is required. All added packet buffers keep the PFCP header headroom and have
explicit ownership.

This profile supports all-session retention only. Nonempty/selective retention
information is optional unsupported input: it is ignored without turning it
into retain-all or acknowledging PSREI. Nested framing is still validated.
Duplicate 183 handling follows the first value, including unsupported-first
followed by empty. Independent byte fixtures cover both orders, unknown children
and truncation. A retention request is not a guarantee from another vendor.

Existing Open5GS failure/session restoration policy is otherwise unchanged.
Sessions on a peer the SMF declares failed may be released under that policy;
this correction does not promise to preserve them. Actual UPF restart loses
that process's sessions. A live session on C did survive the tested UPF-only
association timeout, where SMF remained associated. Sessions on unaffected A/B
survived both directional tests. UP-initiated recovery requires a peer supporting
that procedure; permanent network loss cannot be recovered until connectivity
returns. No bounded recovery-time SLA is claimed.

## Additional introduced defect found and fixed

A simultaneous-timeout repetition aborted the UPF. A diagnostic talloc abort
backtrace identified `pfcp_recv_cb` calling `ogs_event_free` on an event allocated
from the UPF event pool when pre-receive validation discarded a stale Setup
response. This was an introduced receive-error-path ownership defect, not a
transaction double free. It also affected the corresponding new SGW-C/SGW-U
branches. Each now calls its own pool's event-free function. SMF uses heap events
and correctly keeps `ogs_event_free`.

Evidence retains the failing `final-both`, `glare-trace-2` and `glare-trace-3`
runs, diagnostic preload source/library and backtrace. The final pair harness
injects 128 independent well-formed, unmatched Setup Responses into each NF
before its network test. They are all discarded and both original processes
continue. Simultaneous recovery with accelerated retries then passed three
normal runs, covering both original initiators, and an ASan/UBSan run. This
campaign is a directed regression, not exhaustive scheduling exploration.

## Change map

| Files | Change |
|---|---|
| `src/{smf,upf}/pfcp-sm.c` | Promote previously associated passive peers to retrying initiators; SMF marks recovery Setup for retention |
| `lib/pfcp/context.h`, `build.[ch]`, `path.[ch]` | Explicit CP retention intent; empty 183 emission; conditional 184/PSREI response |
| `lib/pfcp/handler.[ch]`, `src/{upf,sgwu}/pfcp-sm.c` | Pass existing-association state into retention acknowledgement |
| `lib/pfcp/pool-codec.c` | Bounded empty-183 support; first-instance handling; unsupported selective input ignored |
| `src/{upf,sgwu,sgwc}/pfcp-path.c` | Correct event-pool release on pre-receive discard |
| `tests/unit/pfcp-association-test.c` | Retention bytes, independent decode fixtures, repetition/optional/framing assertions |
| `tests/pfcp/pfcp-asymmetric-recovery.py` | Real SMF/UPF, unmatched-response regression, directional/both loss, feature-off and brief loss |
| `tests/pfcp/pfcp-netfilter-control.py`, `run-pfcp-recovery.sh` | Namespace-local nftables fault injection; counters, snapshots and exact container cleanup |
| `tests/pfcp/dynamic-dnn-integration.py` | Actual NAS/N4/GTP-U recovery, first-request destinations, unaffected-session continuity |
| ADR, plan and runbook | Updated recovery contract, evidence and reproduction commands |

## Executed evidence

Durable bundle: `/home/marujo/open5gs_pfcp_recovery_results_20261104`.
`SHA256SUMS` seals logs, captures, commands, initial/final provenance, complete
source snapshot, incremental patch and local image archives. Scratch builds:
`/tmp/open5gs-pfcp-recovery-20261104`. Every NF runs in a disposable Docker
network namespace; fresh MongoDB uses tmpfs. Only the test namespace's nftables
table is changed. No host firewall, deployment or existing database is modified.

| Result | Evidence and scope |
|---|---|
| PASS characterization | `red-smf-02`, `red-upf-02`: original recovery gap reproduced in both directions; unchanged PIDs/recovery values |
| PASS | `tests-ownership.log`: complete build; Meson core, crypt, unit and smf-selection suites; nine directed association cases including retention |
| PASS | `verified-association` seven runtime scenarios; `verified-selection` fourteen scenarios; static/learned, family/slice, OR/location and fallback regressions |
| PASS | `ownership-both-1`, `ownership-both-2`, `ownership-both-cp`: both endpoints recover automatically; 128 rejected responses per NF; no abort; no process restart |
| PASS | `verified-nas-smf`, `verified-nas-upf`: automatic recovery and first N4 destinations after directional loss, actual NAS accept and GTP-U replies |
| PASS | `verified-restart-up`, `verified-restart-cp`: ordinary process restart still recovers with repeated C/B/C/B first requests |
| PASS | `verified-brief`: exactly one Heartbeat Response lost; retries succeed without deassociation |
| PASS | `verified-feature-off`: association recovery also works with advertisement/learning disabled; no learned publication |
| PASS | `verified-existing`, `verified-epc`: registration/authentication, different-DNN slice and EPC attach/simple regressions |
| PASS | `verified-asan-association`: directed ASan/UBSan with leak detection; `ownership-asan-both`: actual instrumented SMF/UPF with narrow baseline suppression below |
| PASS | TShark 4.2.2 independent decoding of 233/identity/DNN/family and retention flags; effective YAML compared with every configured advertisement |
| PASS | Final local images: loader closure, help, startup and PFCP advertisement/learning smoke |
| FAIL, fixed | Initial empty compound encoder attempt failed; replaced by scoped adapter and retested |
| FAIL, fixed | UPF abort on discarded stale Setup response; backtrace and final regression described above |
| FAIL, baseline excluded | Unfiltered full-NF LeakSanitizer shutdown: 96 bytes in `talloc_enable_null_tracking` per NF, reproduced in a standalone talloc-only program without Open5GS |
| FAIL, baseline excluded | Existing Session Establishment rejection lacks Node ID; no-match fallback still triggers retransmissions/NAS reject; unchanged |
| NOT RUN (optional) | T09/T10, strict stage 4B |
| NOT RUN | Cluster, other vendors, image-contained full NAS, learned IPv6/slice NAS, external N6, selective retention and SGW malformed-Setup runtime campaign |

Unfiltered ASan `asan-smf`/`asan-upf` functional recovery passed, but NF exits are
nonzero due to the unrelated 96-byte talloc startup tracker. Raw reports remain.
`talloc-startup.c`/log independently reproduce it. `talloc-startup.lsan` suppresses
only the `talloc_enable_null_tracking` stack; no blanket talloc suppression or
`detect_leaks=0`. Filtered final full-NF runs exit zero and report no additional
ASan/UBSan faults. The directed helper disables null tracking on shutdown and
requires no suppression. The excluded SQN holding-window bug remains unchanged.

For the two final NAS runs, no peer DNN or location mappings are configured on
the SMF. UPF A is `127.0.0.7`/internet; B `.8` and C `.9` share edge1 and compatible
10.48.0.0/24 resources. SMF's provisioned allocator is unchanged. A session during
SMF-side loss goes to live B. After recovery:

| Run | PSI | First N4 destination | Frame | Retransmissions | NAS/data |
|---|---|---|---|---|---|
| `verified-nas-smf` | 4, 5, 7, 8 | C, B, C, B | 65, 75, 85, 95 | 0 each | accepted; two GTP-U replies each |
| `verified-nas-upf` | 4, 5, 7, 8 | B, C, B, C | 85, 95, 105, 115 | 0 each | accepted; two GTP-U replies each |

The differing initial destination is controlled round-robin position, not lost
eligibility. Each run's `sessions.json` contains attempt ID, requested DNN, UE IP,
selection source/reason, timestamps, SQN, first frame, CP/UP SEIDs, UL/DL TEIDs,
Cause, retransmissions and matched GTP-U echo frames. Evidence includes more than
the first post-recovery session and crosses the next periodic Heartbeat.
The separate no-match attempt records fallback/error behavior; its failed NAS
result is not counted as successful compatible selection.

## Images and reproduction

Final dirty core hash: `0d6a194893d6` (full hash in final provenance).
Locally saved runtime images:

| Image | Docker inspection identity |
|---|---|
| `open5gs-smf:2c80f8f2ba9d-dirty-0d6a194893d6` | `sha256:943188a274fbb0fb40ed2cf10bc2cce6814d73391b2c349e117e470571cd89ff` |
| `open5gs-upf:2c80f8f2ba9d-dirty-0d6a194893d6` | `sha256:cdc8cfc0b8e1feda175f1205e5da1d9e45f9f6dae6212ad8973019bf416f11ca` |

Image archive SHA-256:
`f5ddfea3d577ac0f94839bdabbb188e8b061dad5898a6e10a666b655766b6836`.
Prior image bundles remain historical and unchanged. No images were published.

The compiler/tools image remains
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`:
Ubuntu 24.04, GCC 13.3, Meson 1.3.2, Ninja 1.11.1, Python 3.12, TShark 4.2.2.
The exported 4 MB isolated network helper
`open5gs-pfcp-nettools:nft-1.1.0-1370dbd7f467`, identity
`sha256:2583c9dad34f062503bc64c92293172a1cafd0ebb09a7273fc66609fb9cbbbb1`,
contains nft 1.1.0 and its minimal musl/busybox closure; its recipe/source identity
are included. Legacy iptables is unavailable on this host kernel; nftables was
used successfully inside each isolated namespace. No packages were installed
on the host and no cluster context was accessed.

Follow the [runbook](dynamic-dnn-runbook.md) to extract/build a fresh snapshot,
load the local archives and execute `run-pfcp-recovery.sh`. Keep original FAILs
and raw sanitizer output when reproducing; retain every command's exit status.
The complete source archive includes pinned dependencies. Core source identity
is checked against the build staging before packaging the images.

This completes the authorized local recovery correction. It neither changes
legacy fallback nor certifies interoperability/session preservation beyond the
cases above. Stage 4B and cluster execution require separate authorization.
