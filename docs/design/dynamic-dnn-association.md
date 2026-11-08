# Stage 3: PFCP pool advertisement and owned learning

Current project status: [final review](dynamic-dnn-final-review.md) and
[operation guide](dynamic-dnn.md). This dated report preserves the results and
limits at its execution date; later corrections/validation do not rewrite it.

Executed on 2026-10-08 (Europe/Lisbon). Scope: Prompt 3 in
[the plan](../../OPEN5GS_DYNAMIC_DNN_PLAN.md), using
[ADR-001](ADR-001-dynamic-dnn-learning.md) and the
[stage 2 codec](dynamic-dnn-codec.md). **Stage 3 is complete for this scope.**
The selector is unchanged and stage 4A has not started.
Subsequently authorized stage 4A: [selector integration and evidence](dynamic-dnn-selection.md).

## Checkout and isolation

HEAD remained `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, branch `main`,
application version `2.8.0`, with no HEAD tag. The stage 2 working changes and
untracked plan/design/tests were present at entry. They were backed up before
editing; no checkout, version switch, commit, push or deployment occurred.
The compilation baseline in this stage is **HEAD plus inherited stage 2 work**,
not a newly claimed clean-HEAD build. The stage 2 report contains its original
clean-HEAD evidence.

Build/source trees were outside the repository at `/tmp/open5gs-stage3`.
The pre-existing Ubuntu 24.04 image `open5gs-pfcp-codec-build:20261007` has ID
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`.
It provides GCC 13.3.0, Meson 1.3.2, Ninja 1.11.1 and TShark 4.2.2.
Dependency/source setup initially fetched the same Meson subprojects as stage 2;
compilation and tests then used `--network none`. No dependencies were installed
on the host. Runtime tests used loopback inside disposable containers, with
`NET_ADMIN` and `/dev/net/tun` confined to those containers. The lab TUN name was
`pooltest`; no production interface, NRF, subscriber, UE or cluster was used.
The isolated SMF deliberately had no reachable NRF or Diameter configuration;
expected connection/configuration diagnostics do not prevent PFCP startup.

Durable evidence is in `/home/marujo/open5gs_stage3_results_20261008`:

- `initial/`: entry working-file copies, hash manifest and inherited diff.
- `stage3-only.diff`: changes relative to that entry state, separating stage 3
  from the inherited stage 2 diff shown by ordinary `git diff`.
- `tested-source-sha256.json`, `versions.json`, `git-status.txt` and
  `unchanged-stage2-generation-and-selector.json`.
- `baseline-build.log`, `baseline-tests.log`, `stage3-build.log`,
  `stage3-tests.log`, `meson-testlog.txt`, sanitizer logs and intermediate logs.
- `runtime-verified/`: final NF YAMLs, logs, identity hex captures, identity
  comparison JSON, `results.json` and `association.pcapng`.
- `config-verified/`: 15 startup/configuration cases and `results.json`.
- `tshark-verified.pdml`, `tshark-verified-pools.tsv`, `tshark-version.log` and
  `decoder-verified.json`; `artifact-sha256.json` checks the evidence files.

## Implementation and change map

| Area | Change and purpose |
|---|---|
| `lib/pfcp/context.[ch]` | Parse independent role-specific boolean controls; store owned explicit subnet identity; reject invalid announcement configuration before PFCP opens; gracefully reject more than 16 subnets; free learned metadata on node destruction |
| `lib/pfcp/association.[ch]` | Validate local identities/DNNs; construct groups using caller-owned serialization scratch; prepare bounded owned snapshots; deduplicate identities/records, exclude ambiguous contexts, publish/clear atomically; validate raw Setup and correlate error responses |
| `lib/pfcp/build.c` | UP Setup Request and accepted Setup Response advertise configured groups; rejected responses contain Node ID and Cause with no success-only metadata |
| `lib/pfcp/pool-codec.[ch]` | Keep stage 2 optional-group classification; recursively check known grouped framing, filter unknown children and invalid optional scalar lengths; preserve first-singleton semantics and supported CP-feature prefix; define the correct V4/V6 wire bits |
| `lib/pfcp/handler.[ch]` | Validate before Accepted; prepare candidate before send; publish only on successful response commit; check response Cause and local epoch; reset feature storage before bounded prefix copy; invalidate on Heartbeat restart |
| `lib/pfcp/path.c`, `xact.[ch]` | Permit 8-byte non-SEID headers; defer Setup errors for safe correlation; notify application on matched erroneous replies, including unsupported version; propagate Setup send failure and delete failed cached transactions; reject unmatched/obsolete Setup responses |
| `src/{smf,upf,sgwc,sgwu}/pfcp-path.c` | Prevalidate before parser/Node-ID lookup/merge; Setup responses cannot discover a new peer |
| `src/{smf,upf,sgwc,sgwu}/pfcp-sm.c` | Gate initial association on handler success; consume failed replacement outcomes while preserving a live association; clear on associated-state exit; SMF clears before unavailable-peer handling |
| Example UPF/SMF YAML | Document opt-ins and explicit persistent pool IDs, retaining defaults |
| Unit registration and `pfcp-association-test.c` | Seven ABTS cases with ownership, association, transaction, announcement and negative subcases |
| `tests/pfcp/*.py` | Reproducible real-NF smoke tests, actual UPF configuration tests and external PDML semantic verification |
| Sanitizer runner | Optional association suite using the same actual C tests; existing codec invocation remains supported |

Generated `message.[ch]`, generator, caches and the tracked R19 DOCX were not
changed from the stage 2 entry state. Their inherited fixes remain intact.
There was no regeneration needed for this handwritten stage. All affected shared
library callers, including the two SGW roles, were compiled. The SGWs do not
implicitly opt into advertisement or learning. `src/smf/context.c`, including
`compare_ue_info` and `selected_upf_node`, and the normal 5GC allocation/selection
callers are unchanged from HEAD.

## Configuration and wire contract

Both controls default to false and accept explicit YAML `true`/`false`:

```yaml
upf:
  pfcp:
    advertise_ue_ip_pools: true
    server:
      - address: 127.0.0.7
    client:
      smf:
        - address: 127.0.0.4
  session:
    - subnet: 10.48.0.0/24
      gateway: 10.48.0.1
      dnn: edge1
      pool_id: edge1-v4-a
    - subnet: 2001:db8:48::/64
      gateway: 2001:db8:48::1
      dnn: edge1
      pool_id: edge1-v6-a
```

```yaml
smf:
  pfcp:
    learn_upf_dnn: true
    server:
      - address: 127.0.0.4
```

These fragments augment normal NF configuration. Pool IDs are unique across
announced entries, 1–64 ASCII bytes `[A-Za-z0-9._:-]`, copied from YAML into
owned subnet storage. Oversized values are flagged before bounded copying can
hide truncation. DNN labels must fit the existing APN-label profile without
silent truncation. Generic sessions without a DNN are skipped with a diagnostic;
they are not wildcard announcements. Feature-off configurations need no IDs.
The switch for the other NF role is not activated by the shared parser.

Each explicit subnet emits one 233 group with one length-prefixed 177 identity,
APN-label Network Instance 22 and IP Version 258. **V4 is `0x01`; V6 is `0x02`.**
No subnet/range, invented slice, UPF NF identity or allocation-support feature is
fabricated. Scratch data survives synchronous encoding; the serialized packet
owns its bytes thereafter. The two independent real-NF initiator scenarios prove
announcement in both Setup message directions.

The SMF never creates DNN services or allocators from announcements. No learned
metadata is passed into the selector in this stage.

## Association, optional information and lifetime

The protocol result is the handler's success/failure; capability results remain
separate (`ABSENT=0`, `VALID=1`, `DEGRADED=2`, `UNUSABLE=3`, `UNSUPPORTED=4`).
Raw validation checks enclosing and known-group framing, first Node ID, first
Recovery Timestamp, Cause and verifiable SMF-set conditions before sending
Accepted or updating peer identity. Required fields are validated even with
learning disabled. Received Setup timestamps do not drive restart detection.
Rejected responses do not need success-only Recovery Timestamp/features.

Optional semantic defects exclude affected groups; independent usable groups
survive in a DEGRADED snapshot. Invalid family/slice restrictions exclude the
group rather than broadening it. Overflow exposes no truncated pool list.
Received identities remain opaque byte arrays with explicit lengths. DNNs,
families, optional-family presence and four-byte slice values remain grouped.
Slice-restricted groups are retained but marked unusable for the planned local
DNN projection. Missing family remains unknown, not evidence for both families.
Missing NI is distinct from absence of the announcement. Exact duplicate records
and identities deduplicate; conflicting identity reuse in distinct contexts
excludes both contexts, without choosing an iteration-order winner.

The candidate is fully owned before send/publication. On successful send/commit,
the event loop swaps the pointer and frees the old snapshot. Failed incoming
replacements, rejected replies and failed sends preserve a previous live snapshot.
Accepted no-233 Setup publishes ABSENT and removes the old learned list. Invalid
optional capabilities publish their diagnostic result and do not retain stale
lists. Neither case rewrites the static borrowed `node->dnn[]` array.

Accepted Setup advances `association_generation`. Publication and invalidation
also advance an independent local epoch; an outgoing Setup captures that epoch.
An intervening acceptance or invalidation makes its reply obsolete. Clearing
sets the pointer to NULL, **which is not ABSENT evidence**. Retransmissions of
one request are handled by the transaction cache and do not republish.

Matched erroneous responses stop/delete their request transaction and invoke its
application callback even when raw length/Node-ID validation would have prevented
handler dispatch. Unmatched responses and unidentifiable short headers do not
complete another transaction. Unsupported Setup response versions retain the
Version Not Supported response behavior and also notify the matched application.
Setup send failure returns an error and removes the transaction so a retry
cannot send a cached Accepted without publishing. Initial FSM transition requires
success in all four NF roles; rejected reassociation does not tear down a live
association. No late reply can republish after the epoch changes.

Snapshots are freed on replacement, node destruction, associated-state exit,
unavailability and detected Heartbeat restart, including passive restoration
without state exit. Existing session recovery/reselection code is preserved.

## Release 19 decisions and source boundary

The pinned references remain
[TS 29.244 V19.5.0](https://www.etsi.org/deliver/etsi_ts/129200_129299/129244/19.05.00_60/ts_129244v190500p.pdf)
and
[TS 23.501 V19.5.0](https://www.etsi.org/deliver/etsi_ts/123500_123599/123501/19.05.00_60/ts_123501v190500p.pdf).
The tracked generator DOCX is unchanged (SHA-256
`1a5cfaf2c1ef67a52f199d2aca1a5cb0e65606e28fa64a7063f82375912c4ab0`).
The relevant table cells and §7.2.3.2, §7.6 and §8.2.177 were rechecked against
that source and the official PDF. This is scoped implementation evidence, not
whole-core Release 19 certification.

- Optional-group children use optional error treatment; semantic defects do not
  automatically reject the association. Structural length defects remain errors.
- The reduced Setup rejection follows §7.2.3.2 and table 7.4.4.2-1: Node ID plus
  Cause. That table has no Offending IE member. The generic §7.6.7–8 wording about
  identifying the offending IE is interpreted subject to the message-specific
  exception/table; no invented generated Setup member was added. This
  interpretation is explicit for review, rather than claiming a table change.
- CP MPAS is checkable from its advertised feature prefix. Missing SMF Set ID
  fails the request/response; malformed first SMF-set FQDN is incorrect. A later
  singleton cannot repair the first value.
- UN6DM is octet 14 bit 2. The Setup table recommends N6 protocols with **should**;
  missing protocols produce a diagnostic, not a mandatory rejection. No N6
  measurement functionality is implemented or advertised here.
- UPF initialization still provides no available NF Instance ID: no SBI identity
  source, UUID initialization or registration was found in `src/upf` or the UP
  builders. Receiver absence cannot establish remote availability. No ID is
  manufactured from pool identity, address or timestamp.
- Packet Inspection is not consumed by the existing selector/handlers. Its
  existing behavior is preserved; no pool-style absence/deny policy is applied
  to that independent optional capability. Inspection capability negotiation
  and full R19 feature support are not claimed.

## Executed results

| Check | Result | Evidence / limit |
|---|---|---|
| Inherited stage 2 baseline: four NF targets, core and unit | PASS | `baseline-build.log`, `baseline-tests.log` |
| Final SMF, UPF, SGW-C, SGW-U and unit compilation | PASS | `stage3-build.log`; no final compiler diagnostics in changed code |
| Final `meson test core unit` | PASS | 2/2, no failures/skips; actual unit report includes the new suite |
| Seven association ABTS cases, with subcases | PASS | Both CP/UP handlers, both initiators, repeated/rejected/absent/failed-send replacement, static preservation, epoch-obsolete replies, owned lifetime, destruction and Heartbeat restart |
| 13 erroneous-response variants, with/without existing snapshot | PASS | Mandatory/presence/length/Cause/conditional/version errors; callback count, transaction deletion, no generation change; unmatched/short-header requests remain outstanding |
| Existing eight codec ABTS cases | PASS | Independent fixtures, nesting/types/multiplicity, capacities, malformed inputs and optional/unknown handling retained |
| ASan + UBSan, codec and association suites | PASS | Same C tests; actual instrumented PFCP/core/app libraries; leak detection and halt-on-error enabled |
| Actual UPF startup/configuration scenarios | PASS | 15/15: missing/empty/duplicate/65-byte/bad-ASCII IDs, DNN limits/labels, invalid boolean, feature-off, generic skip, wrong-role switch, 64-byte identity, 16 groups, rejection of 17 groups |
| Actual SMF + UPF, both initiators | PASS | Source 5 and 6, two owned groups/DNN/families, associated state |
| UPF restart with same live SMF | PASS, scoped | New accepted snapshot after transaction holding window; SMF PID unchanged; no UE-session continuity claim |
| Actual unavailable-peer path | PASS | Timeout, learned invalidation and associated-state exit in SMF log |
| Actual SMF request errors/optional/absence/restart | PASS | Wire rejection with Node ID/Cause, DEGRADED/ABSENT replacement, malformed framing with no new generation, passive Heartbeat invalidation and subsequent relearning |
| Actual matched erroneous Setup response | PASS | Error callback log, no association/publication and no retransmission of that SQN during 1.5 s (> three T1 expirations, before next association attempt) |
| Pool identity stability (T24) | PASS | Fresh real UPF processes: same YAML twice, reversed entries + changed N4, explicit ID edit; compare decoded records rather than whole packets |
| Independent final decoder | PASS, scoped | TShark 4.2.2 verifies 14 emitted groups across types 5/6, exact ID lengths/DNNs, V4/V6 association and UEIP/SSET/MPAS unset |
| Generated-source/caches and selector/allocator preservation | PASS | Hash comparison with entry state; no selector/allocation diff |
| Selection and allocator compatibility in UE sessions | NOT RUN | Stage 4A is not authorized; no UE/session/N6 experiment in stage 3 |
| Strict policy, API/SUPI, orchestration, deployments/migration | NOT RUN | Outside scope |
| Live SGW interoperability, Association Update, same-peer session retention, full R19 conformance | NOT RUN | Builds and shared handlers covered; no such runtime/certification claim |

The final PCAP intentionally includes one malformed negative fixture, frame 28.
No positive UPF announcement is malformed. The independent decoder verifier
checks groups and semantics, not merely successful command exit.

Intermediate FAIL results were resolved before completion: initial C compilation
had a missing format macro and descriptor cast; a test used the old node factory
signature; runtime readiness searched for the wrong log string; the timer fixture
used 1 ms instead of 1000 ms; the restart fixture initially reused SQN 1 inside
the holding window; a timeout assertion confused earlier restart invalidation
with state exit. Most significantly, external decoding found V4/V6 inverted in
the new sender after the initial internal tests had passed. The sender, C/Python
semantic checks and final capture were corrected; the independent verifier now
passes. `decoder-initial.log` and the initial PDML preserve that negative evidence.
There are no unresolved test FAIL results.

## Reproduction

Use the dependency-complete image described above. First materialize an external
source copy of HEAD plus the working diff/untracked sources; do not run Meson
in the live checkout. Commands below run **inside** the build container, with
`/artifacts` mounted from that external directory:

```sh
meson setup /artifacts/build /artifacts/source --prefix=/artifacts/install -Dbuildtype=debug -Dwerror=false
ninja -C /artifacts/build -j 6 tests/core/core tests/unit/unit src/smf/open5gs-smfd src/upf/open5gs-upfd src/sgwc/open5gs-sgwcd src/sgwu/open5gs-sgwud
meson test -C /artifacts/build core unit --print-errorlogs
meson setup /artifacts/sanitize-build /artifacts/source --prefix=/artifacts/sanitize-install --wrap-mode=nofallback -Db_sanitize=address,undefined
ninja -C /artifacts/sanitize-build -j 6 lib/pfcp/libogspfcp.so.2.8.0
python3 tests/unit/pfcp-pool-sanitizer.py /artifacts/sanitize-build /artifacts/asan-codec
python3 tests/unit/pfcp-pool-sanitizer.py --association /artifacts/sanitize-build /artifacts/asan-association
```

The runtime commands additionally need container-local `NET_ADMIN` and TUN:

```sh
python3 tests/pfcp/dynamic-pools-smoke.py /artifacts/build /artifacts/runtime-verified
python3 tests/pfcp/dynamic-pools-config-test.py /artifacts/build/src/upf/open5gs-upfd /artifacts/runtime-verified/up-initiated-upf.yaml /artifacts/config-verified
tshark -r /artifacts/runtime-verified/association.pcapng -T pdml > /artifacts/tshark-verified.pdml
python3 tests/pfcp/verify-pool-pdml.py /artifacts/tshark-verified.pdml /artifacts/decoder-verified.json
```

Scripts must run in a disposable network namespace with no external networking,
not on a host or production network. Build commands do not install or start NFs.
The capture contains only synthetic lab addresses/data.

Evidence SHA-256:

| File | SHA-256 |
|---|---|
| Final `association.pcapng` | `94d247c4f0ff15e3f633f7d1970eeea02d51b5fe230287909950fda099f95429` |
| `decoder-verified.json` | `c2f447ec52d1e367454740c25a9444db4f4c0434395001e63dd8d43176c0f98e` |
| `tested-source-sha256.json` | `f9c1b2cad9bcd65e546a5c06ee1204e4123c0a00fc80b1b480859871dca53d61` |

## Remaining limits and the next gate

A restarted UPF starts its SQN counter again. Reuse within the old Setup holding
window is indistinguishable from a transport retransmission to this cache and
will not republish; the restart test waits beyond that window. This stage does
not replace the transaction duplicate-detection algorithm or invent pool
instability detection.

A later Heartbeat can reveal restart **after** an incoming Setup was accepted.
It invalidates that snapshot too. In the passive SMF restoration path the FSM
remains associated and existing recovery runs without automatically requesting
another Setup. The owned list therefore remains invalidated until a subsequent
accepted Setup; the actual SMF mock test proves relearning in that state.
Resetting Heartbeat history from Setup timestamps would violate the chosen
contract; automatically sending a new Setup could affect session retention.
Neither workaround was added. Session retention/deletion on existing-Node-ID
Setup remains the baseline limitation recorded by the ADR.

Pool IDs convey no subnet, routing, NAT or address uniqueness evidence. Matching
IDs do not prove same-DNN pool compatibility, and different IDs do not prove
incompatibility. This stage establishes no N6 forwarding or UE session outcome.
There is no YAML hot reload, persisted receiver ledger, full Association Update
or cross-release/full-R19 capability certification.

To proceed to **stage 4A**, obtain a new explicit instruction and use the tested
source/ADR as baseline. Wire the snapshot into the legacy DNN arm only, preserving
DNN **OR** location, existing fallback/rotation and allocator ordering. Tests must
separate an accepted ABSENT snapshot from NULL invalidation, enforce conservative
family/slice context, cover static/learned precedence and both restart/cache
limits above, and characterize location/fallback results without asserting strict
DNN enforcement. Stage 4B and session/N6 validation remain separate work.
