# Dynamic DNN discovery: stage 2 codec implementation and evidence

Date: 2026-10-07. Baseline HEAD remains
`2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, branch `main`, application `2.8.0`.
Initial local changes were the untracked plan and stage 1 documentation; they
were preserved. No branch/version switch, commit, deployment or NF runtime was
performed. Scope is codec only: no pool announcement, learned peer data, selector
change, allocator change, Association Update procedure, API/SUPI or stage 4B.

Contract: [ADR-001](ADR-001-dynamic-dnn-learning.md),
[Release 19 review](dynamic-dnn-release19-review.md).
Normative reference: [TS 29.244 V19.5.0](https://www.etsi.org/deliver/etsi_ts/129200_129299/129244/19.05.00_60/ts_129244v190500p.pdf),
§7.2.3.2, §7.4.4.1–2, §7.5.2.2, §7.6 and §8.2.128/176/177. Detailed rows were
also inspected in the unchanged local R19.5.0 DOCX; its SHA-256 remains
`1a5cfaf2c1ef67a52f199d2aca1a5cb0e65606e28fa64a7063f82375912c4ab0`.

## Resulting changes

| Area | Change and reason |
|---|---|
| `lib/pfcp/support/pfcp-tlv.py` | Exact normalization of UE IP address, preserving distinct pool types; context-specific repetitions; generated parser hooks/cleanup and header-length checks |
| `support/cache/tlv-group-list.py`, `tlv-msg-5.py`, `-6.py`, `-7.py` | Regenerated from the DOCX; correct 233/177 aliases, repetition counts and executable maximum updates |
| `lib/pfcp/message.[ch]` | Regenerated: group 233 in Setup Request/Response and Update Request, arrays of 16 groups/16 identities/8 slices; NI/IP Version remain singleton |
| Create PDR and Usage Information descriptors | Correct the same type-93 alias defect to 177; Create PDR has a specified IPv4/IPv6 pair, Usage Information a singleton |
| `lib/pfcp/pool-codec.[ch]` | Bounded PFCP prevalidation, explicit pool decode status, identity length helpers and checked APN-label NI conversion |
| `lib/pfcp/ogs-pfcp.h`, `meson.build` | Expose/build the new codec adapter; no shared core/GTP parser edits |
| `tests/unit/pfcp-pool-test.c`, unit registration | Eight ABTS cases, including many independent negative/boundary subcases, integrated in existing `unit` target |
| Fixture and sanitizer scripts | Independent wire assembler and reproducible ASan/UBSan runner using the same ABTS cases |

The corrected alias in the current R19 table is in **Create PDR**, not Traffic
Endpoint/PDI. Two identities here are a protocol-specified pair in IPv4-before-
IPv6 order; the generated array uses two explicit descriptors so later instances
are ignored. The local 16/16/8 limits in pool discovery have different semantics:
an excess yields `UNSUPPORTED`, without exposing a truncated pool list. The
audit's earlier Traffic Endpoint example was corrected in the ADR.

No unrelated R19 IE definitions were removed. The Update Request descriptor is
corrected because it contains the same group; this does not implement Update.
Existing feature flags and outgoing UP Setup builders remain unchanged.

## Decoder contract and ownership

`ogs_pfcp_scan_pool_information()` inspects a TLV body, with no association
acceptance or learned mapping. Result fields report groups seen, invalid and
decoded. For structurally invalid data it returns `OGS_ERROR`, status
`FRAMING_ERROR` and the offending type where identifiable. The generated parser
also checks complete PFCP header availability and the declared message length
before pulling the header; a zero-length packet no longer triggers an assertion.
Mandatory presence, Cause interpretation and transaction notification remain
handler/procedure responsibilities in stage 3.

| Input | Codec result |
|---|---|
| No 233 | `ABSENT`; original generic decoder/lifetime |
| Supported pool groups | `VALID`; every supported repetition retained |
| Good independent groups plus semantic optional defects | `DEGRADED`; omit affected groups, retain all independent valid groups |
| No valid groups, including empty/missing-identity groups | `UNUSABLE`; message can still be decoded |
| More than 16 groups, 16 identities/group, 8 slices/group, or identity longer than 64 bytes | `UNSUPPORTED`; no groups exposed, remaining message decoded |
| Declared TLV extent exceeds its containing span, incomplete nested TLV header or inconsistent message length | Structural error; no partially decoded message returned |
| Repeated singleton NI/IP Version | First value governs, even if later occurrences conflict; later values cannot repair a defective first value |
| Unknown optional children/top-level IEs | Ignore while retaining known fields; test includes 300 distinct unknown types at each scope |

Framing is checked even after semantic defects/capacity excess; overflow cannot
hide a malformed tail. Identity helpers encode/decode the internal big-endian
length and opaque bytes. The wire helper supports 1–65533 bytes; group/message
envelopes impose further limits. The receive profile's 64-byte cap is separate.
NI conversion accepts complete bounded APN labels, preserves case and operator
labels, and never truncates, guesses an OI suffix or treats network input as a
NUL-terminated string. IP Version validates the R19 flag octet, allows either or
both families, ignores spare bits and does not interpret a zero-family value as
an enclosing length error. S-NSSAI value length is four bytes.

The PFCP adapter filters unusable pool groups before the shared TLV decoder.
It removes unknown optional fields and later NI/IP singleton occurrences from
the filtered view, so they cannot exhaust generic descriptor bookkeeping.
Filtering never modifies the input packet. A message containing 233 owns
`pool_decode_storage`; its octet fields borrow that storage until
`ogs_pfcp_message_free()`. Message-free also covers parser failure paths.
Other messages retain their existing borrowed-packet lifetime. No pointer may
be retained beyond message dispatch without copying, as stage 3 requires.
Future learning must consult the explicit status and observed count, rather
than inferring absence from the filtered list: `UNUSABLE`/`UNSUPPORTED` can
contain zero decoded groups while still having received IE 233.

Changing generated structure layouts requires rebuilding PFCP consumers; the
SMF/UPF/SGW-C/SGW-U builds below cover those consumers. This is not an assurance
of binary compatibility with separately built older libraries or plugins.

## Isolated environment and executed commands

Scratch root: `/tmp/open5gs-pfcp-codec-ji13ti1h`; no host packages/services changed.
The original HEAD was extracted into `baseline`; current modified files were
overlaid into a separate `source`. Dependencies fetched by Meson stayed there.
Copies used for final builds had fresh mtimes to prevent a stale object when
regenerated files carried an earlier timestamp. Workspace/subproject contents
were not changed by builds. Containers were ephemeral build/test containers.

Image: `open5gs-pfcp-codec-build:20261007`, image ID
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`.
Base: Ubuntu 24.04, manifest
`sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55`.
GCC 13.3.0, Meson 1.3.2, Ninja 1.11.1, talloc 2.4.2, libtins 4.5,
libmongoc 1.26.0 and YAML 0.2.5. The recipe used:

```dockerfile
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends build-essential meson ninja-build pkg-config cmake flex bison git ca-certificates python3 python3-docx python3-yaml libtalloc-dev libyaml-dev libsctp-dev libc-ares-dev libgnutls28-dev libgcrypt20-dev libssl-dev libmongoc-dev libbson-dev libnghttp2-dev libmicrohttpd-dev libcurl4-gnutls-dev libidn11-dev libtins-dev tshark && rm -rf /var/lib/apt/lists/*
```

Fetched subprojects were recorded: prometheus-client-c
`a58ba25bf87a9b1b7c6be4e6f4c62047d620f402`, freeDiameter
`14725af3ba0edbf9ff61c4e3239ed42464423b2e`. The image is the exact executed
environment; rerunning an unpinned apt recipe can install newer package versions.

Commands below use container paths from the actual run (`/artifacts` binds the
scratch root), with working directory `/artifacts/source`. Baseline used the
same commands with `baseline`, `baseline-build` and `baseline-install` instead.
No installation or NF startup command was executed.

```sh
meson setup /artifacts/build /artifacts/source --prefix=/artifacts/install --wrap-mode=nofallback
ninja -C /artifacts/build -j 6 tests/core/core tests/unit/unit src/smf/open5gs-smfd src/upf/open5gs-upfd src/sgwc/open5gs-sgwcd src/sgwu/open5gs-sgwud
meson test -C /artifacts/build core unit --print-errorlogs
meson setup /artifacts/sanitize-build /artifacts/source --prefix=/artifacts/sanitize-install --wrap-mode=nofallback -Db_sanitize=address,undefined
ninja -C /artifacts/sanitize-build -j 6 lib/pfcp/libogspfcp.so.2.8.0
python3 /src/tests/unit/pfcp-pool-sanitizer.py /artifacts/sanitize-build /artifacts/sanitize-final
```

The sanitizer runner obtains compiler flags from that build's
`compile_commands.json`, compiles the actual ABTS pool test source, and links the
instrumented PFCP/core libraries. It enables ASan leak detection and UBSan
halt-on-error; it does not suppress findings. The driver explicitly releases
the global talloc tracking root after core termination.

Generation used the host scratch venv (`python-docx 1.2.0`, lxml 6.1.3,
typing_extensions 4.16.0), from repository root:

```sh
python3 -m venv /tmp/open5gs-pfcp-codec-ji13ti1h/venv
/tmp/open5gs-pfcp-codec-ji13ti1h/venv/bin/pip install python-docx==1.2.0
/tmp/open5gs-pfcp-codec-ji13ti1h/venv/bin/python lib/pfcp/support/pfcp-tlv.py -f lib/pfcp/support/r19.5.0/29244-j50.docx -c /tmp/open5gs-pfcp-codec-ji13ti1h/cache-fresh-pair -o /tmp/open5gs-pfcp-codec-ji13ti1h/fresh
/tmp/open5gs-pfcp-codec-ji13ti1h/venv/bin/python lib/pfcp/support/pfcp-tlv.py -f lib/pfcp/support/r19.5.0/29244-j50.docx -c /tmp/open5gs-pfcp-codec-ji13ti1h/cache-copy -o /tmp/open5gs-pfcp-codec-ji13ti1h/cached
```

The fresh-cache path was nonexistent before generation. All fresh caches equal
the checked-in caches byte for byte. Cached/fresh/checked-in generated bodies
agree, excluding only `Created on` and `from` provenance lines; codec contents
were not normalized. The DOCX was not edited. Cache type-list zero defaults are
raised by assignments in the regenerated group/message caches, as before.

## Results and binary evidence

| Check | Result | Evidence / practical limit |
|---|---|---|
| Original HEAD build and `core`/`unit` | **PASS** | Six requested targets built; both Meson tests passed |
| Modified codec and SMF/UPF/SGW-C/SGW-U builds | **PASS** | Final source copy rebuilt after last generator/test changes |
| Modified `core` and `unit` | **PASS** | Both Meson tests; existing TLV/GTP/PFCP rule suites and eight new pool cases |
| Independent request/response bytes | **PASS** | Full encoded TLV bodies equal independently assembled fixtures, not just round-trip |
| Optional semantics, counts, malformed input | **PASS** | Each local boundary and excess; simultaneous 16 groups × 16 opaque 64-byte IDs × 8 slices plus 100-byte NI; missing/empty/bad identity, slices, family, labels, duplicates, nested overruns and all truncated request prefixes |
| Shared alias contexts | **PASS** | Create PDR pair preserves order/ignores third; Usage Information uses first singleton |
| Cached / empty-cache regeneration | **PASS** | Generated bodies agree and caches reproduce exactly |
| ASan/UBSan codec cases | **PASS** | Instrumented changed codec/core path, original-packet lifetime checks, malformed/boundary cases, leak detection enabled |
| Independent decoder | **PASS, scoped** | TShark 4.2.2 decodes target pool IEs; feature-extension octets carry undecoded notes |
| Live Setup acceptance, Cause/send/transaction/reassociation | **NOT RUN** | Stage 3 handlers/FSM work intentionally absent |
| Actual UPF restart identity stability and learned snapshots | **NOT RUN** | Advertisement/configuration/learning intentionally absent |
| N6 traffic, allocations, session migration, cluster integration | **NOT RUN** | No NF runtime/deployment; migration outside scope |

There are no outstanding failures in the checks executed for the final codec.
Intermediate failures were retained: first image recipe misspelled
`libc-ares-dev`; first isolated setup lacked `libtins`; a read-only workspace
setup could not materialize an absent Meson subproject; fresh generation with
Ubuntu's python-docx 1.1.0 failed on a merged cell. These were corrected in the
external environment/source copy. The first sanitizer harness run found the
baseline global talloc tracking root (96 bytes); driver cleanup and explicit
`-ltalloc` linking fixed the harness, with leak detection still enabled.
Negative parser diagnostics in passing cases are expected, not test failures.

Binary fixtures and hashes are included as reviewable working-tree artifacts
in [the fixture directory](../../tests/unit/fixtures/pfcp-pool/README.md).
The independent PCAP contains no malformed-packet indication in TShark 4.2.2.
Inspected field names came from `tshark -G fields`, not guessed names:

```sh
tshark -v
tshark -G fields
tshark -r tests/unit/fixtures/pfcp-pool/pool-setup.pcap -V
tshark -r tests/unit/fixtures/pfcp-pool/pool-setup.pcap -T fields -e pfcp.msg_type -e pfcp.ie_type -e pfcp.ue_ip_address_pool_length -e pfcp.ue_ip_address_pool_identity -e pfcp.network_instance -e pfcp.s_nssai_sst.sst -e pfcp.s_nssai_sst.sd -e pfcp.ip_version.v4 -e pfcp.ip_version.v6
```

Both packets decode identities of lengths `10,3,8`, bytes
`65646765312d76342d61,410042,696e7465726e6574`, NI `edge1,internet`, slices
`01/112233` and `02/ffffff`, and family flags `V4+V6,V4`.
This confirms the selected IEs, not complete R19 feature decoding by this older
external decoder or end-to-end interoperability.

External logs include `baseline-setup-final.log`, `baseline-build.log`,
`baseline-tests.log`, `setup-final.log`, `build-verified.log`,
`tests-verified.log`, `sanitize-build-verified.log`, `sanitize-verified.log`,
`generator-fresh-pair.log`, `generator-cached-final.log`, `tshark-version.log`,
`tshark-fields.log`, `tshark-decode.log` and `tshark-summary.tsv` in the scratch
root. Temporary artifacts can be removed by the host; fixtures, scripts,
commands, source decisions and results remain in the repository documentation.

Stage 2 ends here. Stage 3 needs a new instruction and must implement association
result gating, transaction errors and conditional/default behavior before
advertisement/learning can be claimed. T27 codec cases passed; T27 handler
conditions, T18/T25 procedure behavior, T24 real restarts and later selection/N6
evidence remain pending. No automatic advancement is authorized.
