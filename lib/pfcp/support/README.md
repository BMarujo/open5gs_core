Regenerate PFCP TLV message files
=================================

This document describes how to regenerate the PFCP TLV message
encoder/decoder files for Open5GS using the 3GPP TS 29.244 document.

Install dependency
==================

Use an isolated Python environment with `python-docx==1.2.0` for the pinned
R19.5.0 input. The Ubuntu 24.04 package (1.1.0) did not parse all merged table
cells correctly in the codec validation; do not accept partial cache output.

```sh
python3 -m venv /tmp/pfcp-generator-venv
/tmp/pfcp-generator-venv/bin/pip install python-docx==1.2.0
```

Prepare TS 29.244 input file
============================

Prepare the 3GPP TS 29.244 Word document.

Example input file:

```text
r19.5.0/29244-j50.docx
```

The generator reads the Word tables directly from the input document.
When the 3GPP document version changes, the table layout may also change.
Some table cells may need to be adjusted manually so that each row has the
expected IE name, presence, comment, and IE type fields.

Update table indexes
====================

Update the table indexes in `pfcp-tlv.py` if the input Word document has
changed.

The generator uses Word table indexes to find each PFCP message table.

```python
# Table number for Message List
msg_list["PFCP Association Setup Request"]["table"] = 16
msg_list["PFCP Association Setup Response"]["table"] = 23
msg_list["PFCP Association Update Request"]["table"] = 24
msg_list["PFCP Association Update Response"]["table"] = 26
msg_list["PFCP Association Release Request"]["table"] = 27
msg_list["PFCP Association Release Response"]["table"] = 28
msg_list["PFCP Node Report Request"]["table"] = 29
msg_list["PFCP Node Report Response"]["table"] = 37
msg_list["PFCP Session Set Deletion Request"]["table"] = 38
msg_list["PFCP Session Set Deletion Response"]["table"] = 39
msg_list["PFCP Session Set Modification Request"]["table"] = 40
msg_list["PFCP Session Set Modification Response"]["table"] = 42
msg_list["PFCP Session Establishment Request"]["table"] = 43
msg_list["PFCP Session Establishment Response"]["table"] = 84
msg_list["PFCP Session Modification Request"]["table"] = 100
msg_list["PFCP Session Modification Response"]["table"] = 127
msg_list["PFCP Session Deletion Request"]["table"] = 132
msg_list["PFCP Session Deletion Response"]["table"] = 133
msg_list["PFCP Session Report Request"]["table"] = 137
msg_list["PFCP Session Report Response"]["table"] = 153
```

The exact indexes may change when the Word document is edited. If the
generator cannot find a message table, run the generator with debug output,
check the printed table indexes, and update `pfcp-tlv.py` again.

```sh
python3 pfcp-tlv.py -d -f r19.5.0/29244-j50.docx -o ..
```

Grouped IE tables
=================

The generator also scans grouped IE tables from the Word document.

Grouped IEs such as `Create URR`, `Update URR`, `Create FAR`, `Update FAR`,
and Access Forwarding Action Information must be parsed as grouped IE
structures. If a grouped IE is not found, it may be generated as a plain
`ogs_tlv_octet_t`, which will cause build errors when existing code accesses
child IEs.

For example, this is wrong for a grouped IE:

```c
typedef ogs_tlv_octet_t ogs_pfcp_tlv_create_urr_t;
```

It should be generated as a structure:

```c
typedef struct ogs_pfcp_tlv_create_urr_s {
    ogs_tlv_presence_t presence;
    ...
} ogs_pfcp_tlv_create_urr_t;
```

When the Word document contains rows such as `Same IEs and requirements as
defined in Table ...`, update the generator so that the referenced IE list is
copied instead of treating the row as a normal IE.

Generate files
==============

If the input document, table indexes or generator logic changed, first regenerate
using an empty external cache; preserve the checked-in cache for comparison.

```sh
mkdir -p /tmp/pfcp-generated
/tmp/pfcp-generator-venv/bin/python pfcp-tlv.py -f r19.5.0/29244-j50.docx -c /tmp/pfcp-empty-cache -o /tmp/pfcp-generated
```

Use a previously nonexistent `/tmp/pfcp-empty-cache` path. Review/update the
checked-in caches from that output, then run the generator with those caches.

```sh
/tmp/pfcp-generator-venv/bin/python pfcp-tlv.py -f r19.5.0/29244-j50.docx -c cache -o ..
```

This regenerates the following files:

```text
../message.h
../message.c
```

Pool Information codec (Release 19 V19.5.0)
===========================================

UE IP Address Pool Information is grouped type 233, not UE IP Address (93).
Its identity child is type 177 with an internal two-octet big-endian identity
length. The generator normalizes the exact spelling of type 93, without
collapsing pool types by substring. Repetitions are context-specific:

| Context | Local storage / protocol count |
|---|---|
| Setup Request/Response, Update Request | 16 groups 233 (local cap) |
| Group 233 | 16 identities 177, 8 S-NSSAIs 257 (local caps) |
| Group 233 Network Instance / IP Version | Singleton; first occurrence |
| Create PDR | Specified pair of identities 177; subsequent instances ignored |
| Usage Information | Identity 177 remains singleton |

Cache type-list entries start with zero `max_tlv_more`; generated group/message
caches raise the maxima as they are loaded. Keep those assignments together with
the per-context `tlv_more` values. Cached and empty-cache output must agree.
The Create PDR pair uses two descriptors for its array, so the shared parser
implements the specified count without confusing it with a local overflow.

`pool-codec.[ch]` prevalidates pool TLV framing and local capacities for Setup
Request/Response and Update Request. Optional semantic defects omit the affected
group, retaining independent valid groups and an explicit decode status. Excess
local capacity suppresses the entire pool list with `UNSUPPORTED`; it does not
return a generic association failure. A structural length failure remains an
error. Unknown optional children and later singleton repetitions are ignored.
No shared core/GTP parser behavior, association decision, advertisement or UPF
selection policy is implemented here.

Messages containing 233 retain an owned filtered body in `pool_decode_storage`;
their octet fields borrow that storage until `ogs_pfcp_message_free()`. Other
messages retain the existing packet lifetime. These fields must still be copied
before retaining capabilities beyond message dispatch. Use
`ogs_pfcp_scan_pool_information()` to obtain framing/capacity diagnostics without
accepting an association. NI conversion follows the ADR's bounded APN-label
profile; the 64-byte opaque receive identity cap is local, not a wire maximum.

Independent fixtures, negative tests, regeneration commands and actual results
are recorded in [the stage 2 report](../../../docs/design/dynamic-dnn-codec.md).

Update PFCP IE support code
===========================

If TS 29.244 removes or changes an IE, update the hand-written PFCP support
code as needed.

For example, if a removed IE is no longer generated in `message.h`, remove
references to that IE from files such as:

```text
../build.c
../handler.c
../types.c
../types.h
../conv.c
../conv.h
```

Use existing PFCP IE helper functions and structures as references when adding
new IEs or removing obsolete ones.

Final check
===========

Check the generated changes.

```sh
git diff ../message.h ../message.c
```

Also check any hand-written files that were updated together with the generated
files.

```sh
git diff ../build.c ../handler.c ../types.c ../types.h ../conv.c ../conv.h
```

Build Open5GS.

```sh
ninja -C build
```
