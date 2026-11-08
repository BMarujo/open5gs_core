# Original versus learned-DNN core: performance comparison and publication

2026-11-08. The user requested a controlled Containernet comparison, publication
of the latest source image, and rollout to both Containernet and the physical
cluster. The performance comparison passed its predefined engineering screen.
It found no added bitrate cap or material receiver-throughput regression in the
tested single-UE scenario. This is not a guarantee of 100% bug freedom or exact
performance identity on every topology, hardware platform or offered load.

Evidence root: `/home/marujo/open5gs_perf_comparison_results_20261108T175906Z`.
The pilot is `pilot-01`; the final five-pair campaign is `comparison-01` and its
independent analysis is `analysis-02`. Failed preparation/operational attempts
are retained separately and do not count as successful measurements.

## What changed, and what did not

| Item | Original | New |
|---|---|---|
| Source | `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41` | `384a54f4d9a0096e12fd1d231df8a2e2b524dd4a` |
| Software version | Open5GS 2.8.0 | Open5GS 2.8.0 |
| UPF mapping | Explicit SMF peer address and DNN | Accepted UPF-originated PFCP advertisement; no SMF peer DNN/location mapping |
| Association initiator in this comparison | SMF | UPF |
| Steady-state data path | Same topology, UPF Pod/node, UE/gNB node and DN | Same |
| Subscriber and session QoS | AMBR 1 Gbit/s UL/DL; 5QI 9; ARP priority 8 | Same |
| Build/runtime | Same verified GCC/toolchain, Meson flags, vendored source revisions, Ubuntu base and installed runtime package versions | Same |

The baseline is the actual fork commit before the feature, not a different
Gradiant image or an automatically selected upstream tag. Both images were
freshly built through the same Helm-compatible Dockerfile. The original build
omits only the new selector test target, which does not exist at that commit;
its three existing core/crypt/unit suites passed. The new build passed all four
core/crypt/unit/selector suites. The runtime package manifests compare exactly.

The data-plane source audit confirms byte-identical `src/upf/gtp-path.c`,
`src/upf/n4-handler.c`, `src/smf/n4-build.c`, PFCP bitrate conversion/type code,
the shared GTP-U send function and QER create/update handling. The changed
shared PFCP handler regions concern association/Heartbeat handling. New metadata
is used during association and session selection; no per-packet rate limiter,
new MBR/GBR calculation or new QoS policy was added. Local pool/identity capacity
limits are metadata storage limits, not bitrate limits.

The feature also contains the documented protocol/recovery corrections; it is
not claimed that every failure-path behavior is identical to the baseline.
Allocator, existing data forwarding and legacy selection/fallback policy remain
as documented in the [guide](dynamic-dnn.md) and [ADR](ADR-001-dynamic-dnn-learning.md).

## Controlled experiment

The reusable fixture is
`open5gs_k8s/deployments-containernet/open5gs/performance/{compare.py,analyze.py}`.
It is independent of the placement study and does not read/change that study's
state or historical results. It creates one fresh owned namespace with isolated
MongoDB, discovery, ancillary NFs, SMF, UPF, gNB and test UEs.

Original/new SMF and UPF executables alternate in the **same Pods**, sharing
their fixed Pod IPs and node placement, with only one executable active per NF.
The inactive container waits. This avoids rescheduling onto different workers
between versions. Each active NF has the same one-CPU/384-MiB limit. The gNB and
iperf client/UE are fixed on edge-2, UPF on cloud-2 and the real DN on the cloud
LAN. Other NFs remain on the same Gradiant 2.8.0 images. Actual Pod imageIDs,
resources and generator placement are checked independently for every trial.

Each trial uses a fresh UE with the same subscription/profile and a unique
synthetic IMSI. SMF/UPF processes restart between variants; they remain alive
with unchanged PIDs throughout each trial's measurements. This is an established
session performance comparison, not a session-preserving upgrade/churn test.
The operator address pool, DNN, QoS and routing match. A machine comparison of
configuration excludes only the declared PFCP mapping/advertisement switches,
association initiator and explicit announcement identity; all other fields agree.

The network profile is unchanged: backhaul 200 Mbit/s / 10 ms, edge-1 access
100 Mbit/s / 10 ms, edge-2 access **50 Mbit/s / 30 ms**, zero configured loss/jitter
and 1,000-packet queues. Read-only snapshots verify all six actual TC endpoint
profiles before/after. No TC setting was changed. The corresponding real cloud
DN iperf3 listener on port 5209 was unused before the campaign and was not replaced.

The five pairs alternate AB/BA order. Every trial performs a short excluded TCP
warm-up, then TCP UL/DL (10 measured seconds, 2-second omit) and UDP UL/DL
(8 seconds, 40 Mbit/s offered, 1,200-byte datagrams). Iperf is in a UE Pod sidecar
sharing the TUN/network namespace. Both an explicit DN route through `uesimtun0`
and interface/source binding prevent measurement through the ordinary Pod path.
TUN/NAT counters and actual network configuration are retained. Continuous
captures include only N4/N2 control traffic, avoiding full-speed capture overhead.

The recorded plan precedes measurement. Its screen is a paired geometric
new/original receiver-throughput ratio, with a 90% Student-t interval contained
in `[0.90, 1.10]`; UDP loss difference must also stay within one percentage point.
This is a bounded engineering screen, not a general certification standard or
proof of identical packet scheduling. Five pairs are a small, topology-specific
sample. The successful pilot is excluded from final statistics.

## Measured results

**PASS: 10 accepted PDU sessions, 40 iperf measurements and five complete pairs.**

Receiver throughput is used consistently; sender offered throughput can exceed
delivered throughput while TCP fills queues. All raw sender/server JSON is kept.

| Traffic | Original mean Mbit/s | New mean Mbit/s | Paired change | 90% interval for change |
|---|---:|---:|---:|---:|
| TCP UL | 44.697 | 44.705 | +0.0171% | −0.0024% to +0.0366% |
| TCP DL | 44.670 | 44.670 | −0.0002% | −0.0010% to +0.0007% |
| UDP UL, offered 40 Mbit/s | 39.589 | 39.589 | −0.0002% | −0.0172% to +0.0168% |
| UDP DL, offered 40 Mbit/s | 40.008 | 40.011 | +0.0074% | −0.0088% to +0.0236% |

All four intervals pass the predefined screen and are much tighter than its
10% band. UDP loss is **0% in both directions and both versions**. Mean UDP
jitter is 0.0326 → 0.0144 ms UL and 0.0187 → 0.0151 ms DL; this small measured
variation is not claimed as an improvement caused by the feature. Finite UDP
receiver accounting includes path/drain time, so its reported UL mean need not
equal the exact offered rate even when all packets arrive.

TCP retransmissions were not zero or identical: UL counts were
`61,74,75,68,68` original versus `74,74,71,70,72` new; DL counts were
`74,72,71,71,71` versus `71,73,72,70,74`. They are retained with raw interval/RTT
data. No claim of exact counter/latency equality is made. The constrained queues
and TCP dynamics are part of the common test path, not a new configured core cap.

## Independent session and QoS evidence

The external TShark/raw TLV analyzer imports no core encoder. For **each** of
the ten sessions it verifies:

- Exactly one first PFCP Session Establishment Request to the expected, fixed
  UPF, Request Accepted response and **zero PFCP retransmissions**.
- Identical Create QER content after excluding generated QER ID: Gate Status
  `00`, Maximum Bitrate `00000f424000000f4240`, QFI `01`. Both UL/DL MBR values
  are 1,000,000 kbit/s, i.e. **1 Gbit/s**.
- Identical NGAP Session-AMBR UL/DL **1,000,000,000 bit/s**, 5QI **9** and ARP
  priority **8**. Thus the session's signalled rate/QoS parameters did not change.
- Correct DNN/UE IP/SUPI/PSI association, stable SMF/UPF Pod UIDs and no container
  restart during the trial. New selections log `source[learned] reason[dnn]`.

These 1-Gbit/s values were deliberately configured in the common synthetic
subscription and are present in the original too; they were not introduced by
dynamic learning. Actual delivered rate still depends on configured subscriber
QoS, CPU, RAN/generator and the available network path. This 50-Mbit/s access
test does not establish maximum UPF throughput at 1 Gbit/s or above.

Key artifacts: `analysis-02/{result.json,samples.csv,samples.json,sessions-qos.json,n4.json,ngap-qos.json,image-provenance.json}`,
`comparison-01/plan.json`, per-trial iperf JSON/configuration/metrics, control
PCAPs, source/package audits and TC profile comparison. Failed attempts are
separate from the final passing pilot/campaign.

## Publication and deployments

The updated core/tests were committed and pushed as
[`384a54f4d9a0096e12fd1d231df8a2e2b524dd4a`](https://github.com/BMarujo/open5gs_core/commit/384a54f4d9a0096e12fd1d231df8a2e2b524dd4a).
Core-input SHA-256 remains
`d8e528dfc3abe4c66856b8f75e364d8bac1758eebbff2b61ab9c52781bf5242c`.
The fresh Helm-compatible public image is:

```text
docker.io/bmarujo/open5gs:dnn-384a54f4d9a0
docker.io/bmarujo/open5gs@sha256:a05e25b5cc4e39defa42b87fed82bcd0ace632ce477a53059f6ebee6fdffdb8b
```

Anonymous manifest retrieval verifies the digest. The original baseline image
`bmarujo/open5gs:baseline-2c80f8f2ba9d` was kept local and imported into the four
owned emulated nodes, rather than published. The two runtime package manifests
are identical. New-image build tests, runtime dependency checks and actual
benchmark imageIDs are recorded. The final analyzer hashes archived OCI index,
manifest and config blobs and matches each trial's actual CRI index/config-ID
form against them. It also asserts identical active NF resource limits. The
baseline OCI index is `sha256:459429ac132921efa87d566854bd8ec44b2d05e6e37fd4894f23d4b7378c4589`;
its CRI config ID is `sha256:5eca3e39264da8c0ea95f48b13d3c7197baf6e88cd274fdbb5fb4c9e1b5ea717`.
These are different OCI identities for the same recorded image, not different
baseline builds. Earlier analysis output is retained unchanged. The initial BuildKit attempt wrongly treated
a bare Docker config ID as an image repository; the verified local toolchain
tag was used for the successful builds and the reproduction guide was corrected.

Repository main/edge/benchmark image pins are updated. The benchmark ownership
guard accepts the exact old/new known tag+digest pairs, retaining cleanup
compatibility while rejecting mixed pairs/wrong digests. Static deployment tests
pass. Historical dynamic-DNN reanalysis derives its expected known digest from
the run's recorded values instead of relabelling old evidence as new.

Live rollouts use backed-up **actual Helm values**, changing only SMF, UPF and
UPF init-container image fields; rendered configuration/resources are otherwise
identical. Containernet release `open5gs` advanced **1 → 2**, and the physical
release **4 → 5**, using unchanged chart 2.3.4. The other nine NF image versions
remain pinned to Gradiant 2.8.0. Normal core NAS/data smoke results and the
physical recovery details are recorded in the same evidence bundle. Both final
normal-core smoke tests **PASS**: fresh accepted Internet PDU session, learned-DNN
selection and three UE gateway echo replies, followed by exact owned-subscriber
and namespace cleanup. Final verification finds all eleven active NF Pods Ready
in each cluster and actual SMF/UPF imageIDs equal to the published digest.
Physical smoke attempts 01/02 failed; attempt 03 passes after the operational
recovery below. This is gateway/tunnel smoke, not physical throughput testing.

Physical deployment exposed existing unavailable-node state. An old SMF Pod
had been terminating on `atcll-5g` since 09:18, before this task. Recreate waited
for it indefinitely. A temporary RollingUpdate strategy with zero surge allowed
the new SMF on a Ready node; the original Recreate strategy was then restored.
No orphan Pod was force-deleted or unavailable node modified.

The first physical smoke used a stale cached SMF endpoint. A later attempt
exposed stale BSF/NRF routing: the Gradiant Services published terminating,
non-serving endpoints as ready because `publishNotReadyAddresses` was true.
Only NRF/SMF/BSF SBI Services were guarded with that field false, retaining the
old Pods but excluding their dead endpoints. PCF/SMF/SCP/AMF runtime state was
refreshed with unchanged images/configuration. This is an operational workaround,
not a feature/protocol source fix. The repository supplies a read-only check and
explicit, UID/Helm-owner-guarded apply/restore tool at
`deployments/open5gs/scripts/guard-sbi-services.py`.

Because the upstream chart hardcodes the Service setting, a subsequent Helm
upgrade can restore it. Until the unavailable-node/orphan problem is resolved,
run the documented guard check/apply after upgrades. Restore it only when the
old endpoints are safe to publish again. Physical smoke failures and their
diagnostics remain recorded; they are not part of the passing performance sample.

## Reproduction, rollback and limits

Use the deployment repository's adjacent performance README and fresh result
directories. Never overwrite the pilot/campaign or reset global TC/networking.
The fixture deletes only its labelled namespace using its recorded UID. Normal
core smoke uses exact synthetic subscriber IDs plus an ownership tag for removal;
operator subscribers/database volumes remain intact.

Image rollback is to the saved physical revision **4** or Containernet revision
**1**, after inspecting current history; it restarts affected NFs and can lose
sessions. The existing orphan can block Recreate during rollback too; use the
same documented controlled strategy transition rather than force-deleting a Pod
on an unfenced node. Service guard rollback restores only its three recorded
flags with matching object UIDs, independently of image rollback.

NOT RUN: high-rate/Gbit saturation, multi-UE scale, churn/soak, physical-cluster
throughput equivalence, real radio behavior, IPv6 throughput and other network
profiles. Prior local IPv6/slice functional tests remain separately valid.
Strict 4B remains excluded. The result supports trusting the tested normal data
path and unchanged QoS parameters; it cannot prove universal identity or no bugs.
