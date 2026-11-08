# Original/new core: simultaneous UEs on an unshaped network

2026-11-08. Follow-up requested after the initial 50-Mbit/s access comparison.
This experiment removes the emulation's six owned HTB/netem stacks temporarily,
and compares original `2c80f8f2ba9d` with feature image `384a54f4d9a` using four
simultaneous UEs. The core executable sources/images are unchanged by this
follow-up; only test fixtures and documentation are added.

Evidence root: `/home/marujo/open5gs_multiue_unshaped_results_20261108T222927Z`.
The original/new image build/source provenance remains in the separately sealed
`/home/marujo/open5gs_perf_comparison_results_20261108T175906Z` bundle.

## Controls

Both variants use fixed Pods/IPs, UPF cloud-2, UE/gNB edge-2, and DN cloud. Each
UE has its own DN iperf listener. TCP UL/DL is sent concurrently by all four UEs,
with a common start gate and bound UE TUN source/interface. The five pairs
alternate original/new order; each measured direction lasts 20 seconds after a
two-second omit, following an excluded warm-up. Every trial establishes four
fresh sessions from the same synthetic subscription profile.

The only normalized NF configuration differences remain PFCP static mapping vs
learned advertisement, association initiator and announcement identity. Both
variants use the same 10-Gbit/s subscriber AMBR. It is a common provisioned QoS
value, not an added feature rate limit or a claim that the emulation delivers
10 Gbit/s. Inner CPU limits are removed from SMF/UPF/gNB/UE/iperf; fixed outer
Docker CPU/memory ceilings and CPU sets remain. No operator subscriber, physical
cluster deployment or placement-study state is changed.

The native TC ownership validator checks all six endpoints. A durable native
restoration-intent journal and exclusive controller lock protect the temporary
unshaped period. Exactly those six root stacks are detached. They must have no
rate classes/netem before/after the command, followed by verified restoration
of their saved effective profiles. No YAML profile is edited. TC counters,
queued packets and PRNG state are not restored.

The independent analyzer uses TShark/raw TLVs, not the core encoder. It verifies
first requests/Cause/retries, equal QER and NGAP QoS, hashed OCI identities,
placement/resources, generator stability and concurrent client starts. Primary
performance evidence is aggregate receiver TCP throughput. A 90% paired ratio
interval inside ±10% is the bounded engineering screen. Per-UE samples, fairness
and outer CPU/throttling counters are retained.

## Retained pilot failure

`pilot-01` used four UEs and unpaced UDP (`iperf3 -b 0`) after TCP. TCP aggregate
rates exceeded 380 Mbit/s. During UDP, all four original-core nr-ue simulators
were OOM-killed at their 192-MiB container limits. No original/new UDP comparison
completed. Raw Pod termination status, captures, command timeouts and logs are
retained. Owned namespace cleanup and restoration of all six effective TC
profiles passed. This overload is a generator/fixture limit observed before the
feature variant ran, not evidence of a regression introduced by dynamic DNN.
The main campaign uses uncapped TCP UL/DL; UDP overload is not relabelled PASS.

## Results

**PASS within the predefined aggregate-throughput engineering screen.**

Five pairs, 4 simultaneous UEs, 40 accepted PDU sessions and 80 measured per-UE iperf runs.
Warm-ups and the failed UDP pilot are excluded.

| Aggregate receiver throughput | Original Mbit/s | New Mbit/s | Paired change | 90% interval for change |
|---|---:|---:|---:|---:|
| TCP UL | 390.053 | 389.650 | -0.100% | -1.277% to +1.090% |
| TCP DL | 444.599 | 437.856 | -1.526% | -3.299% to +0.279% |

These are aggregate rates across four concurrent UEs, not a per-UE cap.
Measured counters/rates are not byte-for-byte identical. Both directions pass
the declared ±10% paired screen; the table retains the actual measured change
and uncertainty instead of calling the rates identical.

| Per-UE receiver rates / fairness | Original | New |
|---|---:|---:|
| TCP UL range, Mbit/s | 92.460–103.635 | 92.719–100.806 |
| TCP UL mean Jain fairness (1 = equal share) | 0.99939 | 0.99941 |
| TCP DL range, Mbit/s | 97.413–119.433 | 100.508–122.841 |
| TCP DL mean Jain fairness (1 = equal share) | 0.99691 | 0.99733 |

**PASS: every session has exactly one first PFCP Session Establishment Request**
to the expected fixed UPF, an accepted response and zero PFCP retransmissions.
The analyzer also checks the complete capture for extra establishments outside
the individual acceptance windows. Both N4 captures have zero kernel drops.
The gNB capture shutdown-statistics file is empty: its drop count is **NOT
RECORDED**, while all forty expected NGAP QoS frames were independently found.
NGAP UL/DL AMBR and PFCP UL/DL MBR are identical at **10 Gbit/s**;
5QI 9, ARP priority 8, Gate Status and QFI are identical too.
New selections use learned DNN metadata with no static peer DNN/location mapping.
The five UPF-initiated new Setup Requests carry IE 233, DNN `internet`, IP Version
IE 258 and Pool Identity IE 177; both association directions receive Accepted.

**PASS: final restoration and preservation.** All six saved effective TC
profiles were restored and verified, the native recovery journal was cleared,
and both owned test namespaces were removed. All normal-core Pods retain their
UIDs, imageIDs and restart counts and are Ready. The four K3s nodes are Ready.
No physical-cluster resource was accessed or modified.

Outer CPU/memory budgets remain cloud-1 3 CPU/2 GiB, cloud-2 2 CPU/1 GiB,
edge-2 6 CPU/3 GiB and DN cloud 1 CPU/128 MiB, with unchanged CPU sets.
Across the 80 measured outer-cgroup spans, additional throttled periods total 0.
These diagnostic spans include the synchronized start gate and snapshot overhead;
their CPU averages are not an exact per-packet CPU benchmark. Single-thread
processing, software radio, sockets and host scheduling can still bound rate
even without aggregate quota throttling. The result does not certify maximum
standalone UPF hardware capacity.

Final evidence: `analysis-03/{result.json,samples.csv,samples.json,aggregate.json,
sessions-qos.json,n4.json,ngap-qos.json,image-provenance.json,cpu.json}`,
`campaign-01/plan.json`, all per-UE iperf JSON/commands/resources/control PCAPs,
`campaign-tc/{before.json,unshaped-before-command.json,unshaped-after-command.json,restoration-result.json}`
and `final-verification.json`. TShark version is recorded in `decoder-version.txt`.

Earlier `analysis-01` failed because the new test script confused Pool Identity
type 177 with IP Version type 258. This was an analyzer bug, corrected using the
validated IE mapping. `analysis-02` then exposed absent gNB shutdown statistics;
the final analyzer records these as NOT RECORDED rather than inventing zero.
The passing final analysis is `analysis-03`, using the same unchanged raw
measurements/captures. Both failed analyses are retained; no core fix was needed.

NOT RUN: valid unpaced UDP original/new equivalence (the original pilot hit
the UE memory limit), higher UE counts, multi-UPF distribution under saturation,
long soak, real radio, IPv6 throughput and physical-cluster capacity. No new
core code or image was required; the existing corrected public image is reused.

## Reproduction and scope

Use `open5gs_k8s/deployments-containernet/open5gs/performance/README.md`, including
`unshaped.py`, `multiue.py` and `analyze-multiue.py`, with fresh result directories.
The same topology's `tc-control.sh recover` restores the durable native journal
after a hard kill; never discard it or reset global networking.

This is a four-UE virtual single-UPF capacity comparison, not a real-radio,
large-scale or soak certification. CPU, memory and socket/generator behavior
still limit achievable rate. Unpaced UDP equivalence is not established. Strict
4B, session migration, new QoS policy and data-plane source changes remain absent.
