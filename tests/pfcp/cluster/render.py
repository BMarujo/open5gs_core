#!/usr/bin/env python3
"""Render only; never call kubectl or change a cluster.
Creates namespace-scoped SMF/UPF lab fixtures. Ancillary NFs/UE are prerequisites.
JSON output is valid Kubernetes YAML and avoids a host PyYAML dependency.
"""
import argparse
import ipaddress
import json
from pathlib import Path
import re
from urllib.parse import urlparse

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--namespace', required=True)
p.add_argument('--lab-id', required=True)
p.add_argument('--role', choices=['namespace', 'smf', 'upf-a', 'upf-b', 'upf-c'], required=True)
p.add_argument('--smf-image')
p.add_argument('--upf-image')
p.add_argument('--tools-image')
p.add_argument('--nrf-uri')
p.add_argument('--smf-n4')
p.add_argument('--output', required=True, type=Path)
a = p.parse_args()
assert re.fullmatch(r'open5gs-dnn-lab-[a-z0-9][a-z0-9-]*', a.namespace)
assert len(a.namespace) <= 63
# The stable pool ID is "lab-<id>-<letter>" and must fit 64 octets.
assert re.fullmatch(r'[a-z0-9][a-z0-9-]{0,57}', a.lab_id)
labels = {'app.kubernetes.io/part-of': 'open5gs-dnn-lab', 'open5gs.lab/id': a.lab_id}


def meta(name):
    return {'name': name, 'namespace': a.namespace, 'labels': labels}


if a.role == 'namespace':
    objects = [{'apiVersion': 'v1', 'kind': 'Namespace',
                'metadata': {'name': a.namespace, 'labels': labels}},
               {'apiVersion': 'networking.k8s.io/v1', 'kind': 'NetworkPolicy',
                'metadata': meta('dnn-lab-only'), 'spec': {'podSelector': {},
                    'policyTypes': ['Ingress', 'Egress'],
                    'ingress': [{'from': [{'podSelector': {}}]}],
                    'egress': [{'to': [{'podSelector': {}}]},
                               {'ports': [{'protocol': 'UDP', 'port': 53}, {'protocol': 'TCP', 'port': 53}]}]}}]
else:
    assert a.tools_image and a.smf_image and a.upf_image
    assert all('latest' not in s for s in [a.tools_image, a.smf_image, a.upf_image])
    nf = 'smf' if a.role == 'smf' else 'upf'
    cfg = {'global': {'max': {'ue': 16, 'peer': 16}},
           'logger': {'level': 'debug', 'file': {'path': '/evidence/nf.log'}}}
    pools = [{'subnet': subnet, 'gateway': gateway, 'dnn': dnn} for dnn, subnet, gateway in [
        ('internet', '10.45.0.0/24', '10.45.0.1'), ('edge1', '10.48.0.0/24', '10.48.0.1'),
        ('nomatch', '10.50.0.0/24', '10.50.0.1')]]
    if nf == 'smf':
        assert a.nrf_uri and a.nrf_uri.startswith(('http://', 'https://'))
        host = urlparse(a.nrf_uri).hostname or ''
        assert host.endswith('.' + a.namespace + '.svc') or host.endswith('.' + a.namespace + '.svc.cluster.local'), 'Use the isolated namespace NRF service'
        cfg[nf] = {'sbi': {'server': [{'address': '$POD_IP', 'port': 7777}],
                          'client': {'nrf': [{'uri': a.nrf_uri}]}},
                   'info': [{'s_nssai': [{'sst': 1, 'dnn': ['internet', 'edge1', 'nomatch']}]}],
                   'pfcp': {'server': [{'address': '$POD_IP'}], 'learn_upf_dnn': True},
                   'gtpc': {'server': [{'address': '$POD_IP'}]},
                   'gtpu': {'server': [{'address': '$POD_IP'}]},
                   'session': pools, 'dns': ['8.8.8.8']}
    else:
        address = ipaddress.IPv4Address(a.smf_n4)
        assert not address.is_loopback and not address.is_unspecified and not address.is_multicast
        letter = a.role[-1]
        pool = dict(pools[0 if letter == 'a' else 1], pool_id='lab-' + a.lab_id + '-' + letter, dev='tunlab')
        cfg[nf] = {'pfcp': {'server': [{'address': '$POD_IP'}], 'advertise_ue_ip_pools': True,
                           'client': {'smf': [{'address': str(address)}]}},
                   'gtpu': {'server': [{'address': '$POD_IP'}]}, 'session': [pool]}
    config_name = 'dnn-' + a.role
    objects = [{'apiVersion': 'v1', 'kind': 'ConfigMap', 'metadata': meta(config_name),
                'data': {'template.json': json.dumps(cfg, indent=2)}}]
    init_code = ('import os,pathlib; text=pathlib.Path("/template/template.json").read_text(); '
                 f'pathlib.Path("/config/{nf}.yaml").write_text(text.replace("$POD_IP",os.environ["POD_IP"]))')
    volumes = [{'name': 'template', 'configMap': {'name': config_name}},
               {'name': 'config', 'emptyDir': {}}, {'name': 'evidence', 'emptyDir': {}}]
    mounts = [{'name': 'config', 'mountPath': '/etc/open5gs', 'readOnly': True},
              {'name': 'evidence', 'mountPath': '/evidence'}]
    security = {'runAsUser': 0, 'capabilities': {'drop': ['ALL']}}
    if nf == 'upf':
        volumes.append({'name': 'tun', 'hostPath': {'path': '/dev/net/tun', 'type': 'CharDevice'}})
        mounts.append({'name': 'tun', 'mountPath': '/dev/net/tun'})
        security['capabilities']['add'] = ['NET_ADMIN']
    pod = {'apiVersion': 'v1', 'kind': 'Pod', 'metadata': meta(config_name), 'spec': {
        'restartPolicy': 'Never', 'automountServiceAccountToken': False,
        'initContainers': [{'name': 'config', 'image': a.tools_image, 'imagePullPolicy': 'Never',
            'command': ['python3', '-c', init_code],
            'env': [{'name': 'POD_IP', 'valueFrom': {'fieldRef': {'fieldPath': 'status.podIP'}}}],
            'volumeMounts': [{'name': 'template', 'mountPath': '/template', 'readOnly': True},
                             {'name': 'config', 'mountPath': '/config'}]}],
        'containers': [{'name': nf, 'image': a.smf_image if nf == 'smf' else a.upf_image,
            'imagePullPolicy': 'Never', 'securityContext': security, 'volumeMounts': mounts},
            {'name': 'tools', 'image': a.tools_image, 'imagePullPolicy': 'Never',
             'command': ['sleep', 'infinity'],
             'securityContext': {'runAsUser': 0, 'capabilities': {'add': ['NET_ADMIN', 'NET_RAW']}},
             'volumeMounts': [{'name': 'evidence', 'mountPath': '/evidence'},
                              {'name': 'config', 'mountPath': '/config', 'readOnly': True}]}],
        'volumes': volumes}}
    objects.append(pod)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps({'apiVersion': 'v1', 'kind': 'List', 'items': objects}, indent=2) + '\n')
