# Local IPv6 and existing-slice regression validation

Date: 2026-11-08, after the final project review. The user requested additional
IPv6/slice validation without touching Kubernetes. All final checks passed;
no core/NF behavior correction was needed. Only test fixtures and documentation
were changed. There were no Kubernetes/Helm calls, deployment changes, image
publications, Git commits or pushes in this follow-up.

## Identity, isolation and coverage

HEAD remains `78f91ae6e5674503c766991e0d8cd10c515869b6`, branch `main`, declared
Open5GS 2.8.0. Existing uncommitted work was preserved. Core-input SHA-256 remains
`d8e528dfc3abe4c66856b8f75e364d8bac1758eebbff2b61ab9c52781bf5242c`.
This is the final-review core with the extendable-IE correction, not a new
behavior revision. Published images still predate that correction.

Fresh evidence is `/home/marujo/open5gs_ipv6_slice_results_20261108T171514Z`.
The source/build from the sealed final-review bundle was copied with copy-on-write
into that fresh directory. The test driver was rebuilt there. Original bundles
and build artifacts were not modified. The immutable compiler/tools image is
`sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551`;
its TShark version is 4.2.2. MongoDB 7.0.26 uses the existing pinned image digest
from the runbook and a disposable tmpfs database.

Every run uses a new Docker `--network none` namespace. Only that namespace
receives NET_ADMIN, TUN addresses/routes and its loopback SCTP/N4/N3 traffic.
Mongo shares that namespace; it does not use the normal core's database.
Wrappers remove only their recorded container IDs. No host firewall/routes,
running emulation, Kubernetes API or existing subscribers were accessed.

Three existing-test modes were exercised:

| Mode | Advertisement / learning | Association and mappings |
|---|---|---|
| `off` | Both disabled | Original static/generic fixture and legacy selection |
| `cp` | Both enabled | SMF initiates; peer addresses only, no static DNN/location mapping |
| `up` | Both enabled | UPF initiates; no SMF `client.upf` mapping |

The active-mode fixture assigns explicit `internet` DNN and persistent IDs to
the existing UPF IPv4 and IPv6 subnets. That lets learned matching be verified;
it does not change the C tests, subscription implementation or slice policy.
No advertised S-NSSAI restrictions are fabricated. Existing slice tests exercise
requested/allowed NSSAI with multiple SDs under SST 1; they do not establish
dynamic selection by slice-restricted PFCP advertisements.

## Existing regressions: PASS

The actual existing C tests, rebuilt in the prior complete final build, ran
with their own real child NFs, NAS/NGAP/SBI/PFCP and GTP-U. No production
selector/session functions were copied or mocked.

| Test | Off | Learned CP initiator | Learned UP initiator | Independent evidence |
|---|---|---|---|---|
| `registration/simple-test` | PASS | PASS | PASS | Dual-stack NAS session; real IPv6 echo request/reply plus Router Solicitation/Advertisement in GTP-U |
| `slice/same-dnn-test` (two C cases) | PASS | PASS | PASS | Requested/allowed NSSAI and same-DNN slice behavior; NGAP exposes SD `000080` and `009000` |
| `slice/different-dnn-test` | PASS | PASS | PASS | Existing subscription/DNN/slice scenario and NAS outcome |
| `attach/simple-test` | PASS | NOT RUN | NOT RUN | Feature-off EPC/SGW-C/SGW-U regression, including IPv6 GTP echo and RS/RA |

These are **10 successful executable invocations / 13 ABTS C cases**. In each
active-mode 5GC invocation, SMF logs contain `source[learned] reason[dnn]`.
Successful connectivity is therefore accompanied by evidence of learned DNN
use, rather than inferred solely from having one available UPF.
Captures independently contain five IPv6 echo replies across this matrix:
one in each 5GC registration run and two in feature-off EPC. Counts describe
the named fixture, not a throughput/load measurement.

Artifacts: `ready-existing-{off,cp,up}/results.json`, each case's complete YAML,
logs and N2/N4/N3 PCAP; `ready-existing-matrix-results.json` records wrapper
commands/exits. The independent decoder also checks the captured NGAP SDs.

## New IPv6-only and dual-stack dynamic sessions: PASS

A test-only extension of `dnn-session-driver` adds `establish6`, `establish46`
and Router Solicitation commands. Existing `establish` stays IPv4. IPv6 replies
are checked for ICMPv6 type, expected source/destination address and GTP TEID;
the existing RA helper checks TEID/QFI and learns the advertised prefix.
NAS address-family flags are asserted, so an IPv6 request silently downgraded
to IPv4 cannot pass. No NF implementation was modified.

The isolated topology has A/`internet` (IPv4) at `127.0.0.7` and B/`edge1`
(IPv4 + IPv6) at `127.0.0.8`. A is added first. SMF has operator-provisioned
address pools but no static peer DNN or location mappings. The first two
sessions must choose B by learned DNN; choosing A by fallback fails the test.
Separate IPv4/IPv6 groups advertised by B provide dual-stack family coverage.

| Initiator | PDU Session ID / requested type | NAS result / allocated address | First N4 frame / UPF | PFCP retransmissions |
|---|---|---|---|---|
| SMF | 1 / IPv6 only | Accepted; `2001:db8:cafe:1::2`, no IPv4 | 35 / B | 0 |
| SMF | 2 / IPv4v6 | Accepted; `10.48.0.2`, `2001:db8:cafe:2::3` | 53 / B | 0 |
| SMF | 3 / IPv4 | Accepted; `10.45.0.2` | 77 / A | 0 |
| UPF | 1 / IPv6 only | Accepted; `2001:db8:cafe:1::2`, no IPv4 | 33 / B | 0 |
| UPF | 2 / IPv4v6 | Accepted; `10.48.0.2`, `2001:db8:cafe:2::3` | 51 / B | 0 |
| UPF | 3 / IPv4 | Accepted; `10.45.0.2` | 75 / A | 0 |

Both runs use synthetic IMSI `9997000007487` in separate disposable databases;
addresses reused across runs are not duplicate allocations within a running SMF.
All six selections log `source[learned] reason[dnn]`. The same SMF process
serves all three sessions in each run and all owned NFs exit successfully.
The existing `unit` and `smf-selection` suites were also rerun: PASS, 2/2.
Their family/restriction/legacy policy boundary coverage remains intact.

Independent TShark/raw TLV analysis verifies three configured advertisements:
`internet`/V4, `edge1`/V4 and `edge1`/V6. They appear in Setup Response (6) for
the CP initiator, and Setup Request (5) for the UP initiator. Each session's
first N4 request is correlated with its attempt window; exactly one request
is present per attempt. For each IPv6 UE address, three request/reply pairs
are independently matched to its UL/DL TEIDs. The two runs contain **12 IPv6
echo request/reply pairs**, plus IPv4 checks for dual-stack/IPv4 sessions.

Data proof is the IPv6 gateway on the local UPF TUN/N6 side, using real GTP-U
encapsulation and kernel return routing. It is not an arbitrary external IPv6
DN/Internet test. Artifacts: `drained-dynamic-{cp,up}/sessions.json`,
`results.json`, effective YAML, logs and `n2-n4-n3.pcapng`;
`independent-analysis.json` verifies both campaigns independently.

## Failures, implementation impact and limits

No final NF test or independent verification failed. Initial instrumentation
attempts are preserved, rather than counted as passing evidence:

- ABTS defaults its child NFs to ERROR regardless of YAML logger level. Initial
  sessions/C tests passed, but the learned-selection log assertion failed. The
  runner now passes `-e info` explicitly and all three modes were repeated.
- Debug SBI logs contain binary NAS multipart data. Strict UTF-8 reading failed
  in the initial new runner; diagnostic text now uses replacement decoding while
  preserving the original log bytes.
- Very short NAS/data bursts finished before the capture buffer delivered the
  last packets. Waiting for capture startup alone was insufficient; the final
  runner also drains capture for two seconds before stopping it, with NFs alive.
  Final first-request and TEID/payload checks use the complete repeated runs.

Only test code/documentation changed: three runners/analyzer files, the isolated
wrapper and test-only session driver. Existing IPv6 allocation/forwarding and
slice procedures required no correction. Current evidence supports preserving
the tested existing behaviors, not an absolute guarantee for every IPv6/slice
configuration or all network failures.

Still NOT RUN: external IPv6 DN/N6 routing/NAT variants; IPv6 N4/N3 transport;
IPv6 same-DNN multi-UPF loss/rotation; full NAS using slice-restricted PFCP pool
groups; broader SST/roaming/handover/load combinations; these scenarios on K8s.
The unit/actual-selector boundary tests retain slice restrictions and exclude
restricted groups from the learned DNN arm. Legacy location/fallback can still
choose such a peer. Strict slice-aware UPF selection/4B remains unimplemented.

## Reproduction

Use a fresh source/build directory with the pinned toolchain and vendor sources
as described in the [runbook](dynamic-dnn-runbook.md). Copying the sealed final
build to a fresh directory is valid here because its configured mount paths
remain `/artifacts/source` and `/artifacts/build`. Build/test artifacts stay
outside the checkout; don't overwrite the recorded results.

```sh
bash tests/pfcp/run-ipv6-slice-regressions.sh "$run" ready-existing-off off
bash tests/pfcp/run-ipv6-slice-regressions.sh "$run" ready-existing-cp cp
bash tests/pfcp/run-ipv6-slice-regressions.sh "$run" ready-existing-up up
bash tests/pfcp/run-ipv6-slice-regressions.sh "$run" drained-dynamic-cp cp dynamic
bash tests/pfcp/run-ipv6-slice-regressions.sh "$run" drained-dynamic-up up dynamic
```

Inside the tools container, with that directory mounted at `/artifacts`:

```sh
python3 /artifacts/source/tests/pfcp/analyze-ipv6-slices.py \
  /artifacts /artifacts/independent-analysis.json
```

The analyzer's expected names deliberately select the final ready/drained runs;
failed earlier attempts remain separate. Exact commands, source identity,
decoder version and SHA256 manifest are retained in the fresh evidence bundle.
The previous final-review bundle's complete SHA256 manifest was checked again
and passed, confirming that source/build copying did not modify it.
