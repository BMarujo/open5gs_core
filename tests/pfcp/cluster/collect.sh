#!/usr/bin/env bash
# Read-only collection; capture itself is started separately in the runbook.
set -euo pipefail
context=${1:?Usage: $0 CONTEXT NAMESPACE LAB_ID OUTPUT}
namespace=${2:?namespace required}
lab_id=${3:?lab ID required}
out=${4:?output required}
[[ $namespace =~ ^open5gs-dnn-lab-[a-z0-9][a-z0-9-]*$ ]] || exit 2
[[ $(kubectl --context "$context" get namespace "$namespace" -o 'jsonpath={.metadata.labels.open5gs\.lab/id}') == "$lab_id" ]]
mkdir "$out"
kubectl --context "$context" get namespace "$namespace" -o json > "$out/namespace.json"
kubectl --context "$context" -n "$namespace" get pods -o json > "$out/pods-imageIDs.json"
kubectl --context "$context" -n "$namespace" get configmaps -l "open5gs.lab/id=$lab_id" -o json > "$out/configmaps.json"
kubectl --context "$context" -n "$namespace" get events -o json > "$out/events.json"
for role in smf upf-a upf-b upf-c; do
    pod="dnn-$role"
    if ! kubectl --context "$context" -n "$namespace" get pod "$pod" >/dev/null 2>&1; then continue; fi
    nf=upf; [[ $role == smf ]] && nf=smf
    kubectl --context "$context" -n "$namespace" logs "$pod" -c "$nf" > "$out/$role.console"
    kubectl --context "$context" -n "$namespace" exec "$pod" -c tools -- cat "/config/$nf.yaml" > "$out/$role-effective.yaml"
    kubectl --context "$context" -n "$namespace" exec "$pod" -c tools -- cat /evidence/nf.log > "$out/$role.log"
    if kubectl --context "$context" -n "$namespace" exec "$pod" -c tools -- test -f /evidence/n4-n3.pcapng; then
        kubectl --context "$context" -n "$namespace" exec "$pod" -c tools -- cat /evidence/n4-n3.pcapng > "$out/$role.pcapng"
    fi
done
sha256sum "$out"/* > "$out/SHA256SUMS"
