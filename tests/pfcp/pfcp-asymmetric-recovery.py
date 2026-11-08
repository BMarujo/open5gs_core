#!/usr/bin/env python3
"""Actual SMF + actual UPF, selective kernel packet loss, no process restart.

BUILD OUTPUT smf|upf|both up|cp stalled|recover|brief. Run through
run-pfcp-recovery.sh so control.sock reaches the isolated nftables helper.
"""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time
import traceback
import yaml

build, out = map(Path, sys.argv[1:3])
direction, initiator, expectation = sys.argv[3:6]
assert direction in ('smf', 'upf', 'both')
assert initiator in ('up', 'cp') and expectation in ('stalled', 'recover', 'brief', 'recover-off')
features = expectation!='recover-off'
if not features: expectation='recover'
retry_ms=int(os.getenv('PFCP_RETRY_MS','1000'))
assert 100<=retry_ms<=2000
processes = []
events = []

def event(name, **data):
    events.append(dict(time=time.time(), event=name, **data))

def control(action):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(str(out/'control.sock'))
        sock.sendall((json.dumps({'action': action, 'direction': direction})+'\n').encode())
        result = json.loads(sock.makefile('r').readline())
        assert result['ok'], result
        return result

def text(role):
    log = out/(role+'.log')
    return log.read_text(errors='replace') if log.exists() else ''

def wait(test, timeout=25):
    end = time.monotonic()+timeout
    while time.monotonic()<end:
        assert all(p.poll() is None for p in processes), 'NF exited'
        if test(): return
        time.sleep(.03)
    raise AssertionError('condition timeout')

def start(role, cfg):
    cfg['logger'] = {'level':'debug', 'file': {'path':str(out/(role+'.log'))}}
    path=out/(role+'.yaml'); path.write_text(yaml.safe_dump(cfg))
    p=subprocess.Popen([str(build/f'src/{role}/open5gs-{role}d'),'-c',str(path)],
                       stdout=(out/(role+'.console')).open('w'),stderr=subprocess.STDOUT)
    processes.append(p); wait(lambda:'initialize...done' in text(role))
    return p

global_cfg={'max':{'ue':16,'peer':16}}
smf_cfg={'global':global_cfg,'smf':{
    'time':{'message':{'duration':retry_ms}},
    'sbi':{'server':[{'address':'127.0.0.4','port':7777}],
           'client':{'nrf':[{'uri':'http://127.0.0.10:7777'}]}},
    'pfcp':{'server':[{'address':'127.0.0.4'}],'learn_upf_dnn':features},
    'gtpc':{'server':[{'address':'127.0.0.4'}]},'gtpu':{'server':[{'address':'127.0.0.4'}]},
    'session':[{'subnet':'10.48.0.0/24','gateway':'10.48.0.1','dnn':'edge1'}],
    'dns':['8.8.8.8']}}
upf_cfg={'global':global_cfg,'upf':{'time':{'message':{'duration':retry_ms}},
    'pfcp':{'server':[{'address':'127.0.0.7'}],'advertise_ue_ip_pools':features},
    'gtpu':{'server':[{'address':'127.0.0.7'}]},
    'session':[{'subnet':'10.48.0.0/24','gateway':'10.48.0.1','dnn':'edge1','pool_id':'edge1-a','dev':'pooltest'}]}}
if initiator=='up':upf_cfg['upf']['pfcp']['client']={'smf':[{'address':'127.0.0.4'}]}
else:smf_cfg['smf']['pfcp']['client']={'upf':[{'address':'127.0.0.7'}]}
with (out/'n4.pcapng').open('wb') as capture_output:
    capture=subprocess.Popen(['tshark','-i','lo','-f','udp port 8805','-w','-'],
                             stdout=capture_output,stderr=(out/'capture.log').open('w'))
time.sleep(.4)
status=1; result={}
try:
    smf=start('smf',smf_cfg);upf=start('upf',upf_cfg)
    wait(lambda:('heartbeat confirmed peer[127.0.0.7] generation[1]' if features
                 else 'PFCP associated') in text('smf'))
    wait(lambda:'PFCP associated' in text('upf'))
    event('initial-association',smf_pid=smf.pid,upf_pid=upf.pid)
    # Independent, valid but unmatched Setup Responses exercise the receive
    # rejection path. UPF events belong to its pool, not the heap allocator.
    # Repeated drops must return events to that pool and leave both NFs alive.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.8', 8805))
        body=bytes.fromhex('003c0005007f00000800130001010060000400000001')
        for i in range(128):
            wire=bytes([0x20,6])+len(body+bytes(4)).to_bytes(2,'big')+(100000+i).to_bytes(3,'big')+b'\0'+body
            for address in ('127.0.0.4','127.0.0.7'):
                probe.sendto(wire,(address,8805))
            time.sleep(.002)
    wait(lambda:all(text(r).count('Unmatched PFCP Setup response')>=128 for r in ('smf','upf')))
    event('unmatched-setup-responses-discarded',per_nf=128)
    offsets={r:len(text(r)) for r in ['smf','upf']}
    target='upf' if direction=='upf' else 'smf'
    control('drop_once' if expectation=='brief' else 'drop')
    event('fault-started',direction=direction)
    if expectation=='brief':
        time.sleep(12)
        assert all('PFCP de-associated' not in text(r)[offsets[r]:] for r in offsets)
    else:
        wait(lambda:'PFCP de-associated' in text(target)[offsets[target]:])
        if direction!='both':
            other='upf' if target=='smf' else 'smf'
            assert 'PFCP de-associated' not in text(other)[offsets[other]:]
        event('target-timeout')
    stats=control('clear'); (out/'fault-counter.json').write_text(json.dumps(stats['before'],indent=2))
    dropped=sum(expr.get('counter',{}).get('packets',0)
                for entry in stats['before']['nftables']
                for expr in entry.get('rule',{}).get('expr',[]))
    assert dropped>0, stats
    if expectation=='brief':assert dropped==1,dropped
    restore=time.time(); event('fault-cleared')
    if expectation=='stalled':
        time.sleep(12)
        assert 'PFCP associated' not in text(target)[offsets[target]:]
        assert 'generation[2]' not in text('smf')[offsets['smf']:]
        result['characterization']='GAP_REPRODUCED: real peer stays up; no automatic reassociation'
    elif expectation=='recover':
        wait(lambda:'PFCP associated' in text(target)[offsets[target]:],30)
        if features:
            wait(lambda:'heartbeat confirmed peer[127.0.0.7]' in text('smf')[offsets['smf']:],30)
        else:
            assert 'PFCP learned pools peer' not in text('smf')
        wait(lambda:all(text(r).rfind('PFCP associated')>text(r).rfind('PFCP de-associated')
                        for r in offsets),30)
        event('reassociated-and-relearned',delay=time.time()-restore)
        stable={r:len(text(r)) for r in offsets}
        time.sleep(11)
        assert all('PFCP de-associated' not in text(r)[stable[r]:] for r in offsets)
        result['characterization']='RECOVERY_PASS'
    else:result['characterization']='BRIEF_LOSS_PASS: association and mapping retained'
    assert all(p.poll() is None for p in processes)
    event('finished-with-original-processes',smf_pid=smf.pid,upf_pid=upf.pid)
    status=0
except Exception:
    (out/'failure.txt').write_text(traceback.format_exc());traceback.print_exc()
finally:
    control('clear')
    capture.send_signal(signal.SIGINT);capture.wait(timeout=5)
    for p in reversed(processes):
        if p.poll() is None:p.terminate();p.wait(timeout=10)
    exits=[p.returncode for p in processes]
    if any(code!=0 for code in exits):status=1
    subprocess.run(['tshark','-r',str(out/'n4.pcapng'),'-Y','pfcp','-T','fields',
                    '-e','frame.number','-e','frame.time_epoch','-e','ip.src','-e','ip.dst','-e','udp.payload'],
                   stdout=(out/'n4.tsv').open('w'),check=True)
    packets=[]
    for row in (out/'n4.tsv').read_text().splitlines():
        frame,epoch,src,dst,payload=row.split('\t');wire=bytes.fromhex(payload.replace(':',''))
        record={'frame':int(frame),'time':float(epoch),'src':src,'dst':dst,'type':wire[1],
                'sqn':int.from_bytes(wire[4:7],'big')}
        pos=8
        while pos+4<=len(wire):
            typ=int.from_bytes(wire[pos:pos+2],'big');n=int.from_bytes(wire[pos+2:pos+4],'big')
            if typ==96:record['recovery']=int.from_bytes(wire[pos+4:pos+4+n],'big')
            if typ==233:record['has_pool_information']=True
            if typ==183:record['retention_length']=n
            if typ==184:record['response_flags']=list(wire[pos+4:pos+4+n])
            if typ==19:record['cause']=wire[pos+4]
            pos+=4+n
        packets.append(record)
    recoveries={ip:sorted({p['recovery'] for p in packets if p['src']==ip and 'recovery' in p})
                for ip in ['127.0.0.4','127.0.0.7']}
    assert all(len(v)==1 for v in recoveries.values()),recoveries
    (out/'n4.json').write_text(json.dumps(packets,indent=2))
    result.update(exit=status,direction=direction,initiator=initiator,events=events,
                  capability_features_enabled=features,nf_exit_statuses=exits,
                  constant_recovery_timestamps=recoveries,
                  scope='actual SMF and UPF; no UE sessions')
    (out/'results.json').write_text(json.dumps(result,indent=2))
sys.exit(status)
