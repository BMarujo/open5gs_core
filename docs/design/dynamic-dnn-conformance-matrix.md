# Scoped clause → behavior → test → evidence matrix

Final review: 2026-11-08. This matrix supersedes the ADR's original planned
evidence column. It distinguishes a specified requirement from this fork's
implementation policy. A passing row describes the named tests, not complete
3GPP conformance or interoperability with every PFCP implementation.

Primary sources are pinned: [TS 29.244 V19.5.0 / Release 19 (S2)](https://www.etsi.org/deliver/etsi_ts/129200_129299/129244/19.05.00_60/ts_129244v190500p.pdf)
and [TS 23.501 V19.5.0 / Release 19 (S1)](https://www.etsi.org/deliver/etsi_ts/123500_123599/123501/19.05.00_60/ts_123501v190500p.pdf).
The checked-in primary generator input is `lib/pfcp/support/r19.5.0/29244-j50.docx`,
SHA-256 `1a5cfaf2c1ef67a52f199d2aca1a5cb0e65606e28fa64a7063f82375912c4ab0`.
The final extension correction was checked against its IE-type table and figures;
the fresh direct PDF download returned HTTP 403, rather than supplying a new
unrecorded specification version.

Evidence abbreviations:

- **F**: `/home/marujo/open5gs_final_review_20261108T154719Z`, current source;
  commands, source provenance, logs, captures, results and SHA256 manifest.
- **A/C/S**: dated [association](dynamic-dnn-association.md),
  [codec](dynamic-dnn-codec.md), [selection](dynamic-dnn-selection.md) reports.
- **R**: [restart](dynamic-dnn-restart-fix.md) and
  [asymmetric recovery](dynamic-dnn-pfcp-recovery.md) reports.
- **K**: [physical cluster](dynamic-dnn-cluster.md),
  [version alignment](dynamic-dnn-version-alignment.md),
  [Containernet/K3s](dynamic-dnn-containernet.md). These predate the final
  extension correction; they are not reruns of F's source.
- **V**: [local IPv6/slice follow-up](dynamic-dnn-ipv6-slices.md),
  `/home/marujo/open5gs_ipv6_slice_results_20261108T171514Z`; same corrected core
  as F, additional test fixtures, no Kubernetes access.

- **P**: [original/new performance and final publication](dynamic-dnn-performance.md),
  `/home/marujo/open5gs_perf_comparison_results_20261108T175906Z`; corrected
  source `384a54f4d9a`, independent control decoding, five paired trials and both
  cluster NAS/data smokes. Previous K campaigns retain their original image IDs.

- **M**: [four-UE unshaped TCP follow-up](dynamic-dnn-multiue.md),
  `/home/marujo/open5gs_multiue_unshaped_results_20261108T222927Z`; same NF
  binaries, 40 sessions / 80 measurements, final `analysis-03`, verified TC restore.

- **U**: [single-UE unshaped TCP follow-up](dynamic-dnn-singleue.md),
  `/home/marujo/open5gs_singleue_unshaped_results_20261109T034153Z`; 10 sessions /
  20 measurements, protocol/QoS checks PASS, throughput equivalence INCONCLUSIVE.

| Clause / nature | Implemented behavior confirmed in code | Test / result | Evidence and boundary |
|---|---|---|---|
| S1 §6.3.3.1–3; provisioning and selection | UP-initiated association supplies the peer; usable learned DNNs enter the existing selector | T05–T08 PASS | F `verified-dynamic`, `verified-selection`; K first-request evidence without static peer DNN/location |
| S1 §6.3.3.3; broader selection requirements | Only the existing DNN arm is extended; additional UDM-required UPF functions are not implemented | NOT RUN, outside MVP | ADR scope; no full R19 selection claim |
| S2 §6.2.6.2–3, §7.4.4.1–2; both Setup directions | UPF Request and successful Response carry configured pools | T05/T06 PASS | F dynamic/static, association and image smoke; A independent decoder |
| S2 table 7.4.4.1-3; pool group | 233 remains grouped; 177/22/257/258 retain context | T02/T03/T21 PASS | F unit and generator output; C literal byte fixtures |
| S2 §8.2.128; identity | Two-byte internal length delimits opaque bytes, independent of NUL; trailing extensions ignored | T02/T04/T27 PASS | F `test_extensions`, `test_identity_helpers`, `extensions.pcap` |
| S2 §8.2.4; Network Instance encoding / local profile | Bounded APN labels, completely validated before conversion; no suffix stripping | T04 PASS | F `test_network_instance`, malformed labels/lengths; this profile does not implement every alternative NI representation |
| S2 §8.2.176; fixed S-NSSAI | Exactly four value bytes; groups preserve restrictions | T16/T21/T27 PASS at boundary; existing slice NAS PASS | F pool and association tests; V existing requested/allowed NSSAI off/on, SD 000080/009000; full NAS using slice-restricted PFCP advertisements NOT RUN |
| S2 §8.2.177; family | V4=1, V6=2, both valid; zero invalid optional semantics; spare bits ignored | T04/T16/T27 PASS, including learned IPv6 NAS/data locally | F unit/family projection; V IPv6-only and dual-stack first N4, NAS family and IPv6 gateway GTP request/reply, both initiators; external IPv6 DN NOT RUN |
| S2 §8.1.2; extendable vs fixed types | Prefix-compatible 177, 258, IP-form Node ID 60 and Recovery Timestamp 96; fixed Cause 19 and S-NSSAI 257 stay exact | T27 PASS | F `extension-before-fix.log` expected FAIL, `final-build-tests.log`, `verified-sanitize.log`; TShark 4.2.2 decodes prefixes and notes unknown octets |
| S2 §5.21.1–2; allocation / local policy | CP allocator unchanged; advertisement does not enable UEIP | T22 PASS characterization | F NAS IPs and routes; K N6; incompatible CIDR negative in stage 5; IE 233 does not detect address-plan incompatibility |
| S2 §7.2.3.2; presence and rejection | Separate protocol acceptance from optional capability usability; rejection has reduced field requirements and documented Node ID exceptions | T18/T19/T25 PASS | F `test_erroneous_response`, `test_optional_association`, handler cases; A raw responses |
| S2 §7.6.1, §7.6.3, §7.6.6; matched erroneous replies | Stop matching Setup retries, notify callback and do not publish a generation | T18/T25 PASS | F directed association tests and actual association scenarios; unidentifiably short headers cannot terminate a transaction |
| S2 §7.6.2–5; header/correlation | Version 1; validate length before merge; unsolicited or obsolete Setup replies cannot establish/publish | T04/T25/T27 PASS | F unit/actual association; 128 unmatched replies per NF in each recovery case |
| S2 §7.6.7; structural length | Bounds checked at outer/nested TLV levels before dereference | T04/T25 PASS | F malformed/truncated cases and sanitizers; optional semantics are not classified as framing overruns |
| S2 §7.6.6, §7.6.8–9; optional and unknown information | Ignore optional semantic defects/unknown children while keeping independent usable groups | T19/T25/T27 PASS | F nine pool cases and ten association cases; no blanket rejection for optional errors |
| S2 §7.6.10; repetitions | First singleton governs; specified Create PDR pair handled, later copies ignored | T03/T04 PASS | F alias-context/repetition tests; Usage Information remains singleton |
| Local storage policy, not protocol maximum | 16 groups × 16 IDs × 8 slices, 64 identity bytes; overflow suppresses entire list as UNSUPPORTED | T03/T04 PASS | F simultaneous limits/excess tests, config scenarios; no silent partial-list eligibility |
| S2 §7.4.4.1–2, §8.2.175; conditional availability | Do not invent UPF Instance ID when no source exists; remote absence does not prove availability | T27 scoped PASS; broader condition NOT RUN | A source-path audit and F optional-handler tests; availability source/integration not implemented |
| S2 §7.4.4.1–2; N6 measurement recommendation | No measurement feature invented; UN6DM without recommended protocols logs a warning | T27 scoped PASS | F `test_optional_association`; actual N6 measurement NOT RUN |
| S2 §7.4.4.1 note 2; unrelated capability defaults | Pool precedence is confined to 233; no universal rewrite of capability policy | T27 scoped PASS | Source diff and optional/absence cases; packet-inspection service NOT RUN |
| S2 §6.2.2; Heartbeat and local recovery policy | Brief loss may retry; timeout invalidates; previously associated SMF/UPF can initiate new Setup | T12 PASS | F `verified-loss-smf`, `verified-loss-upf`, `verified-loss-both`, `verified-brief`; unchanged NF PIDs/recovery values; K heartbeat-only loss NOT RUN |
| S2 §6.2.6.2.1–2, table 7.4.4.1-2, §8.2.134; retention | CP reconnect emits empty 183; associated UP endpoint acknowledges retain-all using 184 PSREI; unsupported selective content ignored | Directed PASS | F retention unit case, CP restart and loss captures; R full NAS loss tests; selective-retention interoperability NOT RUN |
| Local ownership/replacement policy | Deep-copy snapshot; atomic replacement, epoch guards, free on invalidation/destruction; NF events use their owning allocator | T11/T12/T18 PASS | F ASan/UBSan and real-NF unmatched/recovery tests; no leak suppression in directed suites |
| Local static/learned policy | Feature off uses static; accepted ABSENT permits static; invalidated NULL is distinct; unusable received data does not revive static | T11/T13/T19 PASS | F actual selector, 14 runtime scenarios; old static arrays retained separately |
| Local legacy policy | DNN OR location, current RR and first-peer fallbacks preserved | T26 PASS characterization | F six selector cases including 6,144 comparisons; real no-match NAS cases fail as recorded, rather than proving strict exclusion |
| Local pool identity persistence policy | Explicit YAML ID survives restart/reordering/N4 change; edits replace identity; no instability detector | T24 PASS | F 15 config scenarios; A real UPF restart comparisons; K recreated C retains ID |
| Local dynamic provisioning boundary | Unknown learned DNN does not create SMF pool/subscription/service | T14 PASS at boundary | S/F selection tests; new NAS for an entirely unprovisioned DNN NOT RUN |
| Established sessions / local non-migration policy | Addition/replacement does not reselect established sessions | T15 PASS, scoped | F gateway GTP data; K 57 payload checks across real N6/DN; crashed peer's sessions not guaranteed |
| Association Update / hot reload | Codec descriptors exist; no new running Update procedure or YAML hot reload | T23 NOT RUN | Explicit scope exclusion, not a missing claim hidden by Setup tests |
| Optional strict selection 4B | Not implemented | T09/T10 NOT RUN (optional) | Deliberately excluded; no strict no-candidate result promised |
| Shared PFCP consumers / regression boundary | SMF, UPF, SGW-C, SGW-U rebuilt; handlers gate state transition on result | T01/T17 PASS, scoped | F complete build; registration/auth, different-DNN slice and EPC attach/simple; exhaustive SGW malformed-Setup runtime campaign NOT RUN |
| QoS / performance regression boundary (local verification, no new normative claim) | Existing QER/bitrate conversion and data path preserved; no new rate limiter | PASS, bounded single-UE comparison | P: 10 accepted sessions, 40 TCP/UDP measurements, unchanged 1-Gbit/s AMBR/MBR, 5QI 9, ARP 8, QFI 1; zero PFCP retries; 50-Mbit/s access, maximum capacity/scale NOT RUN; M adds four simultaneous UEs with no TC shaping, equal 10-Gbit/s QoS, zero N4 retries and a passing paired TCP screen; unpaced UDP equivalence NOT RUN; U adds one-UE uncapped TCP rates/ranges with equal QoS and zero N4 retries, but equivalence INCONCLUSIVE |

Source-to-test links: [pool cases](../../tests/unit/pfcp-pool-test.c),
[association cases](../../tests/unit/pfcp-association-test.c),
[actual selector cases](../../tests/unit/smf-selection-test.c),
[integration harness](../../tests/pfcp/dynamic-dnn-integration.py).
Full execution identities, intermediate failures and image/deployment boundaries
are in the [final review](dynamic-dnn-final-review.md).
