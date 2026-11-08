# Dynamic DNN discovery: stage 1 audit and work status

Current project status: [final review](dynamic-dnn-final-review.md) and
[operation guide](dynamic-dnn.md). This dated report preserves the results and
limits at its execution date; later corrections/validation do not rewrite it.

Audited on 2026-10-06 (Europe/Lisbon). Stage 1 is complete as a documentation
audit with a recorded build blocker. At that audit date, stages 2, 3, 4A, 4B and
deployment work had **not** been executed. No core source, configuration template,
dependency manifest or generated codec file was changed by stage 1.

Contract: [ADR-001](ADR-001-dynamic-dnn-learning.md).
Plan: [OPEN5GS_DYNAMIC_DNN_PLAN.md](../../OPEN5GS_DYNAMIC_DNN_PLAN.md).
Reproducible characterization: [experiments](dynamic-dnn-audit-experiments.md).
Normative revision on 2026-10-07: [Release 19 review](dynamic-dnn-release19-review.md).
Subsequent authorized stage 2: [codec implementation and executed evidence](dynamic-dnn-codec.md).
Stage 2 resolved the build dependency blocker in an isolated image, built the
original HEAD and modified targets, and passed core/unit and scoped sanitizer
checks. Original audit results below are historical; stages 3/4A/4B remain absent.
The current plan and ADR use TS 23.501 V19.5.0 and TS 29.244 V19.5.0, replacing
the initial R17 normative profile. The results below retain their original
dates and limitations; this documentation revision does not constitute a new
core build or an R19 interoperability test.

## Checkout and instructions

| Item | Observed value |
|---|---|
| Root | `/home/marujo/open5gs_core` |
| HEAD | `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41` |
| Branch | `main` |
| Last commit | 2026-09-21, `OSS-Fuzz: Add new fuzzer targets sbi request and openapi model` |
| Declared version | `2.8.0` in root `meson.build`; no local tag points at HEAD |
| `git describe --tags --always --dirty` | `2c80f8f2b`; this does not account for untracked files |
| Origin fetch/push | `git@github.com:BMarujo/open5gs_core.git` (no credentials) |
| Initial status | `?? OPEN5GS_DYNAMIC_DNN_PLAN.md`; no tracked changes |
| Reference ancestry | `157f611a530e292e40ec50f9d23f0ef5d4fcd6a6` is an ancestor; 205 commits to HEAD |
| Applicable files | No `AGENTS.md` in root, ancestors `/`, `/home`, `/home/marujo`, or any repository subdirectory; `/home/marujo/.codex/AGENTS.md` exists but is empty |

The repository's `lib/pfcp/support/README.md` and build guide were also read.
No instruction file from a sibling repository applies here. No branch switch,
reset, fetch, tag change, commit, push, PR or cluster operation was performed.
The original untracked plan was preserved in the external audit directory before
editing; it remains untracked. All new repository artifacts are Markdown.

The reference's generated codec, generator and caches are unchanged at HEAD.
Other PFCP/context/UPF files differ substantially: current peer identity/address
merge and subnet validation logic, additional rule validation and unit tests,
and SMF session cleanup/accounting changes must be preserved. Association
handler deficiencies and the selector algorithm discussed here remain present.
The current unit target **already links `libpfcp_dep`** and registers
`pfcp-rule-test.c`; do not copy a stale dependency change from an older plan.

Upstream searches were read-only on 2026-10-06 for `UE IP Pool association` and
`dynamic DNN UPF`. No equivalent implementation/PR was verified in the returned
results. Related discussions [#2877](https://github.com/open5gs/open5gs/discussions/2877)
and [#4392](https://github.com/open5gs/open5gs/discussions/4392) concern selection
and pool topology, not evidence that this feature is implemented. This was a
bounded search, not an exhaustive assertion that no such work exists.

## Confirmed path through this HEAD

All links below are pinned to the audited fork commit; line references identify
the inspected implementation rather than the old plan's line numbers.

| Boundary | Code and finding |
|---|---|
| UPF YAML → shared parser | [`src/upf/init.c:57`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/src/upf/init.c#L57) calls `ogs_pfcp_context_parse_config(APP_NAME, "smf")` |
| `session` → subnet | [`lib/pfcp/context.c:753`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/lib/pfcp/context.c#L753) parses subnet/gateway/APN or DNN/device/ranges and calls `ogs_pfcp_subnet_add`; no pool identity option |
| Stored pool | [`context.h:380`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/lib/pfcp/context.h#L380): embedded DNN, address family, subnet/gateway, ranges, allocator pool and device; maximum 16 subnets |
| UPF capabilities | [`src/upf/context.c:44`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/src/upf/context.c#L44): FTUP/EMPU/MNOP/VTIME/FRRT, length 4; UEIP is not enabled |
| Setup transmission | [`lib/pfcp/build.c:186`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/lib/pfcp/build.c#L186), `ogs_pfcp_up_build_association_setup_request/response`: Node ID, timestamp, features (Cause in response); no loop over subnets or pool/DNN advertisement |
| Receive → peer | [`src/smf/pfcp-path.c:97`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/src/smf/pfcp-path.c#L97): `ogs_pfcp_parse_msg`, `ogs_pfcp_extract_node_id`, find/add/merge peer; unknown Setup request **or response** can create a peer before transaction/FSM acceptance |
| Transaction → handler | [`src/smf/smf-sm.c:427`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/src/smf/smf-sm.c#L427): transaction lookup/receive then FSM dispatch; packet/message freed immediately afterward |
| Handler → peer features | [`lib/pfcp/handler.c:107`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/lib/pfcp/handler.c#L107): only function features learned; no DNN snapshot |
| Static peer mapping | [`context.c:716`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/lib/pfcp/context.c#L716): `node->dnn[]` contains borrowed YAML strings copied as pointers; separate from subnet's embedded DNN |
| SMF selection | [`src/smf/context.c:1388`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/src/smf/context.c#L1388): DNN OR E-Cell/NR-Cell/TAC; cyclic match, optional associated-peer RR fallback, then first peer regardless of association |
| Normal 5GC session | [`nudm-handler.c:357`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/src/smf/nudm-handler.c#L357) allocates IP; [`npcf-handler.c:590`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/src/smf/npcf-handler.c#L590) selects UPF, later sends PFCP establishment (line 817) |
| EPC callers | `src/smf/gn-handler.c:310/326`, `s5c-handler.c:266/294` select before allocating, then `gsm-sm.c` sends establishment; same selection helper, different ordering |

The missing arrows are UPF pool → emitted IE 233, parsed IE 233 → owned peer
snapshot, and snapshot → DNN comparison. Implementing only the middle arrow
does not deliver the requested mapping behavior.

## Association and reassociation defects

The four CP/UP Setup handlers in `lib/pfcp/handler.c` were read, along with
SMF/UPF/SGW-C/SGW-U Setup cases in both `will_associate` and `associated`.

1. Both request handlers call `*_send_association_setup_response(...REQUEST_ACCEPTED)`
   first, ignore its return value, then update features. There is no complete
   mandatory/conditional validation before acceptance. Node ID has already been
   checked in `util.c`/the receive callback, but that is not full Setup validation.
   Recovery Timestamp presence is not checked by these handlers.
2. Both response handlers commit the transaction and update features without
   checking Cause presence or acceptance. They return true unconditionally.
3. Initial Setup cases ignore handler results and unconditionally transition to
   `associated`. Reassociation cases ignore results too, leaving the old state
   without an explicit successful replacement boundary.
4. Feature copies do not first clear previous storage; absent or shorter feature
   data can preserve stale state. UP-side CP features are global context data,
   so changes must avoid accidentally claiming they are per-peer.
5. `ogs_pfcp_send_error_message()` in `path.c:553` populates only Cause for Setup
   responses. It omits Node ID, while `ogs_pfcp_extract_node_id()` requires it.
   Ordinary Setup builders do include Node ID. A rejection-response regression
   should be independently reviewable; do not relax the parser to mask it.
6. The receive callback drops parser/Node ID failures before transaction dispatch.
   Error replies dropped there may not stop the outstanding retransmission as
   required. Stage 3 needs a minimal error-routing/notification path, not just
   boolean checks around FSM transitions.
7. Heartbeat handlers maintain recovery state; no-heartbeat transitions back to
   `will_associate`, and restoration/session handling already exists. There are
   no Association Update cases in these four FSMs. Snapshot invalidation must
   cover heartbeat-detected restart even on paths that restore while remaining
   associated. Do not overwrite recovery semantics using the Setup timestamp.

The original plan's rule “any validation failure must prevent association” was
too broad. The ADR classifies association errors separately from optional
capability errors, including lengths versus semantics, accepted versus rejected
responses, and failed replacement versus a still-live previous association.
This requires protocol fixes in later work, not in this audit.

## Codec and generation findings

[`message.h:2004`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/lib/pfcp/message.h#L2004)
contains the misleading declaration:

```c
ogs_pfcp_tlv_ue_ip_address_t ue_ip_address_pool_identity;
```

The Setup request/response fields likewise use `ogs_pfcp_tlv_ue_ip_address_t`
for pool information. Their descriptors in `message.c` point to type 93, even
though separate type-177 and grouped type-233 descriptors exist. The grouped
descriptor itself selects the wrong identity child. Scalar aliases have similar
C layouts, so successful compilation cannot establish wire correctness.

The root cause is
[`pfcp-tlv.py:309`](https://github.com/BMarujo/open5gs_core/blob/2c80f8f2ba9d378bc845d73fe2fbf4820039fc41/lib/pfcp/support/pfcp-tlv.py#L309):

```python
elif ie_type.find('UE IP address') != -1:
    ie_type = 'UE IP Address'
```

The same error survives in `cache/tlv-msg-5.py`, `-6.py`, `-7.py`, and
`tlv-group-list.py`. `tlv-type-list.py` contains correct numeric types but zero
`max_tlv_more`. `get_cells()` hard-codes repetitions for other IEs, not these
pools/identities/slices. S-NSSAI multiplicity must also be corrected, not just
233 and 177. Network Instance/IP Version remain singletons.

`lib/core/ogs-tlv-msg.c:641` skips unknown descriptors; repeated singleton
instances after the first are not stored. Array exhaustion logs “Multiple of
the same type TLV need more room” and continues, losing data; it does not return
an explicit overflow result. A malformed nested TLV returns a generic error
for the whole parse. Octet parsing at lines 568/576 borrows `tlv->value`.
`ogs_pfcp_node_free()` currently frees transactions and addresses but has no
learned-data cleanup. These observations motivate bounded status and ownership
changes in the ADR, with regression coverage for shared TLV/GTP users.

The generator imports python-docx and opens a DOCX even when caches exist.
Tracked sources include R17.7.1, its modified variant, and R19.5.0. The generated
header names `r19.5.0/29244-j50.docx`. Both cache-backed and cache-free generation
were executed **without changing the repository** and reproduce HEAD's bodies.
Only generation date, username and source path differ. This is successful
baseline regeneration, not a repaired codec or proof of protocol interoperability.
The R19 DOCX's zero-based tables 16/23 (Setup) and 18 (pool group) were inspected
directly. The initial comparison concerned pool presence/repetition only; it did
not establish R19 equivalence for the full association/error procedures. The
2026-10-07 review now uses the matching R19 normative profile and separately
records transaction error handling, conditional UPF Instance ID and other
optional defaults. Cache group `index = 333` is not the DOCX table number; use the
actual table headers when tracing the source. The input already describes the
right group; the generator normalization/repetition logic needs correction.

## Isolated baseline and results

Scratch root: `/tmp/open5gs-dnn-audit-b1oeg_64`; original plan backup:
`plan-original.md`. No system packages were installed. Python dependencies were
installed only in this directory's venv. Build/prefix paths are external to the
checkout. Docker server 29.8.2 was reachable; only `info` and image listing were
used. No containers were created, and no deployment images were rebuilt.

| Check | Result / exact limitation |
|---|---|
| Host toolchain | Ubuntu 26.04.1, GCC 15.2.0, Meson 1.10.1, Ninja 1.13.2, Python 3.14, pkg-config 2.5.1 |
| `meson setup /tmp/open5gs-dnn-audit-b1oeg_64/build . --prefix=/tmp/open5gs-dnn-audit-b1oeg_64/install --wrap-mode=nofallback` | **BLOCKED**, exit 1 at `lib/core/meson.build:314`: `talloc` not found; CMake absent |
| Additional host dependency probe | libmongoc-1.0, libbson-1.0, yaml-0.1, libmicrohttpd, libcurl pkg-config names absent; full dependency resolution did not finish |
| Ninja / Meson suite listing and tests | **NOT RUN**: setup failed, no configured full build |
| Cached generator | **PASS**, exit 0, generated bodies equal HEAD |
| Fresh-cache generator | **PASS**, exit 0, generated bodies equal HEAD |
| Extracted C legacy selector | **PASS**, 8 assertions, GCC `-std=gnu89 -Wall -Wextra -Werror`; minimal context adapters, no core integration |
| Proposed identity contract | **PASS (model only)**, fresh processes, ordering/N4 independence, explicit change accepted, duplicate/invalid IDs rejected, 64-byte boundary |
| Actual UPF restart, new codec, snapshots, PFCP/FSM integration | **NOT RUN**: feature intentionally not implemented |
| External PFCP decoder | **NOT RUN**, `tshark` absent; no PCAP produced |
| Sanitizers, database/TUN/SCTP and end-to-end suites | **NOT RUN**; no isolated NF runtime environment prepared |

The first characterization harness compile failed because its new `main` lacked
an explicit C89 return; this was fixed in the documentation harness and the
rerun passed. It was not a baseline core failure. The downloaded ETSI PDFs could
not be saved via urllib (HTTP 403); the official PDFs were successfully inspected
through the web reader. No standards PDF was added to the repository.

The selector characterization directly confirms: DNN case-insensitive matching,
same-DNN rotation, an incompatible-DNN TAC match, no-match associated fallback,
unavailable peer skipped when an alternative exists, all-unavailable first-peer
fallback, and first-peer fallback with RR disabled. This resolves the test
expectation conflict without changing selection policy.

Reproduce the generator baseline with the following commands, adapting only
`audit_dir` to a fresh temporary directory:

```sh
audit_dir=/tmp/open5gs-dnn-audit-b1oeg_64
python3 -m venv "$audit_dir/venv"
"$audit_dir/venv/bin/pip" install python-docx==1.2.0 PyYAML==6.0.3 lxml==6.1.3 typing_extensions==4.16.0
mkdir -p "$audit_dir/cached" "$audit_dir/fresh"
cp -a lib/pfcp/support/cache "$audit_dir/cache-copy"
"$audit_dir/venv/bin/python" lib/pfcp/support/pfcp-tlv.py -f lib/pfcp/support/r19.5.0/29244-j50.docx -c "$audit_dir/cache-copy" -o "$audit_dir/cached"
"$audit_dir/venv/bin/python" lib/pfcp/support/pfcp-tlv.py -f lib/pfcp/support/r19.5.0/29244-j50.docx -c "$audit_dir/cache-fresh" -o "$audit_dir/fresh"
```

Use a **fresh** `cache-fresh` path. Compare outputs excluding only the generated
comment header; do not normalize codec contents. Actual artifacts include
`meson-setup.log`, `build/meson-logs/meson-log.txt`, `commands.json`,
`cached-generator.log`, `fresh-generator.log`, `message.[ch].fresh.diff`,
`python-requirements.txt`, `characterize.py` and `characterize.log`.
Direct source inspection is recorded in `docx-pool-rows.log` and
`docx-pool-group.log`.
The passing C/model run's extra files are in
`/tmp/open5gs-dnn-characterization-xiolo7nl`. These temporary directories may be
cleaned by the host; the Markdown records commands, outcomes and the harness.

| Baseline file | SHA-256 |
|---|---|
| R19.5.0 DOCX | `1a5cfaf2c1ef67a52f199d2aca1a5cb0e65606e28fa64a7063f82375912c4ab0` |
| `pfcp-tlv.py` | `8bcfbbfcba939af2d1d068e400edb1e23e3a76672fb3acf2598fe0ec5372719c` |
| `message.h` | `3bb426390310b2e375f269895edaef629d2f9eeda2e797878043c5e139cfaaa5` |
| `message.c` | `1609486c675ca86a01367ee815175af4e6273a2b927887c0e3952303a54d5d61` |

For stage 2, prepare a dedicated build image or user prefix with the dependencies
listed in the checkout's build guide (talloc, YAML, SCTP, c-ares, GnuTLS, gcrypt,
OpenSSL, Mongo C/BSON, nghttp2, microhttpd, curl, idn, Meson/Ninja/CMake/flex/bison).
Record the image digest and package versions when actually built. The HEAD also
allows `mongoc2` as a fallback; do not assume a specific missing alternative from
the earlier libmongoc probe. No image is claimed built by this audit.

After setup succeeds, build `tests/core/core`, `tests/unit/unit`, SMF, UPF,
SGW-C and SGW-U targets; list tests with `meson test -C "$audit_dir/build" --list`,
then run `meson test -C "$audit_dir/build" core unit --print-errorlogs`.
Those two tests are registered in Meson at HEAD and do not start the full core
network. Targeted TLV/PFCP unit coverage belongs there; current unit tests already
contain PFCP rule checks but no dynamic pool advertisement checks. Run database,
registration/slice/attach and SCTP-dependent integration only in a separate
network/runtime environment with its own addresses, database and TUN resources.

## Exit decision and remaining gates

The 2026-10-07 documentation revision preserves HEAD, branch and all tracked
files. Initial status for this revision was the untracked plan and `docs/design/`
created by stage 1. Previous Markdown was backed up in
`/tmp/open5gs-r19-review-7k4c40qc/before`; paragraph extracts of the local R19/R17
DOCX files were created outside the checkout for source inspection. Only the R19
profile is normative for the revised contract. No generator/build/test rerun was
needed for documentation changes; previous passes and blockers remain as above.
Documentation checks passed for all five Markdown files: local links, code
fences, whitespace, current normative URLs and the T01–T27 acceptance matrix.
The four generator/codec hashes above were verified unchanged, and
`git diff HEAD --name-only` was empty. The revision diff is recorded externally
as `/tmp/open5gs-r19-review-7k4c40qc/documentation.diff`.

The mechanism is justified for actual configured pools and the controlled
addressing topology in the ADR. The original plan has been corrected for optional
IE handling, observable identity stability, same-DNN incompatible pools, and the
true legacy fallback. Advertisement control and SMF mapping policy are separate.

Unresolved experimental work is explicit: a dependency-complete full build,
repaired-codec fixtures and independent decoding, real UPF restart identity
stability, FSM negative/reassociation paths, snapshot memory checks, and N6 data
validation. Exact implementation of bounded parser status must be reviewed with
the stage 2 diff; it must satisfy the ADR's error classes. These are future-stage
gates, not claims that those tests already passed.

Advance to stage 2 only on a new user instruction, retaining this HEAD or reviewing
new deltas, using the R19 ADR, clause review and pinned R19.5.0 generator source,
and addressing the isolated
build dependency blocker. Stage 2 is codec work only and must end with reproducible
generation, independent bytes, error/multiplicity tests and successful relevant
build/tests, including the codec portion of T27. T18/T25 transaction behavior and
T27 handler conditions remain stage 3 gates. Stages 3/4A and especially 4B are
not implicitly authorized.
