#!/usr/bin/env python3
"""Verify TShark PDML against configured lab identity/family semantics.

Usage: PDML OUTPUT_JSON. Consumes an external decoder's fields, not Open5GS.
Rejects V4/V6 inversion and malformed positive announcements; negative fixtures
in the same capture may intentionally be marked malformed.
"""
import json
from pathlib import Path
import sys
from xml.etree import ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
records = []
message_types = set()
malformed_negative = []
for packet in root.findall('packet'):
    frame = packet.find(".//field[@name='frame.number']").get('show')
    malformed = packet.find(".//proto[@name='_ws.malformed']") is not None
    if malformed: malformed_negative.append(frame)
    pfcp = packet.find("proto[@name='pfcp']")
    if pfcp is None: continue
    for group in pfcp.findall('field'):
        typ = group.find("field[@name='pfcp.ie_type']")
        if typ is None or typ.get('show') != '233': continue
        identity = group.find(".//field[@name='pfcp.ue_ip_address_pool_identity']")
        if identity is None: continue
        name = bytes.fromhex(identity.get('value')).decode('ascii')
        if '-v4-' not in name and '-v6-' not in name: continue
        assert not malformed, frame
        is_v4 = '-v4-' in name
        v4 = group.find(".//field[@name='pfcp.ip_version.v4']").get('show')
        v6 = group.find(".//field[@name='pfcp.ip_version.v6']").get('show')
        assert (v4, v6) == (('True', 'False') if is_v4 else ('False', 'True')), (frame, name, v4, v6)
        assert group.find(".//field[@name='pfcp.network_instance']").get('show') == 'edge1'
        assert group.find(".//field[@name='pfcp.ue_ip_address_pool_length']").get('show') == str(len(name))
        for flag in ('ueip', 'sset', 'mpas'):
            feature = pfcp.find(".//field[@name='pfcp.up_function_features.%s']" % flag)
            assert feature is not None and feature.get('show') == 'False', (frame, flag)
        msg_type = pfcp.find(".//field[@name='pfcp.msg_type']").get('show')
        message_types.add(msg_type)
        records.append({'frame': frame, 'type': msg_type, 'id': name, 'v4': v4, 'v6': v6})
assert message_types == {'5', '6'}, message_types
assert len(records) >= 14, len(records)
result = {'status': 'PASS', 'verified_groups': len(records), 'message_types': sorted(message_types),
          'intentionally_malformed_frames': malformed_negative, 'records': records}
Path(sys.argv[2]).write_text(json.dumps(result, indent=2))
print(json.dumps({k: v for k, v in result.items() if k != 'records'}, indent=2))
