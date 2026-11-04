#!/usr/bin/env bash
# Delete only the newly created, explicitly identified lab namespace.
set -euo pipefail
context=${1:?Usage: $0 CONTEXT NAMESPACE LAB_ID EXPECTED_NAMESPACE_UID}
namespace=${2:?namespace required}
lab_id=${3:?lab ID required}
expected_uid=${4:?namespace UID recorded at creation required}
[[ $namespace =~ ^open5gs-dnn-lab-[a-z0-9][a-z0-9-]*$ && -n $expected_uid ]] || exit 2
actual_uid=$(kubectl --context "$context" get namespace "$namespace" -o 'jsonpath={.metadata.uid}')
actual_lab=$(kubectl --context "$context" get namespace "$namespace" -o 'jsonpath={.metadata.labels.open5gs\.lab/id}')
[[ $actual_uid == "$expected_uid" && $actual_lab == "$lab_id" ]] || { echo 'Namespace identity mismatch; refusing cleanup' >&2; exit 2; }
# API UID precondition prevents a name-reuse race between inspection/deletion.
kubectl --context "$context" delete --raw "/api/v1/namespaces/$namespace" -f - <<EOF
{"apiVersion":"v1","kind":"DeleteOptions","preconditions":{"uid":"$expected_uid"}}
EOF
