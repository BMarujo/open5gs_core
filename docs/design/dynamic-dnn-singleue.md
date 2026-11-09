# Original/new core: one UE without emulated network rate limits

2026-11-09. The user requested actual throughput with exactly one UE, comparing
the pre-feature original core against the current dynamic-DNN version on an
unshaped Containernet network.

Evidence root: `/home/marujo/open5gs_singleue_unshaped_results_20261109T034153Z`.

## Method and controls

The test reuses the original source `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`
and current executable source `384a54f4d9a0096e12fd1d231df8a2e2b524dd4a`, both
Open5GS 2.8.0, with the same recorded build/runtime dependencies. No new core
binary or image is required. The test driver now also accepts `--ues 1`; its
previous four-UE mode is retained.

One UE and one TCP flow (`iperf3` default parallelism 1) are active per trial.
Upload and download are measured separately for 20 seconds after a two-second
omit and an excluded warm-up. Five paired original/new trials alternate order.
Every trial uses a fresh PDU session; SMF/UPF run inside the same fixed Pods and
resource/placement configuration for both variants. UE/gNB are on edge-2, UPF on
cloud-2, and the DN on the cloud LAN. The source IP/interface and DN route bind
traffic to the UE TUN, not an ordinary Pod shortcut.

The six owned HTB/netem stacks are detached for the command and independently
verified as noqueue with no rate classes. Their saved effective profiles are
restored in the guarded wrapper's finally block, with a native durable recovery
journal and controller lock. No topology YAML profile is changed. Counters and
queued packets cannot be restored. No TCP application rate cap is requested.

Both variants have the same non-binding 10-Gbit/s AMBR/MBR. Inner CPU quotas are
removed from SMF/UPF/gNB/UE/iperf, while the same real host, outer Docker resource
budgets and CPU sets remain. This is a network without artificial TC shaping,
not infinite hardware or software-radio capacity. The independent analyzer
checks first PFCP requests/Cause/retries, IE 233 in the learned case, equal
NGAP/QER QoS, actual OCI identities, placement/resources and throughput.

## Environment recovery

The topology was stopped at entry: all seven old node/DN containers had exited
137 and the runtime directory was absent. Each stopped container filesystem was
saved as a private local Docker checkpoint, with its original inspect metadata,
before the supported ownership-guarded stale cleanup/start. Checkpoint images
are not published. Persistent operator MongoDB data was retained and backed up
while offline before recovery. The normal deployment is recovered from its
previous saved actual Helm values before the separate benchmark namespace runs.
That cold recovery necessarily changes Kubernetes Pod UIDs relative to the old
stopped cluster; preservation during the benchmark is checked from the recovered
normal deployment's new baseline. The physical cluster is outside this test.

## Results

**Executed: ten accepted sessions and twenty TCP measurements; one UE and one flow at a time.**

The paired throughput screen result is **INCONCLUSIVE**. Warm-ups are excluded.

| Receiver TCP throughput | Original mean Mbit/s | New mean Mbit/s | Paired change | 90% interval for change |
|---|---:|---:|---:|---:|
| UL | 392.219 | 375.964 | -3.864% | -13.949% to +7.403% |
| DL | 402.554 | 431.753 | +5.812% | -12.544% to +28.021% |

| Receiver TCP range across five repetitions | Original Mbit/s | New Mbit/s |
|---|---:|---:|
| UL | 332.424–414.755 | 363.339–392.338 |
| DL | 388.132–421.006 | 381.476–585.683 |

These are the measured rates for one UE, not the four-UE aggregate from the
earlier run. Exact throughput equality is not claimed; individual samples and
the paired uncertainty are retained. The screen was declared before measurement:
90% paired log-ratio interval inside [0.90,1.10]. The intervals are too wide
to establish equivalence at that margin. This does not establish a source-caused
regression or an improvement either. No sample was removed or replaced. In
particular, original UL repetition 5 was 332.424 Mbit/s, while new DL repetition
5 was 585.683 Mbit/s; those remain in the means and intervals. Determining the
cause, or estimating a more stable maximum, requires a separate controlled
CPU/simulator/TCP investigation rather than attributing the difference to DNN
learning from these measurements alone.

**Protocol/control checks PASS:** ten first PFCP Session Establishment Requests
to the expected UPF, ten accepted responses, zero PFCP retransmissions and no
extra establishments hidden outside acceptance windows. Both variants signal
the same 10-Gbit/s UL/DL AMBR/MBR, 5QI 9, ARP priority 8, QFI and Gate Status.
Five new UPF-initiated Setup Requests carry IE 233 and DNN `internet`; their
selections log learned DNN metadata with no static peer DNN/location mapping.

N4 capture drop counters are zero on SMF/UPF. The recorded gNB control-capture
drop statistic is: `NOT RECORDED (empty shutdown statistics)`.
All ten expected NGAP QoS frames were independently found. Actual imageIDs
match hashed OCI manifests/configs; Pods, placement and resource budgets are
stable through the trials. Outer CPU/throttling evidence is in `analysis-01/cpu.json`.

**Final preservation checks PASS:** all six original TC profiles restored,
native recovery journal cleared, owned benchmark namespace removed; all thirteen
normal-deployment Pods retain their post-recovery UIDs, imageIDs and restart
counts and are Ready. All four K3s nodes are Ready. No physical cluster change.
The previous stopped node filesystems remain in seven local checkpoint images;
persistent MongoDB has its offline backup and was reused by normal recovery.

Evidence: `campaign-01/plan.json`, per-trial JSON/commands/configs/captures,
`campaign-tc/{before.json,unshaped-before-command.json,unshaped-after-command.json,restoration-result.json}`,
`analysis-01/{result.json,samples.csv,samples.json,sessions-qos.json,associations.json,image-provenance.json,cpu.json}`,
`final-verification.json`, recovery logs/checkpoints and the SHA256 manifest.
The normal core and emulation are left running; temporary test resources are removed.

NOT RUN: several TCP streams in the same UE, unpaced UDP, real radio, IPv6
throughput, long soak and a standalone hardware-capacity benchmark. No new NF
source behavior, core compilation or image publication was needed.

## Reproduction and scope

Use the deployment repository's
`deployments-containernet/open5gs/performance/README.md`: the same `unshaped.py`
wrapper, `multiue.py --ues 1 --protocols tcp --pairs 5 --tcp-seconds 20`, and
`analyze-multiue.py` with the recorded image archive and TC evidence. Use fresh
result directories and retain failed attempts. This is a single-UE, single-TCP-
flow virtual path measurement, not standalone UPF hardware capacity or a real
radio benchmark. Multiple TCP streams, unpaced UDP, IPv6 throughput and long
soak are outside this run. The earlier four-UE campaign and UDP generator OOM
remain separate historical evidence.
