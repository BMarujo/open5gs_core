#!/usr/bin/env python3
"""Check independent TShark fields against effective integration YAML.

PDML CONFIG_YAML_OR_DIRECTORY OUTPUT_JSON. Requires PyYAML (tools image).
Accepts the actual number of captured retransmissions; checks every advertised
configured group rather than imposing a transport-dependent packet minimum.
"""
import json
from pathlib import Path
import sys
from xml.etree import ElementTree as ET
import yaml

pdml, configs, output = (Path(p) for p in sys.argv[1:4])
expected = {}
for path in ([configs] if configs.is_file() else configs.glob('*.yaml')):
    config = yaml.safe_load(path.read_text())
    for pool in config.get('upf', {}).get('session', []):
        if 'pool_id' not in pool or 'dnn' not in pool:
            continue
        family = 6 if ':' in pool['subnet'] else 4
        expected[pool['pool_id']] = (pool['dnn'], family)
assert expected, 'No configured advertised pools'
records = []
for packet in ET.parse(pdml).getroot().findall('packet'):
    pfcp = packet.find("proto[@name='pfcp']")
    if pfcp is None:
        continue
    for group in pfcp.findall('field'):
        typ = group.find("field[@name='pfcp.ie_type']")
        if typ is None or typ.get('show') != '233':
            continue
        identity = group.find(".//field[@name='pfcp.ue_ip_address_pool_identity']")
        if identity is None:
            continue  # Negative independent malformed fixtures can omit ID.
        name = bytes.fromhex(identity.get('value')).decode('ascii')
        if name not in expected:
            continue  # Negative independent fixtures have their own assertions.
        dnn, family = expected[name]
        assert packet.find(".//proto[@name='_ws.malformed']") is None, name
        assert group.find(".//field[@name='pfcp.network_instance']").get('show') == dnn
        assert group.find(".//field[@name='pfcp.ue_ip_address_pool_length']").get('show') == str(len(name))
        v4 = group.find(".//field[@name='pfcp.ip_version.v4']").get('show')
        v6 = group.find(".//field[@name='pfcp.ip_version.v6']").get('show')
        assert (v4, v6) == (('True', 'False') if family == 4 else ('False', 'True'))
        records.append({'frame': packet.find(".//field[@name='frame.number']").get('show'),
                        'type': pfcp.find(".//field[@name='pfcp.msg_type']").get('show'),
                        'pool_id': name, 'dnn': dnn, 'family': family})
assert records, 'No configured advertisements decoded'
seen = {r['pool_id'] for r in records}
assert seen == set(expected), (seen, set(expected))
result = {'result': 'PASS', 'groups': len(records), 'records': records,
          'scope': 'Every effective configured pool appears with correct ID/DNN/family; negative fixtures checked separately'}
output.write_text(json.dumps(result, indent=2))
print(json.dumps({'result': 'PASS', 'groups': len(records), 'pool_ids': sorted(seen)}))
