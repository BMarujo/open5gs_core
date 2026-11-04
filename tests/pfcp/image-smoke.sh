#!/usr/bin/env bash
# Start the actual locally packaged images, then stop only these containers.
set -euo pipefail
artifacts=$(realpath "${1:?Usage: $0 ARTIFACTS}")
out="$artifacts/${2:-image-smoke}"
mkdir "$out"
chmod 0777 "$out" # dumpcap drops DAC capabilities; lab evidence directory only.
mapfile -t images < "$artifacts/image-provenance/tags.txt"
smf_id= upf_id= capture_id=
cleanup() {
    for pair in "upf:$upf_id" "smf:$smf_id" "capture:$capture_id"; do
        role=${pair%%:*}; id=${pair#*:}
        if [[ -n $id ]]; then
            docker stop -t 10 "$id" >/dev/null || true
            docker logs "$id" > "$out/$role.console" 2>&1 || true
            docker inspect "$id" > "$out/$role-container.json" || true
            docker rm "$id" >/dev/null || true
        fi
    done
}
trap cleanup EXIT INT TERM
python3 - "$out" <<'PY'
import json, pathlib, sys
out = pathlib.Path(sys.argv[1])
common = {'global': {'max': {'ue': 16, 'peer': 16}}}
smf = dict(common, logger={'level':'debug','file':{'path':'/evidence/smf.log'}}, smf={
 'sbi':{'server':[{'address':'127.0.0.4','port':7777}], 'client':{'nrf':[{'uri':'http://127.0.0.10:7777'}]}},
 'pfcp':{'server':[{'address':'127.0.0.4'}], 'learn_upf_dnn':True},
 'gtpc':{'server':[{'address':'127.0.0.4'}]}, 'gtpu':{'server':[{'address':'127.0.0.4'}]},
 'session':[{'subnet':'10.48.0.0/24','gateway':'10.48.0.1','dnn':'edge1'}], 'dns':['8.8.8.8']})
upf = dict(common, logger={'level':'debug','file':{'path':'/evidence/upf.log'}}, upf={
 'pfcp':{'server':[{'address':'127.0.0.8'}], 'client':{'smf':[{'address':'127.0.0.4'}]}, 'advertise_ue_ip_pools':True},
 'gtpu':{'server':[{'address':'127.0.0.8'}]},
 'session':[{'subnet':'10.48.0.0/24','gateway':'10.48.0.1','dnn':'edge1','pool_id':'edge1-image-b','dev':'imageb'}]})
for role, cfg in [('smf',smf),('upf',upf)]: (out/(role+'.yaml')).write_text(json.dumps(cfg))
PY
smf_id=$(docker run -d --network none --label open5gs.dynamic-dnn.lab=true \
    -v "$out:/evidence" "${images[0]}" -c /evidence/smf.yaml)
capture_id=$(docker run -d --network "container:$smf_id" --cap-add NET_ADMIN --label open5gs.dynamic-dnn.lab=true \
    -v "$out:/evidence" sha256:7e59045bafddbee78b43ed533388cde5da77da3a5b80c807ac2f2f561013a551 \
    tshark -i lo -f 'udp port 8805' -a duration:5 -w /evidence/association.pcapng)
sleep 1
upf_id=$(docker run -d --network "container:$smf_id" --cap-add NET_ADMIN --device /dev/net/tun \
    --label open5gs.dynamic-dnn.lab=true -v "$out:/evidence" "${images[1]}" -c /evidence/upf.yaml)
for _ in {1..100}; do
    if [[ -f $out/smf.log ]] && rg -q 'PFCP associated \[127.0.0.8\]' "$out/smf.log"; then break; fi
    sleep .1
done
rg 'initialize...done' "$out/smf.log" "$out/upf.log"
rg 'PFCP associated \[127.0.0.8\]|PFCP pool DNN\[edge1\]' "$out/smf.log"
[[ $(docker inspect -f '{{.State.Running}}' "$smf_id") == true ]]
[[ $(docker inspect -f '{{.State.Running}}' "$upf_id") == true ]]
docker wait "$capture_id" > "$out/capture-exit.txt"
[[ $(cat "$out/capture-exit.txt") == 0 ]]
printf 'PASS: actual SMF/UPF image startup and PFCP learning\n' > "$out/result.txt"
