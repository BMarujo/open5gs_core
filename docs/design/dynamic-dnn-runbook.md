# Dynamic DNN MVP: local reproduction and isolated cluster runbook

Prepared on 2026-10-12; updated on 2026-11-04 for the authorized restart
and asymmetric Heartbeat recovery corrections. See the [recovery report](dynamic-dnn-pfcp-recovery.md),
the [restart report](dynamic-dnn-restart-fix.md) and the
historical [stage 5 report](dynamic-dnn-integration.md) for results and scope.
Local commands below were exercised in the reported environment. Cluster
commands are preparation only: Kubernetes admission, networking and UE sessions
have **NOT RUN**. Executing stage 6 requires a new explicit instruction and an
identified test context and dedicated namespace. Stage 4B is excluded.

## Inputs and local reproduction

Use Linux/amd64 with Docker, `/dev/net/tun`, NET_ADMIN permission, Python 3.12+
on the host and enough space for the source, full build and evidence. Never use
host networking or an existing MongoDB. The runner creates its own network
namespace and tmpfs MongoDB, and removes only its recorded container IDs.

The current evidence bundle is `/home/marujo/open5gs_pfcp_recovery_results_20261104`.
The prior `/home/marujo/open5gs_dnn_restart_results_20261104` and original
`/home/marujo/open5gs_stage5_results_20261011` remain unchanged;
their older images/source do not contain the latest asymmetric recovery correction.
`SHA256SUMS` covers its files. `final-provenance` records HEAD, dirty patch,
untracked inputs and all source hashes; `source-snapshot.tar.gz` includes the
final source and pinned vendored subprojects without a Meson build cache.
The runtime, compiler/tools and minimal nftables helper images are exported as separate tar
archives. The archives are local artifacts, not registry publications.

```sh
bundle=/home/marujo/open5gs_pfcp_recovery_results_20261104
cd "$bundle"
sha256sum -c SHA256SUMS
docker load -i open5gs-dnn-toolchain-image.tar
docker load -i open5gs-dynamic-dnn-images.tar
docker load -i open5gs-pfcp-nettools-image.tar
docker image inspect open5gs-smf:2c80f8f2ba9d-dirty-0d6a194893d6
docker image inspect open5gs-upf:2c80f8f2ba9d-dirty-0d6a194893d6
# Required once if this exact MongoDB image is not already local:
docker pull mongo@sha256:8c3ce64d1a433bf57ea79035b48a38ea3e532997a1328fe3266e2e2e8bfb41b6
```

For a new run, choose a **fresh** external output directory. Do not overwrite
the original evidence. Extract the source snapshot so `source/` is immediately
inside that directory. Alternatively, from the working checkout use
`docker/dynamic-dnn/prepare-build.py REPO VENDOR_SOURCE OUTPUT`; `VENDOR_SOURCE`
must contain the pinned Git subprojects listed in the report. That helper
preserves current tracked/untracked inputs and stages outside the checkout.
The source snapshot avoids needing those Git repositories again.

```sh
run=/tmp/open5gs-dnn-new-run
mkdir "$run"
tar -xzf "$bundle/source-snapshot.tar.gz" -C "$run"
tools=sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551
docker run --rm --network none -v "$run:/artifacts" \
  -w /artifacts/source "$tools" sh -c \
  'meson setup /artifacts/build --prefix=/artifacts/install -Dbuildtype=debug -Dwerror=false --wrap-mode=nodownload && ninja -C /artifacts/build -j6 && meson test -C /artifacts/build core crypt unit smf-selection --print-errorlogs'
cd "$run/source"
bash tests/pfcp/run-dnn-integration.sh "$run" dynamic-01 dynamic
bash tests/pfcp/run-dnn-integration.sh "$run" static-01 static
bash tests/pfcp/run-dnn-integration.sh "$run" incompatible-01 incompatible
bash tests/pfcp/run-dnn-integration.sh "$run" regression-01 regression
bash tests/pfcp/run-dnn-integration.sh "$run" epc-01 epc
# Both now verify repeated C/B/C/B sessions after actual loss/return.
bash tests/pfcp/run-dnn-integration.sh "$run" recovery-01 recovery
bash tests/pfcp/run-dnn-integration.sh "$run" recovery-cp-01 recovery-cp
# No NF restart: selectively lose Heartbeat Responses, then restore connectivity.
bash tests/pfcp/run-pfcp-recovery.sh "$run" asymmetric-smf-01 smf up recover
bash tests/pfcp/run-pfcp-recovery.sh "$run" asymmetric-upf-01 upf cp recover
bash tests/pfcp/run-pfcp-recovery.sh "$run" asymmetric-both-01 both up recover
bash tests/pfcp/run-pfcp-recovery.sh "$run" brief-01 smf up brief
bash tests/pfcp/run-pfcp-recovery.sh "$run" feature-off-01 smf up recover-off
# Actual NAS/N4/GTP-U, with new first-request evidence after network recovery.
bash tests/pfcp/run-pfcp-recovery.sh "$run" nas-smf-01 smf up recover nas
bash tests/pfcp/run-pfcp-recovery.sh "$run" nas-upf-01 upf cp recover nas
```

The asymmetric runner needs the exported minimal nft 1.1.0 helper and a host
kernel with nftables support; legacy iptables is not required. All rules are
inside the newly created container network namespace. `PFCP_RETRY_MS=125`
accelerates retries for simultaneous-recovery stress; default is 1000 ms.
The pair test also injects 128 independent unmatched Setup Responses into each
NF to exercise discard-event ownership. `RECOVERY_BUILD=/artifacts/sanitize-build`
selects instrumented binaries for pair tests. Full-NF leak checks require the
narrow `LSAN_OPTIONS=suppressions=/artifacts/talloc-startup.lsan:print_suppressions=1`
for the independently reproduced baseline startup tracker; preserve the raw
unfiltered FAIL as explained in the report. The suppression file is in the bundle.

Record each command's UTC time and exit status separately, including failures.
The successful dynamic/static runs also characterize the baseline no-match
error: a fallback request is sent, invalid UPF error responses are retransmitted,
then the UE receives a NAS reject. They do not prove strict compatibility.
The incompatible-pool run expects an accepted session whose ordinary return
path fails; its PASS is a successful negative characterization. The historical
ordinary-return failure is corrected in this source snapshot; preserve and
investigate any nonzero recovery result rather than counting it as success.

Repackage a tested source/build using `build-images.sh REPO ARTIFACTS`; it
requires a Git checkout for provenance and a fresh `runtime-context` directory.
It verifies staged core inputs against the checkout, packages the tested ELF
closure, checks loaders/help and saves both images without pushing. A source
snapshot alone has no Git metadata; use the supplied image archives or restore
the recorded HEAD plus patch/untracked inputs into a separate checkout first.

```sh
bash docker/dynamic-dnn/build-images.sh /home/marujo/open5gs_core "$run"
bash tests/pfcp/image-smoke.sh "$run" image-smoke-01
```

The images include the same 5GC NF/library payload used in the local tests.
Full NAS tests used the build executables; only startup and PFCP learning were
tested inside the runtime image containers. Diameter extension deployment,
external N6 and other CPU architectures remain outside that evidence.

## Cluster prerequisites and isolation

Before any creation in a subsequently authorized stage 6, record the explicit
context, new namespace, unique lab ID, cluster/node versions and available
resources. Never infer the target from the current kubeconfig context. Inspect
the following read-only and resolve concrete failures before applying fixtures:

- Namespace name must be unused and start with `open5gs-dnn-lab-`. Lab ID is
  1–58 lowercase letters/digits/hyphens, beginning with a letter or digit; it
  forms explicit stable pool IDs, never generated from Pod IPs or list order.
- CNI must enforce the supplied NetworkPolicy. It permits same-namespace
  traffic and DNS port 53 only. Verify actual isolation from existing NFs,
  including any Multus/secondary interfaces that bypass that policy.
- Reserve non-colliding lab UE CIDRs; examples are A/internet `10.45.0.0/24`,
  B and C/edge1 `10.48.0.0/24`, SMF-only nomatch `10.50.0.0/24`. Adapt the
  renderer's explicit example pools together if they conflict with the cluster.
- Provide **isolated**, compatible NRF, AMF, AUSF, UDM, UDR, PCF, NSSF, BSF,
  MongoDB, subscriber fixtures and a gNB/UE emulator in this namespace. Record
  their image IDs/configuration. The supplied renderer creates SMF/UPFs only.
  NRF URI must resolve to a service in this namespace. Do not point the test
  SMF or AMF at the existing installation's discovery or subscriber database.
- The local C UE driver uses loopback gNB addresses in one Docker namespace;
  it is not a ready cross-Pod gNB. Use an isolated emulator configured with
  reachable AMF NGAP/SCTP and gNB N3 addresses. Verify SCTP, UDP 8805 and UDP
  2152 reachability, MTU and return paths. No complete cluster UE stack is
  claimed by these fixtures.
- TUN, NET_ADMIN/NET_RAW and capture permissions must be allowed. UPF mounts
  `/dev/net/tun` as a character device and requests NET_ADMIN. A hostPath mount
  may still be denied by the runtime's device cgroup; use a supported device
  plugin or a separately reviewed lab-only security adjustment if needed.
  Admission/Pod Security can also reject these permissions. The scripts do not
  alter node security, install a plugin or enable privileged Pods automatically.
- Import the runtime and tools archives on every scheduled test node. The
  fixtures use `imagePullPolicy: Never`; no registry credentials or publication
  are required. In K3s, an operator can use `sudo k3s ctr -n k8s.io images import
  ARCHIVE` on each test node. Record actual CRI image identities and match the
  payload/labels from the bundle; Docker's local index and CRI config digest
  can be represented differently. A tag alone is insufficient evidence.

Only public synthetic subscriber test vectors may be used in this package.
Any actual lab credentials belong in a separate controlled fixture, not this
repository or report. All ancillary NF, subscription and emulator provisioning
is a prerequisite; no existing deployments are reused or changed here.

## Render and create new lab resources in stage 6

The following is a **template for future execution**, not a record of changes.
Set `context` to the user-designated context; choose fresh values for the rest.
`kubectl create` intentionally fails for an existing name. Inspect the rendered
JSON (valid YAML) before creation. No controller or deployment wait is added
to the core: readiness checks here are operator test sequencing.

```sh
context=USER_DESIGNATED_TEST_CONTEXT
namespace=open5gs-dnn-lab-UNIQUE_LOWERCASE_SUFFIX
lab_id=UNIQUE_LOWERCASE_ID
fixtures=/tmp/open5gs-dnn-cluster-new
mkdir "$fixtures"
smf_image=open5gs-smf:2c80f8f2ba9d-dirty-0d6a194893d6
upf_image=open5gs-upf:2c80f8f2ba9d-dirty-0d6a194893d6
tools_image=open5gs-pfcp-codec-build:20261007
python3 tests/pfcp/cluster/render.py --namespace "$namespace" --lab-id "$lab_id" \
  --role namespace --output "$fixtures/namespace.json"
kubectl --context "$context" create -f "$fixtures/namespace.json"
namespace_uid=$(kubectl --context "$context" get namespace "$namespace" -o 'jsonpath={.metadata.uid}')
printf '%s\n' "$context" "$namespace" "$lab_id" "$namespace_uid" > "$fixtures/ownership.txt"
# Provision/validate the isolated ancillary NF and UE prerequisites here.
python3 tests/pfcp/cluster/render.py --namespace "$namespace" --lab-id "$lab_id" \
  --role smf --smf-image "$smf_image" --upf-image "$upf_image" \
  --tools-image "$tools_image" --nrf-uri "http://nrf.$namespace.svc.cluster.local:7777" \
  --output "$fixtures/smf.json"
kubectl --context "$context" create -f "$fixtures/smf.json"
kubectl --context "$context" -n "$namespace" get pod dnn-smf -o json > "$fixtures/smf-start.json"
smf_n4=$(kubectl --context "$context" -n "$namespace" get pod dnn-smf -o 'jsonpath={.status.podIP}')
python3 tests/pfcp/cluster/render.py --namespace "$namespace" --lab-id "$lab_id" \
  --role upf-a --smf-image "$smf_image" --upf-image "$upf_image" \
  --tools-image "$tools_image" --smf-n4 "$smf_n4" --output "$fixtures/upf-a.json"
kubectl --context "$context" create -f "$fixtures/upf-a.json"
```

If the SMF has no Pod IP or the expected initialization has not completed,
stop the attempt and collect diagnostics; do not substitute a production
address. Verify the effective SMF configuration contains `learn_upf_dnn: true`
and **no** `pfcp.client.upf` list. Record its UID/restartCount/PID when available
before and after B/C addition. The tools sidecar exposes `/config/smf.yaml`;
the runtime image has no shell or `kubectl cp` tar helper.

After UPF A creates `tunlab`, provision its gateway in its Pod namespace:

```sh
kubectl --context "$context" -n "$namespace" exec -i dnn-upf-a -c tools -- \
  python3 - 10.45.0.1/24 < tests/pfcp/cluster/net.py
```

The Linux NF does not provision TUN addresses/routes itself. This script
configures only `tunlab` in the test Pod; it does not set host routes/NAT or
Internet access. B and C need `10.48.0.1/24` on their own TUNs. Data to each
UPF's gateway is an echo test; external N6 additionally needs a verified route
back to the actual owning UPF for every UE IP. Two same-DNN UPFs do not by
themselves provide that external return routing.

Begin bounded captures **before the first session attempt**, e.g. in a host
terminal per Pod (repeat for A and each new B/C before its UE attempt):

```sh
kubectl --context "$context" -n "$namespace" exec dnn-smf -c tools -- \
  tshark -i any -f 'udp port 8805 or udp port 2152' -a duration:180 \
  -w /evidence/n4-n3.pcapng
```

Use the SMF capture as the canonical N4 timeline and each UPF capture for N3
evidence. Keep captures separate or deduplicate deliberately; merging both
endpoints without deduplication can fabricate retransmissions. Record clocks,
capture drops and TShark version. Captures must finish/flush before collection.
Start capture promptly after the tools sidecar is ready. To capture initial
Setup itself, start an independent namespace sniffer before creating the NFs,
or explicitly record that the initial association packet was missed; the
first session still must be captured. No missing packet is a protocol PASS.

Keep a session on A active, then render/create B and later C by repeating the
UPF render command with `--role upf-b` / `upf-c`. Use the same actual SMF N4
address, the stable lab ID and the corresponding output filename. Configure
their TUNs and verify association logs before issuing a new session attempt.
Do not restart the SMF to discover them.

## Attempts, evidence and known failures

Use one explicit attempt ID for each establishment or rejection. Store a CSV
with UTC start/end, SUPI, PSI, DNN, S-NSSAI, UE IP, selected metadata source and
reason, first PFCP request timestamp/frame/source/destination/SQN, CP/UP SEID,
UL/DL TEID, request/retransmission count, PFCP Cause and NAS/data result.
Correlate accepted N4, NAS and GTP-U before counting success. No final ping
after a retry proves that the first UPF was correct.

Exercise internet on A, edge1 on B, add C, and consecutive edge1 attempts that
demonstrate the existing B/C rotation. Check the original A session's owner/IP
and echo after each addition. Do not configure peer TAC/cell matches in these
DNN tests. Verify absence of duplicate UE IPs. Test the provisioned nomatch
DNN separately: record the legacy fallback destination and baseline error
response/timeout behavior, not an expected strict refusal to select.

Then lose and return a peer with a live alternative. Verify replacement
publication and current-epoch Heartbeat confirmation, then multiple new sessions
and at least the next periodic Heartbeat. A healthy returned peer must rejoin
DNN selection automatically. The earlier late-Heartbeat deletion bug is fixed;
if it recurs, retain the evidence as FAIL. An explicit manual later Setup is a
separate case and does not make an unsuccessful automatic-return case pass.
Do not claim old sessions on the failed UPF survive. A deliberately incompatible
same-DNN CIDR is a separate negative routing case, not a different DNN/pool.
T09/T10 remain **NOT RUN (optional)** because strict stage 4B is absent.

Collect before deleting Pods/namespace; `emptyDir` evidence disappears with
them. For a failed/stopped Pod collect immediately, including `logs --previous`
where applicable and events/exit statuses. The helper collects existing SMF/UPF
Pods and available captures without changing configuration:

```sh
bash tests/pfcp/cluster/collect.sh "$context" "$namespace" "$lab_id" \
  /tmp/open5gs-dnn-cluster-evidence-new
```

Also export the isolated ancillary NF/emulator logs, session CSV, capture
commands/versions, image IDs, node/CNI/device inspection and command exit
statuses. The collector does not create missing session evidence. Do not mark
cluster tests PASS from manifest rendering or NF readiness alone.

## Rollback and stopping point

Collect evidence first. Delete only the **new** owned lab namespace using its
recorded label and immutable UID. The helper checks both, then includes a UID
precondition in the API delete to prevent a name-reuse race:

```sh
bash tests/pfcp/cluster/rollback.sh "$context" "$namespace" "$lab_id" "$namespace_uid"
```

No existing deployment is modified, so no production rollout/rollback is part
of this procedure. Imported image removal, if desired, must be limited to
unreferenced exact lab image identities; never prune a node's image store.
If namespace deletion is blocked, inspect its events/finalizers and report;
do not strip arbitrary finalizers or remove node interfaces/firewall rules.
This deletion script passed local syntax review; the real Kubernetes delete
API and admission have **NOT RUN** in stage 5.

`python3 tests/pfcp/cluster/validate.py FRESH_OUTPUT` exercises rendering and
simulates label/UID ownership and deletion-precondition checks with a fake
kubectl. It does not contact a cluster or validate its delete API/admission.

Stop after the requested stage and report PASS, FAIL and NOT RUN separately.
The introduced peer-return regression now has passing local correction
evidence; the independent baseline session error remains excluded. No strict policy,
API/SUPI placement, deployment wait, orchestration or session migration is
introduced by this runbook.
