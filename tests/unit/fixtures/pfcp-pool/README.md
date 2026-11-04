# Independent PFCP pool fixtures

These Setup Request/Response packets are assembled by
[pfcp-pool-fixtures.py](../../pfcp-pool-fixtures.py) using literal TS 29.244
V19.5.0 types and Python's `struct`, without the Open5GS encoder. The C header
[pfcp-pool-fixtures.h](../../pfcp-pool-fixtures.h) contains the same PFCP bytes;
`pool-setup.pcap` adds IPv4/UDP framing with link type 101 (raw IP).

Each packet contains two groups (233), identities (177) `edge1-v4-a`, opaque
`41 00 42`, and `internet`; NI (22) `edge1`/`internet`; two S-NSSAIs (257);
IP Version (258) both families / IPv4. Response additionally contains Cause 1.
The 12-byte UP Function Features value exercises extension-octet preservation.

Regenerate into a temporary directory, then compare all four artifacts:

```sh
python3 tests/unit/pfcp-pool-fixtures.py /tmp/pfcp-independent-fixtures
cmp tests/unit/pfcp-pool-fixtures.h /tmp/pfcp-independent-fixtures/pfcp-pool-fixtures.h
cmp tests/unit/fixtures/pfcp-pool/pool-request.hex /tmp/pfcp-independent-fixtures/pool-request.hex
cmp tests/unit/fixtures/pfcp-pool/pool-response.hex /tmp/pfcp-independent-fixtures/pool-response.hex
cmp tests/unit/fixtures/pfcp-pool/pool-setup.pcap /tmp/pfcp-independent-fixtures/pool-setup.pcap
tshark -v
tshark -G fields
tshark -r tests/unit/fixtures/pfcp-pool/pool-setup.pcap -V
```

Validated with TShark 4.2.2. It decodes pools, identities, NI, slices and families;
it reports extension feature octets as undecoded notes, not malformed packets.
The C tests additionally encode the decoded messages and compare their entire
TLV bodies with these independent bytes. This is codec evidence, not a live
association or Release 19 interoperability certification.

| Artifact | SHA-256 |
|---|---|
| `pool-request.hex` | `38b46869220ae0c08c481a578c112a57bc7f2af12f001cf44fa242b2d2cfca65` |
| `pool-response.hex` | `9bc339f10bceabc3d15df3f5b1e0c84f93abca7ef4e2b19af1d5fe4250a964a9` |
| `pool-setup.pcap` | `3cb1dcf786b7f57dd0cda98dd338091d608a46bf33a772d8b6142fd1e38a6dc1` |
