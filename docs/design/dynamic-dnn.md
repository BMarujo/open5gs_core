# Dynamic DNN learning with the existing SMF selector

## Official upstream integration — 2026-11-10

The fork now includes official upstream `5cb35ead227d25a5ed2161d66faade56a9fd12db`
(157 commits since the feature's upstream base). Open5GS still declares 2.8.0.
See [the merge report](dynamic-dnn-upstream-merge.md) for executed tests and
image/deployment status. Historical reports keep their original source identity.

**Recovery update:** upstream now deletes the affected UPF sessions when that
UPF declares its SMF unreachable after heartbeat expiry. Retention is acknowledged
only while the UPF still has an existing association/session context. Old sessions
on that failed association cannot be assumed to survive. Accepted reassociation
still relearns DNNs and restores eligibility for new sessions. Sessions on other
UPFs are checked separately. No migration or strict selection is introduced.


The [single-UE unshaped TCP follow-up](dynamic-dnn-singleue.md) records fresh
original/new rates and ranges. Its paired equivalence screen is INCONCLUSIVE
because of measured variability; protocol/QoS and TC restoration checks pass.


A subsequent [four-UE unshaped TCP comparison](dynamic-dnn-multiue.md) removes
the 50-Mbit/s access profile, repeats original/new UL/DL in five pairs and records
its own scope, retained UDP generator failure and verified TC rollback.

This fork advertises configured UPF pools during PFCP Association Setup, learns
their DNNs in SMF-owned memory and uses them in the existing DNN selection arm.
A UPF can initiate association and become selectable without a static per-peer
DNN mapping in the SMF. The DNN, subscriber permissions and SMF address pools
must already be provisioned.

The MVP is implemented. Read the [final review](dynamic-dnn-final-review.md) for
executed checks, the latest source/image distinction and remaining validation
limits. [ADR-001](ADR-001-dynamic-dnn-learning.md) records the decisions;
the [clause matrix](dynamic-dnn-conformance-matrix.md) records the scoped Release
19 interpretation. This is not certification of the complete core.

## Configuration

Both switches default to `false` and apply independently:

| Setting | Effect |
|---|---|
| `upf.pfcp.advertise_ue_ip_pools` | Emit configured explicit-DNN pools in UPF Setup Request and successful Setup Response |
| `smf.pfcp.learn_upf_dnn` | Learn a separate snapshot and use it for the selector's DNN comparison |
| `upf.session[].pool_id` | Persistent identity of each advertised pool; required when that entry is advertised |

These are complete example NF sections for an IPv4 test DNN. Replace addresses
with assigned local N4/N3/SBI addresses and configure the remaining NFs, PLMN,
SST 1 and subscriber separately. Provision TUN, routes, forwarding and N6 before
testing. These example addresses are not deployment commands.

```yaml
# smf.yaml: UPF initiates; no client.upf peer mappings or location matches.
logger:
  level: debug
smf:
  sbi:
    server:
      - address: 10.10.0.4
        port: 7777
    client:
      nrf:
        - uri: http://10.10.0.10:7777
  pfcp:
    server:
      - address: 10.10.0.4
    learn_upf_dnn: true
  gtpc:
    server:
      - address: 10.10.0.4
  gtpu:
    server:
      - address: 10.10.0.4
  session:
    - subnet: 10.48.0.0/24
      gateway: 10.48.0.1
      dnn: edge1
  dns:
    - 1.1.1.1
  mtu: 1400
  info:
    - s_nssai:
        - sst: 1
          dnn:
            - edge1
```

```yaml
# upf.yaml
logger:
  level: debug
upf:
  pfcp:
    server:
      - address: 10.10.0.8
    client:
      smf:
        - address: 10.10.0.4
    advertise_ue_ip_pools: true
  gtpu:
    server:
      - address: 10.20.0.8
  session:
    - subnet: 10.48.0.0/24
      gateway: 10.48.0.1
      dnn: edge1
      pool_id: edge1-v4-a
      dev: ogstun
```

For SMF-initiated association, remove UPF `pfcp.client.smf` and add
`smf.pfcp.client.upf: [{address: 10.10.0.8}]`. No peer `dnn` property is needed.
The UPF then advertises in its successful Setup Response. Do not configure both
initiators merely to enable the feature; both directions are supported.

An advertised entry needs a nonempty explicit DNN and a unique pool ID within
that UPF configuration. IDs are 1–64 ASCII bytes from `[A-Za-z0-9._:-]`. Persist
them in configuration; reordering entries or changing N4 must not change them.
Editing an ID is a valid explicit replacement. There is no historical identity
database and no detector for accidental edits across restarts. Generic entries
without DNN are skipped, rather than advertised as universal support.

## Protocol and data lifetime

The pinned profile is TS 29.244 **V19.5.0 / Release 19**; Open5GS software remains
2.8.0 and the PFCP header version remains 1. IE 233 is a grouped UE IP Address
Pool Information IE. It contains repeated identity 177, optional singleton
Network Instance 22, repeated S-NSSAI 257 and optional singleton IP Version 258.
Identity has its own two-byte length; DNN uses the bounded APN-label profile.
IPv4 is bit 1, IPv6 bit 2. The sender emits one group/identity per configured
subnet and derives its family from that subnet. It does not set UEIP or transfer
the complete allocator configuration, CIDR or N3 endpoint in IE 233.

Local receive limits are 16 groups, 16 identities per group, 8 slices per group
and 64 identity bytes. They are implementation limits, not protocol repetition
maxima. Exceeding them suppresses the pool list as `UNSUPPORTED`, without
publishing a truncated list or automatically rejecting association. An invalid
optional group is omitted; independent valid groups may survive as `DEGRADED`.
Structural overruns, missing mandatory Setup fields and rejection Cause are
handled separately. The first singleton occurrence governs. Unknown optional
IEs, spare bits and extension octets after the supported prefix of extendable
IEs are ignored. Fixed-length S-NSSAI remains four bytes.

Validation precedes peer creation/merge and Request Accepted. Only accepted
Setup and successful send/transaction handling publish the new snapshot.
Matched erroneous responses stop request retransmissions and notify the PFCP
application. The message owns its filtered decoding buffer; the peer snapshot
copies all retained pool/DNN/family/slice bytes and survives message destruction.
Static `node->dnn[]` remains separate. Replacement frees the old snapshot;
deassociation, timeout, restart invalidation and peer destruction clear it.
Epoch checks prevent obsolete Setup/Heartbeat transactions from replacing or
invalidating newer state. Setup Recovery Timestamp does not seed restart history.

After a brief lost Heartbeat, the existing transaction can retry successfully
without deassociation. After timeout, learned data is invalidated. A previously
associated SMF/UPF actively retries Setup even if originally passive; accepted
Setup learns the returning peer again. CP recovery requests all-session retention
using empty IE 183 and the UP endpoint acknowledges it with PSREI in IE 184 when
applicable. Selective retention is unsupported. This does not promise recovery
of PDU sessions lost in a UPF crash or migration to another peer.

## Effective mapping and legacy selection

| State | Source for the DNN comparison |
|---|---|
| Learning disabled | Static configuration |
| Accepted Setup without any IE 233 (`ABSENT`) | Static configuration |
| Accepted valid/degraded advertisement | Usable learned groups replace static |
| Invalidated snapshot (`NULL`), unusable or unsupported advertisement | No learned DNN match; no silent static revival |

There is no static/learned union. Groups preserve their pool context. Explicit
family coverage must fit the effective session type; absent family is unknown,
with matching analogous to static DNN configuration. Slice-restricted groups
are retained but excluded from the MVP DNN arm. The S5-C caller selects before
setting the effective type in this baseline; that ordering is unchanged.

The complete selector still uses **DNN OR location**, its existing rotation,
associated-peer round-robin fallback and final first-peer fallback. Therefore a
location match or fallback can select an incompatible peer, and the final
fallback can even return an unavailable peer. Strict selection (4B) is absent.
Several usable UPFs for one DNN rotate according to the existing policy.

Different DNNs may use different address pools. Several UPFs serving the same
DNN must all route the CP-allocated addresses that may reach them. IE 233 does
not expose their CIDRs, so the feature cannot detect incompatible address plans.
It leaves the allocator and established session ownership unchanged. Existing
sessions on other UPFs were tested during addition, loss and return of a peer;
sessions on a crashed UPF have no continuity guarantee.

## Build, images and Helm

Use the [isolated runbook](dynamic-dnn-runbook.md) and
[generator instructions](../../lib/pfcp/support/README.md). Meson/Ninja build all
PFCP users; rebuild shared libraries and daemons together. Record the checkout,
dirty patch, source hash and pinned vendor commits. The final review records
actual commands and immutable toolchain identity. A source-level reproduction
does not imply byte-identical images: runtime package downloads are not locked.

[The Helm Dockerfile](../../docker/helm/Dockerfile) compiles and tests inside its
builder stage, installs to `/opt/open5gs` and copies that installation into the
runtime stage. It contains all NF daemons; the Gradiant chart chooses a daemon
per workload. Only SMF and UPF require the custom feature image. The validated
remaining NFs use Gradiant 2.8.0. The separate local-test image packager copies
the already-tested build and its ELF dependencies into small SMF/UPF images;
those images do not provide the complete Helm init/probe tool contract.

Pin Helm `image.registry`, `repository`, `tag` and `digest`; preserve the chart's
complete `customOpen5gsConfig`, mounted paths and securityContext. The UPF needs
the chart's TUN/network privileges and return routing. Inspect rendered YAML
and actual Pod imageIDs; tag names or chart `appVersion` alone are insufficient.
Gradiant chart 2.3.4 required no template changes in the recorded deployments.
The final corrected source is published as `bmarujo/open5gs:dnn-384a54f4d9a0`,
digest `sha256:a05e25b5cc4e39defa42b87fed82bcd0ace632ce477a53059f6ebee6fdffdb8b`.
It is deployed to SMF/UPF in both designated clusters, with actual imageIDs and
NAS/data smoke verified. See the [performance/publication report](dynamic-dnn-performance.md)
for original/new comparison, build provenance, physical operational guards and
rollback. Earlier images/results retain their historical identity.

## Diagnosis, validation and rollback

SMF selection logs include requested DNN, `source`, `reason`, association state,
generation and peer. Debug logs include each owned pool and snapshot publication,
invalidation and Heartbeat confirmation. A PFCP association alone proves neither
a usable DNN mapping nor successful data forwarding.

For each new NAS attempt correlate SUPI, PDU Session ID, DNN, UE IP, selected
peer, first N4 Session Establishment Request, SQN/SEIDs and retry count. For data
proof also correlate UL/DL GTP TEIDs and a unique payload at N6/DN. Control
location matches in DNN tests. Preserve failed attempts; distinguish a fresh NAS
retry from a retransmission of the same PFCP transaction. The known baseline
no-match error can cause PFCP retries and NAS rejection; this is documented,
rather than fixed by changing the requested legacy policy.

Disable learning to return to static selection and disable advertisement
independently. These are startup options: apply revised configuration through
the existing process/Helm lifecycle. Removing learning without restoring static
peer mappings leaves legacy fallback. Runtime snapshots are not persisted.

For deployment rollback, use that deployment's recorded Helm revision and values,
restore image digests and both feature settings together, and preserve database
and subscriber data. Rollback/restart can lose sessions. Test cleanup must use
recorded resource identities and UID preconditions; do not delete normal releases,
network definitions or persistent volumes as lab cleanup. Exact lab procedures
remain in the dated [cluster](dynamic-dnn-cluster.md),
[version alignment](dynamic-dnn-version-alignment.md) and
[Containernet](dynamic-dnn-containernet.md) reports.

The feature adds no YAML hot reload, NRF discovery of UPFs, dynamic DNN/service
provisioning, SUPI policy/API, orchestrator, strict selection or session migration.
IPv6-only/dual-stack NAS and GTP-U gateway data, plus existing slice scenarios,
also passed the [local follow-up](dynamic-dnn-ipv6-slices.md) with learning off
and with both association initiators. External IPv6 DN traffic and full NAS using
slice-restricted PFCP advertisements remain untested, as do other vendors,
load/soak, roaming and handover.
