# Dynamic DNN learning: Release 19 review

Reviewed on 2026-10-07 (Europe/Lisbon). Documentation revision of stage 1 only;
no implementation or interoperability pass. Contract: [ADR-001](ADR-001-dynamic-dnn-learning.md).
Baseline and executed checks: [audit](dynamic-dnn-audit.md).
Subsequently authorized stage 2 completed the codec portion of this review:
[implementation and actual test results](dynamic-dnn-codec.md). Handler conditions,
transactions, advertisement and learning were subsequently implemented in
stage 3 ([results and limits](dynamic-dnn-association.md)). Stage 4A subsequently
integrated learned DNNs with legacy selection ([results and limits](dynamic-dnn-selection.md)).
The original source-review table below does not itself report those procedures as passed.

## Pinned reference and checkout

- [S1: TS 23.501 V19.5.0, Release 19, ETSI January 2026](https://www.etsi.org/deliver/etsi_ts/123500_123599/123501/19.05.00_60/ts_123501v190500p.pdf).
- [S2: TS 29.244 V19.5.0, Release 19, ETSI March 2026](https://www.etsi.org/deliver/etsi_ts/129200_129299/129244/19.05.00_60/ts_129244v190500p.pdf).
- Local generator input: `lib/pfcp/support/r19.5.0/29244-j50.docx`, SHA-256
  `1a5cfaf2c1ef67a52f199d2aca1a5cb0e65606e28fa64a7063f82375912c4ab0`.

V19.5.0 is a deliberate, reproducible target aligned with the generator input,
not a claim to have selected the latest available revision. The initial ADR's
V17.7.0/V17.10.0 normative references are superseded. Relevant clauses were
reviewed again, including presence/error rules, rather than changing links alone.
The official PDFs were inspected with the web reader; the tracked R19 DOCX was
also read directly for detailed PFCP clauses and Setup/group table cells.
No standards document was copied into the repository.

HEAD remains `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, branch `main`, declared
application version `2.8.0`. This application version is distinct from 3GPP
Release 19. PFCP still uses header version `1` (`lib/pfcp/types.h:31`); it must
not be changed to `19`. Generated R19 definitions alone do not establish complete
R19 support: the handwritten UP Function Features section still cites R17.7.1,
and handlers/selection have the documented baseline limitations.

## Clause review and implementation obligations

This table records a **source review**. Its test column identifies future
acceptance evidence, not tests executed against an implemented feature. Protocol
rules and local restrictions are intentionally identified separately.

| Reference | Nature | Review result and effect on the design | Evidence still required |
|---|---|---|---|
| S1 §6.3.3.1–2 | Architecture / optional provisioning path | UP-initiated N4 association can make a UPF known to SMF; NRF is not required by this mechanism. | T05/T06 |
| S1 §6.3.3.3 | Session selection / conditional requirements | DNN learning supplies one selection input. Required UDM-provided UPF functionalities are additional constraints, outside this MVP's guarantee. | T26; controlled topology without those additional requirements |
| S2 §5.21.1–2; §8.2.25 | Allocation / capability signaling | Keep CP allocation; pool advertisement is not UP allocation support. Do not set UEIP, SSET or MPAS to justify learning. | T22; outgoing feature bytes |
| S2 §6.2.6.1–3 | Procedure | Retain both initiators. CP-initiated Setup support is mandatory; UP-initiated support is optional. Success must precede session signaling. | T05/T06/T18 |
| S2 §7.4.4.1–2, tables 7.4.4.1-1/-3 and 7.4.4.2-1 | Optional announcement / grouped structure | Group 233 remains the mechanism for configured pools; identities 177 repeat, NI 22 is optional singleton, slices 257 repeat, IP Version 258 is optional singleton. | T02/T03/T21 |
| S2 §8.2.128; §8.2.4 | Encoding | Identity has its own two-byte length inside the IE. NI may use domain-name/APN encoding; MVP uses APN labels consistent with existing builders. | Independent T02/T04 bytes; external decoder |
| S2 §8.2.176–177 | Encoding / semantics | Slice value has four bytes. At least one V4/V6 bit is required; both are valid. Zero family is a semantic defect when the IE framing is valid. | T04/T16/T27 |
| S2 §7.2.3.2 | Mandatory / conditional / conditional-optional / optional | Children of an optional group are optional for receiver error handling. Conditional checks require sufficient information; CO absence/defects use optional handling. | T19/T25/T27 |
| S2 §7.2.3.2; §7.4.4.2 | Rejection response | Do not require success-only fields on rejection. Include Node ID and Offending IE where required; do not return only Cause from the generic Setup error builder. | T18/T25; check error-response codec support |
| S2 §7.6.1, §7.6.3, §7.6.6 | Matched erroneous response | Stop the corresponding request's retransmissions and notify the PFCP application, including invalid message length and missing mandatory/checkable conditional IEs. Dropping the packet before the handler must not bypass this. | T18/T25 with active transaction and observable retry/notification results |
| S2 §7.6.2–5 | Version / framing / correlation | Retain PFCP version 1. Discard unidentifiable short headers; reject inconsistent request lengths appropriately. Unsolicited responses cannot publish or establish a generation. | T04/T25/T27 |
| S2 §7.6.7–9 | Structural versus semantic errors | Apply length rules to structural failure. Ignore unusable optional semantics and unknown optional/CO IEs; process known usable information. Do not inspect spare bits as reserved values. | T04/T19/T25/T27 |
| S2 §7.6.10 | Repetitions | Unspecified repetitions use the first singleton. A local 16/16/8 storage cap is not a protocol-specified maximum: report unsupported capabilities, never silently truncate learned lists. | T03/T04 |
| S2 §7.4.4.1–2; §8.2.175 | Conditional UPF Instance ID | Sender includes an available 5G UPF instance ID. Receiver cannot infer availability merely from Release 19 or an absent IE. ID is separate from pool identity. | T27; stage 3 availability recheck |
| S2 §7.4.4.1–2 | Conditional N6 measurement protocols | Condition follows advertised UN6DM. The audited UPF initializes neither UN6DM nor an N6 measurement implementation. Do not invent support. | T27 with checkable condition set/unset |
| S2 §7.4.4.1 note 2 | Optional Packet Inspection default | Absence has its own local-configuration/default capability rule. Pool static/learned precedence applies only to IE 233. | T27; no change to unrelated capability policy |
| S2 §6.2.6.2.2 | Existing-Node-ID Setup / retention | Successful replacement has session-handling rules separate from metadata replacement. Baseline compliance here was not established; adding peer B does not prove same-peer session retention. | T11/T15 and explicit limitation |
| ADR local policy | Implementation profile | Explicit persistent pool IDs, owned atomic snapshots, conservative restriction handling and static precedence remain local decisions. They are not new R19 rejection causes. | T11/T16/T19/T24 |

DOCX Setup tables have zero-based indices 16 and 23; the pool child table is 18.
The cache group's `index = 333` is not the source table number. Type 233/177
aliasing to 93 and missing repetitions remain generator/cache defects. No
R19 wire-format change was found that justifies those defects or a downgrade.
The obsolete §8.2.82 remains Void.

## Changes to the earlier audit contract

The core mechanism, CP allocator, explicit identity persistence, bounded owned
snapshots and legacy DNN OR location/fallback policy remain as designed. The
revision makes response transaction handling concrete for R19, records UPF
Instance ID availability as conditional, and adds checks for unrelated optional
capability defaults. These requirements cannot be replaced by a single
"invalid optional IE => reject association" rule.

No instance-ID source was found in `src/upf` initialization or the PFCP UP Setup
builders; an available generated TLV member is not such a source. Stage 3 must
recheck the actual initialization path before deciding presence. Pool learning
must not invent an NF identity, configure NRF or advertise measurement/allocation
capabilities it does not implement.

T25 now explicitly observes transaction completion/error notification and absence
of further retransmissions for correlated malformed responses, in initial and
reassociation states. It also contrasts unmatched responses and a header too
short to identify a transaction. T27 covers R19 boundary/forward compatibility:

1. Independent request/response fixtures preserve unknown optional IEs at
   message and group scope while decoding all supported pools.
2. S-NSSAI length four, IP Version V4/V6/both/zero, truncation versus semantic
   defects, and received spare bits follow the separate rules above.
3. Longer valid UP Function Features do not cause blanket association rejection;
   supported-prefix copying clears previous bytes and respects buffer bounds.
4. UPF Instance ID is emitted when available; remote absence alone is not an
   availability test. UN6DM conditions are checked when verifiable. Include
   known conditional-optional absence/defects and non-233 optional absence.
5. Original R19 descriptors survive regeneration outside the intentional codec
   corrections. Feature-off mode still handles protocol errors correctly.

Codec fixtures belong to stage 2; handler/presence/transaction integration to
stage 3; T26 selector characterization remains valid for stage 4A. No tests above
are reported passed by this documentation-only review.

## Exit and limitations

The design is now referenced to Release 19 V19.5.0 for the requested scope.
Baseline generator runs and eight extracted-selector assertions retain their
original, limited results; a documentation revision does not upgrade them into
R19 interoperability evidence. The initial dependency setup failure (`talloc`
first missing dependency) was subsequently resolved by the authorized stage 2
build, as recorded in its report.

The original codec gates (isolated build, reproducible repair, independent R19
bytes and negative fixtures) subsequently passed in stage 2. At that point,
remaining gates were association and peer ownership integration tests, real
identity restart tests and N6 validation.
The full R19 capability-selection/retention requirements are not certified by
this work. Association Update, strict stage 4B, API/SUPI, orchestration, deployment
waiting and session migration remain outside scope. Stage 3 was subsequently
authorized and implemented; its report supersedes those pending
association/ownership gates and records the actual remaining limits. Stage 4A
was subsequently authorized and completed; its report records selection-boundary
evidence and preserved legacy limitations. UE/N6 validation remains separate;
4B and subsequent stages have not started.
