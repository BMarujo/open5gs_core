# ADR-001: Learn UPF DNN mappings during PFCP Setup, retaining legacy selection

Date: 2026-10-06; Release 19 revision: 2026-10-07; stage 3: 2026-10-08;
stage 4A: 2026-10-11; stage 5: 2026-10-11–12.
Feature regression and asymmetric recovery corrections: 2026-11-04.
Status: stages 2, 3 and 4A implemented and validated for the documented scope.
Advertisement, owned learning and legacy selection integration are implemented.
Stage 5 local integration and image preparation are delivered with explicit
recovery and baseline error FAILs. Strict selection (4B) and cluster execution
(stage 6) have not started.
The introduced late-Heartbeat invalidation regression was subsequently fixed;
see [correction and new evidence](dynamic-dnn-restart-fix.md). Historical stage
results below retain their original scope and dates.
The subsequently authorized asymmetric Heartbeat recovery and event ownership
corrections are specified in Decision 7 and the
[recovery report](dynamic-dnn-pfcp-recovery.md).
Baseline: `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, branch `main`.
Evidence and limitations: [audit](dynamic-dnn-audit.md),
[executable characterizations](dynamic-dnn-audit-experiments.md),
[Release 19 clause review](dynamic-dnn-release19-review.md).
Executed codec changes/results: [stage 2 report](dynamic-dnn-codec.md).
Executed association changes/results: [stage 3 report](dynamic-dnn-association.md).
Executed selector changes/results: [stage 4A report](dynamic-dnn-selection.md).
Executed integration/images: [stage 5 report](dynamic-dnn-integration.md).
Prepared isolated validation/rollback: [runbook](dynamic-dnn-runbook.md).

## Context and scope

An incoming Setup already creates a PFCP peer, but does not populate its DNN
mapping. Today SMF selection obtains that mapping from YAML. We will advertise
actual configured UPF pools and project learned metadata into the DNN arm of
the existing selector. SMF services, subscriptions and address allocation stay
operator provisioned. No stage 4B, API/SUPI policy, orchestrator, deployment wait,
session migration, NRF discovery or Association Update implementation is included.

The protocol association, usable capability snapshot and session selection are
three different decisions. A successful association is neither an authorization
for every DNN nor proof that the peer can route an allocated UE address.

## Sources and normative boundary

- [S1: TS 23.501 V19.5.0, Release 19](https://www.etsi.org/deliver/etsi_ts/123500_123599/123501/19.05.00_60/ts_123501v190500p.pdf), §6.3.3.1–3: UP-initiated association is an explicit provisioning path; session selection remains a separate SMF function.
- [S2: TS 29.244 V19.5.0, Release 19](https://www.etsi.org/deliver/etsi_ts/129200_129299/129244/19.05.00_60/ts_129244v190500p.pdf), clause mapping below; same version as the checked-in generator DOCX.
- [S3: official build guide](https://open5gs.org/open5gs/docs/guide/02-building-open5gs-from-sources/), checked against the checkout's guide and Meson files.

These pinned versions supersede the initial Release 17 review. The reviewed
scope targets Release 19; it does not certify the entire Open5GS checkout.
Open5GS `2.8.0`, 3GPP Release 19 and PFCP header version `1` are independent
version identifiers. Keep this HEAD and the R19 generator input; no downgrade.

S2 permits optional pool information in UP-originated Setup messages with configured
pools, without an explicit UEIP prerequisite. Pool information (233) contains
repeatable identities (177), optional Network Instance (22), repeatable S-NSSAI
(257) and IP Version (258). Identity has an internal two-octet length. UEIP
describes UP allocation; CP allocation remains possible. Optional groups' children
are optional from the receiver's error-handling perspective. Semantic optional
errors are ignored; missing required top-level fields and structural length
errors follow different rules. Rejection responses have reduced requirements,
with Node ID exceptions. Unexpected singleton repetitions use the first instance.
Recovery Timestamp in Setup is ignored for recovery decisions.

Our inference: using actual configured pools with CP allocation is defensible;
advertising arbitrary DNN strings without pools is not the selected mechanism.
The policy below is deliberately narrower than all wire encodings and is not
a claim of certification or full Release 19 compliance. Preserving legacy
selection does not implement every S1 §6.3.3.3 requirement, including required
UPF functionalities received from UDM. The controlled MVP assumes no additional
such requirements; it must not be presented as satisfying those deployments.

| Subject | S2 clause/table; nature | Design decision | Future evidence |
|---|---|---|---|
| Both Setup directions | §6.2.6.2–3; §7.4.4.1–2; optional | UP builders advertise in request and successful response | T05/T06 |
| Pool group | Table 7.4.4.1-3; required/optional children | Preserve nesting and all supported repeated children | T02/T03/T21 |
| Encoding | §8.2.128, §8.2.4, §8.2.176–177 | Length-delimited identity; APN label encoding for this profile | T02/T04 |
| Allocation | §5.21.1–2; mandatory/conditional | CP allocator unchanged; do not set UEIP | T22 |
| Presence and rejection | §7.2.3.2; mandatory handling | Distinct association and capability results | T18/T19/T25 |
| Framing | §7.6.3, §7.6.7; mandatory handling | No out-of-bounds reads; classify framing separately | T04/T25 |
| Optional semantics | §7.6.6, §7.6.8–9; mandatory handling | Continue association processing without unusable information | T19/T25 |
| Repetitions | §7.6.10; mandatory handling | First singleton; bounded lists with explicit capacity status | T03/T04 |
| Erroneous matched responses | §7.6.1, §7.6.3, §7.6.6; mandatory handling | Stop request retransmissions and notify PFCP application even when discarding response | T18/T25 |
| Other R19 conditions/defaults | §7.4.4.1–2; conditional/optional | Check only verifiable conditions; scope pool precedence to 233; preserve other capabilities | T27 |
| Forward compatibility | §7.6.1, §7.6.8–9; mandatory handling | Ignore unknown optional/CO IEs and spare bits without losing known pool data | T04/T27 |

## Decision 1: independent local controls

The following names are **new proposed options**, absent from this HEAD:

```yaml
upf:
  pfcp:
    advertise_ue_ip_pools: true   # proposed, default false
    client:
      smf:
        - address: 127.0.0.4
  session:
    - subnet: 10.48.0.0/16
      gateway: 10.48.0.1
      dnn: edge1
      pool_id: edge1-v4-a        # proposed field
```

SMF independently opts in with proposed `smf.pfcp.learn_upf_dnn: true`
(default false). This changes the source of the DNN comparison only. It is
neither the UPF advertisement switch nor a strict selection switch. SGW-C/SGW-U
do not implicitly opt in through their shared PFCP library. Feature-off parsing
still follows protocol error handling, but does not activate learned mappings.

`pool_id` is an explicit YAML string for each announced `session` entry. Locally
accept 1–64 ASCII bytes from `[A-Za-z0-9._:-]`; require uniqueness across the UPF's
announced entries, including IPv4 and IPv6 entries. These are implementation
limits, not a 64-byte 3GPP restriction. Reject invalid local configuration before
opening PFCP. Skip generic entries without explicit DNN with a diagnostic; they
are not advertised as wildcards. With advertisement disabled, existing
configuration remains valid without identities. Validate the full announcement
before startup, rather than discovering excessive groups during transmission.

The operator persists identities in configuration. Serialize those bytes directly,
never a random value, hash, N4 address, list position, PID or Recovery Timestamp.
Restarting with the same configuration therefore preserves identity. Reordering
entries or changing N4 addresses cannot change it. An explicit identity change
is a valid replacement; the receiver cannot distinguish a deliberate edit from
accidental instability after losing its history. There is no persistence ledger
in this MVP and **no rejection rule for undetectable instability**. While an old
snapshot exists, differences can be logged, without claiming continuity of a
physical pool. Changing a subnet without changing its ID is also undetectable
from this advertisement, which carries no subnet.

The isolated identity contract experiment and the stage 3 real-UPF restart/wire
tests pass (T24). The harness starts fresh processes twice with identical YAML,
then with reordered entries and a changed N4 address, then with an explicit
identity change. It compares decoded identity bytes by pool configuration;
whole PFCP packets differ in sequence numbers and restart timestamps.

## Decision 2: codec, profile and ownership

Correct the generator's substring normalization and explicit repetition rules,
then regenerate from the **existing R19.5.0 DOCX**, both with updated caches and
from empty caches. The normative PDF and generator input now share V19.5.0.
The checked-in DOCX is available; its SHA-256 is in the audit. Stage 2 must
compare the relevant DOCX rows with S2 and keep unrelated R19 definitions intact.

Use 16 groups per message, up to 16 identity IEs and 8 S-NSSAIs per group;
Network Instance and IP Version are singletons. These finite capacities fit the
existing `more16`/`more8` descriptor scheme and are local limits, not wire maxima.
The UPF emits one group and one identity per explicit configured subnet (the
current subnet capacity is 16). The receiver preserves multiple identities per
group; it never silently keeps only the first. The corrected group is also used
by the Association Update descriptor, without implementing that procedure.
Audit other occurrences of the same erroneous identity alias as codec fallout;
the current R19 tables identify Create PDR and Usage Information, rather than
the earlier Traffic Endpoint example. Create PDR permits an ordered IPv4/IPv6
pair, whereas Usage Information remains singleton. Keep separate fixtures and no
allocator changes. The generator must apply repetition per context, not globally
repeat every occurrence of type 177 or 257.

For 177, payload is `uint16_be(identity_length) || identity_bytes`; TLV length is
`2 + identity_length`. Generic framing allows at most 65533 identity bytes in
that IE alone; parent and message limits reduce the usable total. The MVP receive
profile retains up to 64 **opaque** identity bytes, using lengths and `memcmp`,
not C string functions. Longer, otherwise well-formed identities are unsupported
capabilities, not a reason to reject an association. The ASCII rule is only for
our local YAML. Example for `edge1-v4-a`:

```text
00 b1 00 0c 00 0a 65 64 67 65 31 2d 76 34 2d 61
```

For Network Instance choose the same APN label representation used by the
existing PDI builder: `edge1` becomes `05 65 64 67 65 31`, without trailing NUL.
Validate every label, aggregate length (at most the current `OGS_MAX_DNN_LEN`,
100), complete consumption, embedded NULs and output bounds before conversion.
Do not silently truncate with `ogs_min()` or accept arbitrary opaque Network
Instances as DNNs. Preserve received case or normalize separately; comparisons
remain case-insensitive as in legacy. Do not strip an operator identifier by
guessing. Unsupported encodings are unusable mappings under this local profile.

For IP Version, **V4 is bit 1 (`0x01`) and V6 is bit 2 (`0x02`)** of value
octet 5 (S2 §8.2.177). Stage 3 external decoding caught and corrected an
inversion in the new sender before completion.

S-NSSAI (257) has a four-octet value (SST and SD); retain the wire value without
inventing slice defaults. IP Version (258) requires at least its flag octet and
at least one of V4/V6; both set is valid. Emit spare bits as zero and ignore
received spare bits. A well-framed zero-family value is an optional semantic
defect, not structural truncation. T04/T27 cover these distinctions. Do not
reject otherwise valid longer UP Function Features solely because the peer
implements more R19 feature octets; clear local storage, copy the supported
prefix safely and retain its actual stored length.

The wire parser's octet fields borrow packet memory. Decode into a temporary
owned snapshot before dispatch releases `pkbuf`; no pointers into the message or
YAML may survive. Store groups, identity byte arrays, DNN, optional family and
slice lists, source, validity/diagnostic status and a local association generation
separately from `node->dnn[]`. The latter retains its borrowed static YAML lifetime.
Snapshot replacement in the NF event loop is atomic to selectors; free the old
snapshot after swapping. Free snapshots on node destruction, deassociation and
detected restart of the boot for which they were confirmed (see Decision 6).
Builder scratch buffers must survive synchronous serialization
of the entire message and be released on every error path.

The shared TLV parser currently loses excess repeated fields and aborts compound
parsing on errors. Adding arrays alone is insufficient. Stage 2 must expose a
bounded parse/capability status or a PFCP-specific prevalidation result, retaining
the distinction between optional semantic defects and invalid enclosing lengths.
Prefer a PFCP-specific bounded adapter; any generic TLV change requires GTP and
core TLV regression tests. Unknown children must not discard otherwise usable
known fields. Do not turn a recoverable optional defect into a generic NULL
message that bypasses association handling.

Stage 2 implemented this as `pool-codec.[ch]`, used by generated Setup
Request/Response and Update Request decoding. At that stage, messages containing
233 owned a filtered body until message-free; decoded octets borrowed that body.
Other messages retained the existing borrowed-packet lifetime. All valid groups survive
optional semantic defects in independent groups; local overflow exposes no
truncated pool list. That was codec state only. Stage 3 now owns peer snapshots
and gates association acceptance. It also filters known optional scalar defects and unknown
children recursively; every Setup now owns its filtered body, even without 233.
Association Update without 233 and other messages retain their previous lifetime.
See the reports for executed fixtures, boundaries, regeneration and sanitizer evidence.

## Decision 3: association result is independent of capability result

Association processing returns an explicit association outcome and a separate
capability outcome; a single boolean must not mean both. Validation precedes
`Request Accepted`, including Node ID, required fields, framing and checkable
conditions. Only a successfully prepared/committed response permits initial
transition; propagate send failures. Reset/copy function features with their
actual validated lengths, rather than keeping bytes from an earlier association.

For responses, correlate the outstanding transaction, finish transaction handling
even for an erroneous reply, then check Cause presence/value. In particular,
S2 §7.6.1/3/6 require stopping request retransmissions and notifying the PFCP
application for matched malformed responses, including inconsistent message
length and missing mandatory/checkable conditional fields. This must work even
when parsing or Node ID extraction fails before the current handler; a generic
discard that leaves the transaction retrying is insufficient. Do not complete
an unrelated transaction from an unsolicited response or a truncated header
that cannot safely identify a transaction. A rejection never
publishes capabilities or establishes an association. For accepted responses,
check the remaining required fields. Do not demand success-only fields from a
rejection. Setup Node ID is still required; fix malformed error responses at the
sender, rather than weakening Node ID validation. Generic Setup error building now includes it, covered by regression checks.
The reduced error response includes Node ID and Cause. Table 7.4.4.2-1
has no Offending IE member; applying §7.2.3.2's message-specific exception
does not add an unlisted member to the generated Setup schema. The generic
wording in §7.6.7–8 about identifying the offending IE is interpreted subject
to that response table; this is recorded explicitly rather than claiming a
new table entry. Recovery Timestamp presence/size
validation is distinct from restart detection; do not derive generations or
pool stability from its Setup value.

Apply R19 presence conditions individually. Setup UPF Instance ID is conditional
when a 5G UP function sends the message and the ID is available; absence alone
does not prove the remote availability condition. No available UPF instance-ID
source was found in the audited UPF initialization/builders; do not generate one
from `pool_id` or add NRF registration to this feature. Recheck availability in
stage 3: there is still no available identity source. N6 Delay Measurement
Protocols is listed as conditional on advertised UN6DM, but the Setup table
uses **should**, so absence generates a diagnostic rather than mandatory
rejection. The raw validator can inspect octet 14 bit 2 beyond the legacy
feature struct; this UPF does not advertise UN6DM. Do not activate unrelated capability flags.
The precedence rule below concerns only pool IE 233. Other optional IEs retain
their own R19 defaults, including Packet Inspection functionality; it would be
incorrect to apply the pool absence policy to all optional capabilities.

| Received condition | Association result | Capability treatment (local policy) |
|---|---|---|
| Valid Setup, no 233 | Accept | New `ABSENT` snapshot; clear old learned data; static fallback |
| Valid supported groups | Accept | `VALID` snapshot; all usable groups committed together |
| Valid groups with no usable DNN, or only supported-format slice-restricted groups | Accept | `UNUSABLE` snapshot; no learned DNN match |
| Semantically bad optional child/group, such as absent identity or bad DNN labels | Continue processing valid Setup | Ignore defective content; exclude affected group; usable independent groups survive in one `DEGRADED` snapshot |
| Malformed family/slice restriction | Continue processing valid Setup | Exclude affected group, never erase its restriction and broaden it |
| Conflicting reuse of an identity in different contexts | Accept | Omit conflicting groups with diagnostic; exact duplicate records deduplicate |
| Unsupported local capacity/identity size | Accept if framing/required fields are valid | `UNSUPPORTED` snapshot; no truncated learned list; clear previous learned list |
| Repeated singleton NI/IP version | Continue | First value governs; ignore later instances, including conflicting ones |
| Missing/invalid required top-level IE | Reject request / fail response | No new generation or learned publication |
| Structural message/IE length failure | Apply §7.6.3/7.6.7 request/response handling | No parsing beyond bounds or partial publication |
| Cause absent, malformed or non-accepted | Fail response | No new generation or learned publication |

Invalidity is observable locally even where the protocol handles a defective IE
as absent. That diagnostic state is not a new wire status or a rejection Cause.
For learning-enabled peers, any observed 233 disables automatic static fallback
for that accepted generation: use only usable learned groups (possibly none).
This conservative local precedence avoids reviving a contradicted static mapping.
It does **not** prevent location matching or legacy fallback. True wire absence
alone restores the static list. There is no explicit-empty wire marker.

In `will_associate`, gate transition on association success. In `associated`,
successful Setup replaces the snapshot and increments a local generation even
if contents are identical; retransmissions handled by the transaction layer do
not create extra generations. A rejected incoming reassociation does not overwrite
the existing association or snapshot. A failed outgoing replacement attempt cannot
establish a new generation; preserve an old association only if still live and
not already invalidated by timeout/restart. Unsolicited/late responses cannot
restore cleared data. Test all these branches in SMF, UPF and shared EPC FSMs.
Do not alter session restoration/retention behavior in this feature.
S2 §6.2.6.2.2 also specifies session handling on an accepted existing-Node-ID
Setup. This audit has not proved the baseline's full retention/deletion behavior;
record it as a separate interoperability limitation, not as continuity provided
by the learned snapshot. T11/T15 must distinguish pool replacement, adding a new
peer and re-establishing the same peer.

## Decision 4: effective mapping and the exact legacy limitation

Feature off uses static DNNs. Feature on uses static DNNs only for `ABSENT`, or
the projection of usable learned groups otherwise; never union contradictory
lists. Keep the configured list for diagnostics and future absence. Log conflicts.

To avoid flattening restrictions, the learned DNN comparison evaluates group
context: exclude groups with any S-NSSAI from this MVP's DNN arm. For explicit
family flags, require coverage of the session's requested IPv4/IPv6 families;
the union of unrestricted groups for the same DNN may cover dual stack. A missing
IP Version remains unknown and is not treated as a restriction (similar to a
static DNN mapping), nor presented as proof of dual-stack support. Unsupported
PDU types get no learned DNN match. This is interpretation of learned metadata,
not a new global eligibility filter. Do not flatten learned data into a universal
string array and lose the context.

Preserve DNN **OR** location matching, rotation, `no_pfcp_rr_select` and fallback.
The actual HEAD also returns the **first list element** after its searches fail,
even when it is not associated, or RR fallback is disabled. Thus no absolute
promise of DNN compatibility or unavailable-peer exclusion is possible in 4A.
The initial-success demonstration requires associated compatible peers and no
competing location match. T07/T08 must say so. T12 uses a live alternative and
separately characterizes the all-unavailable case. No-candidate handling and
strict DNN exclusion belong to unrequested 4B or a separately authorized fix.

## Decision 5: address pools and routing

Different DNNs may use different SMF pools: `internet`/10.45.0.0/16 on A and
`edge1`/10.48.0.0/16 on B are compatible with existing DNN-based allocation before
selection. Do not exclude this supported topology from the MVP.

Multiple candidate UPFs **for the same DNN/family** must all serve every address
the SMF can allocate for that DNN/family. Disjoint, incompatible pools on B and C
for `edge1` cannot be made safe by mapping learning: in the normal 5GC path the
address was allocated before selection, and the selector does not consult pool
identities. Same ID does not prove same prefix; different IDs do not prove
incompatibility. Group 233 does not convey the subnet, ranges or N6 reachability,
so this cannot be detected automatically from these metadata.

Use one allocator and verified per-session return routing/NAT in the initial
same-DNN experiment. Overlapping addresses across independent allocators require
coordination, outside scope. T22 must measure address uniqueness and ownership
of the return path. Test an incompatible-pool topology as a documented negative
case, not an automatic association rejection or promised success. Allocator/pool
selection changes would require a scope revision. Existing EPC callers select
before allocating; do not generalize the 5GC ordering to them or reorder them.

## Decision 6: replacement Setup and Heartbeat confirmation

Correction authorized on 2026-11-04. A newly accepted Setup replaces the old
owned snapshot immediately. It starts without Heartbeat confirmation. A
successful response to a Heartbeat sent in that snapshot's current association
epoch binds the replacement to the Heartbeat recovery value. Requests and
responses to pre-Setup probes cannot establish that binding. Commit success
and capture-before-free are required for confirmation.

Late restart detection must not delete the fresh replacement because the
baseline Heartbeat history still refers to an older boot. A snapshot confirmed
for an older recovery value is still invalidated by a later higher Heartbeat
value, including without a state exit. Delayed lower values cannot invalidate
a confirmed newer snapshot. Timeout, deassociation and destruction still clear
both confirmed and not-yet-confirmed snapshots; a rejected Setup cannot reset
confirmation. Binding does not change eligibility or wait for a deployment.

Setup Recovery Timestamp remains ignored. That first fix neither seeds
`remote_recovery` from Setup nor changes baseline restoration decisions or
initiates extra Setup procedures. The active CP's existing restart transition
may still re-associate and replace a snapshot again. See the correction report
for both initiators, stale-probe tests, packet evidence and retained limitations.
This supersedes the previously documented introduced invalidation limit, not
the independent SQN holding-window or Session Establishment error defects.

## Decision 7: renegotiate after one-sided association loss

Separately authorized on 2026-11-04 after reproducing the case with real SMF
and UPF. Heartbeat Responses must still be sent without an association
(S2 §6.2.2.2); receipt of Heartbeat alone does not publish learned capabilities.
On entry to `will_associate`, SMF and UPF promote a previously accepted passive
peer to an active Setup initiator with the existing retry timer. A generation
of zero does not trigger this promotion. Accepted Setup replaces owned metadata;
timeout clears it; failures and unsolicited/obsolete responses cannot republish.
Both original initiators and simultaneous recovery are supported locally.

CP recovery Setup explicitly requests retention of all sessions using empty
IE 183 (S2 §6.2.6.2.1–2, table 7.4.4.1-2). An already-associated UP endpoint
acknowledges that request with IE 184 PSREI bit 1 (§8.2.134). Initial Setup
does not claim retention. A PFCP-specific adapter handles the valid empty
compound; generic TLV/GTP behavior and generated descriptors remain unchanged.
Selective/nonempty retention is unsupported optional information: ignore it,
never broaden it to retain-all or acknowledge it. The first singleton instance
governs; nested framing errors remain errors (§7.6.8–10).

This supersedes the previous scope restriction on additional Setup procedures.
It preserves the baseline policy for sessions on a failed peer and does not
promise migration or universal retention. UP-initiated recovery requires remote
support; permanent connectivity loss cannot be solved by association timers.
Legacy selection/fallback and allocation stay unchanged. Decision 6's epoch
and post-Setup Heartbeat confirmation guards remain required.

Discarding a Setup before dispatch must release events through the allocator
that created them: UPF/SGW-C/SGW-U events return to their NF pools; SMF events
use the heap. A simultaneous-recovery regression exposed and corrected this
introduced ownership defect. Independent unmatched-response injection and
normal/instrumented recovery now exercise that path. See the report for actual
NAS first-request evidence, raw FAILs, the narrowly suppressed baseline talloc
startup leak and remaining NOT RUN boundaries.

## Change map and next-stage gates

| Stage | Files/areas | Required result |
|---|---|---|
| 2 | `lib/pfcp/support/pfcp-tlv.py`, caches, generated `message.[ch]`, PFCP decoding adapter/`types.[ch]`, unit registration | Correct 233/177 types, context-specific repetition, bounded status, length helpers, independent byte fixtures; cache/fresh regeneration agree |
| 3 | `lib/pfcp/context.[ch]`, `build.c`, `handler.[ch]`, `path.c`, `src/upf/context.[ch]`, example YAML | Independent advertisement/learning controls, validated owned snapshots; no selection change yet |
| 3 | `src/{smf,upf,sgwc,sgwu}/pfcp-sm.c`, receive/transaction error paths | Association result gating in both states/directions, Cause checks, cleanup; no optional-error blanket rejection |
| 4A | `src/smf/context.c` and focused tests/logs | Learned context-aware DNN arm with unchanged OR/fallback/rotation |
| Later validation | `tests/unit`, `tests/core`, isolated PFCP/FSM and integration harness | Both initiators, reassociation, static/learned precedence, memory lifetimes, EPC regressions |

Stage 1 ended with documentation only. Stage 2 was subsequently authorized and
completed using this HEAD, the contract, pinned generator input and an isolated
dependency-complete build. Its exit evidence passed: independent request/response
fixtures, malformed/optional distinctions, multiplicity/overflow cases,
reproducible regeneration without unrelated codec drift, R19 T27 codec fixtures
and relevant builds/tests. An external decoder was available for the target IEs.
See the stage 2 report for evidence and limits. Full handler/transaction R19
behavior is not implied by this result. T18/T25 transaction and T27 handler
presence/default tests continue in stage 3, which requires a new instruction.
Stage 3 was subsequently authorized and completed. See its report for the
change map, actual PASS/FAIL/NOT RUN results, decoder verification and commands.
Accepted Setup publishes a separate owned snapshot; successful publication and
invalidation also advance a local epoch. Outgoing Setup transactions capture
that epoch, so obsolete responses cannot publish after restart/invalidation or
an intervening accepted Setup. `association_generation` counts accepted Setups;
transport retransmissions do not increment it.

Invalidation sets the snapshot pointer to NULL; it is **not** an accepted
`ABSENT` snapshot. Stage 4A must preserve this distinction, particularly in the
passive restoration path that remains associated. Only an accepted no-233
Setup provides ABSENT evidence for future static fallback.

Two observed procedure limits remain: a restarted UPF can reuse a SQN inside
the previous request's holding window and be treated as a retransmission;
and a later Heartbeat restart detection invalidates even a newly published
snapshot. A passive SMF restores sessions using the existing path without
automatically negotiating another Setup. A subsequent Setup relearns metadata.
Do not reset Heartbeat history from a Setup timestamp or add an unrequested
Setup that could affect session retention to conceal either limit.

Stage 4A was subsequently authorized and completed on 2026-10-11. The public
SMF selector now reads the owned snapshot directly. Feature off uses static;
only an accepted ABSENT snapshot restores static while learning is enabled.
NULL invalidation yields no DNN match. Family coverage uses the effective
`sess->session.session_type` at selection, including prior allocator downgrades;
unknown/type 0, unstructured and Ethernet produce no learned DNN match. The
S5-C caller currently selects before setting that type; its ordering was not
changed and this limitation is explicitly tested/documented. Location and
fallback still apply to every such case.

The test-only driver runs on the actual SMF poll thread, with real PFCP
association/FSM, session objects, allocator and selector. It does not implement
a production API or simulate a UE establishment procedure. Six ABTS cases,
6,144 legacy policy comparisons, sanitizers and 14 isolated runtime scenarios
passed. See the stage 4A report for the precise selection-boundary evidence,
source precedence, retained allocated-session ownership and untested UE/N6
continuity. Both stage 3 restart/cache limits remain; no automatic renegotiation,
retention fix or strict fallback change was added. No stage advances automatically.

Stage 5 was subsequently authorized and executed on 2026-10-11–12. Actual
NAS/NGAP sessions and packet captures prove the first Session Establishment
destinations A/internet, B/edge1, C/edge1, B, C without peer mappings/location
matches, plus established A-session data after additions. A secondary
CP-initiated case proves learned precedence over deliberately conflicting
static DNNs. The allocator remains operator-configured and unchanged.

The ordinary-return NAS test **fails**: returned C's generation 2 is invalidated
by the subsequent Heartbeat restart detection, and the next first request
goes to B. Explicit later Setup restoration is a separate passing boundary
case. This confirms the stage 3 limit rather than resolving it. Separately,
an incompatible same-DNN CIDR can accept a CP-allocated out-of-subnet address
and fail normal data return; the protocol advertisement is not a CIDR
compatibility detector. Neither result justifies a new strict selector policy.

The no-match NAS test exposes the independent baseline Session Establishment
error builder's missing Node ID. A wire reproducer produces identical defective
bytes against clean HEAD and modified UPF. The parser was not relaxed and no
unrequested protocol correction was mixed into the feature. This is a FAIL
protocol result despite successful reproduction of the defect.

Fresh complete build, selected 5GC/EPC regressions, directed memory tests and
actual local runtime image startup/association pass. Runtime images identify
HEAD plus dirty core hash and are saved locally. Cluster fixtures, evidence
collection and UID-guarded rollback are prepared, not deployed. These findings
leave the recovery/error gates open; image-backed full NAS, learned IPv6/slice
NAS, external N6 and cluster behavior remain unexecuted. Stage 5 does not change
production core behavior, authorize stage 4B/6 or migrate sessions.
