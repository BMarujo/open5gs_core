# Dynamic DNN audit: isolated experiments

Date: 2026-10-06. Baseline: `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`.

This executable documentation does not change the core. Run the Python block
from the repository root with Python 3 and PyYAML 6.0.3. It compiles an extracted,
unchanged copy of the two current selector functions against minimal list and
context adapters in a temporary directory. This is a selector characterization,
not a daemon, PFCP, allocator, or integration test.

The identity experiment tests the **proposed configuration contract**, in fresh
Python processes. It is not evidence that the current UPF accepts `pool_id` (it
does not), or that identity bytes survive an actual UPF restart. The latter is
T24 in the plan and remains required in stage 3.

```python
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import yaml

work = Path(tempfile.mkdtemp(prefix="open5gs-dnn-characterization-"))
source = Path("src/smf/context.c").read_text()
selector = source[source.index("static bool compare_ue_info("):
                  source.index("void smf_sess_select_upf(")]
prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#define ogs_assert assert
#define ogs_strcasecmp strcasecmp
#define OGS_GTP2_RAT_TYPE_EUTRAN 6
#define smf_pfcp_state_associated 1
#define OGS_FSM_CHECK(p, state) (*(p) == (state))
#define ogs_error(...) ((void)0)
typedef struct node {
    struct node *next;
    int sm, num_of_dnn, num_of_e_cell_id, num_of_nr_cell_id, num_of_tac;
    const char *dnn[16];
    unsigned e_cell_id[16], nr_cell_id[16], tac[16];
} ogs_pfcp_node_t;
typedef struct {
    struct { const char *name; } session;
    int gtp_rat_type;
    struct { unsigned cell_id; } e_cgi, nr_cgi;
    struct { unsigned tac; } e_tai;
    struct { struct { unsigned v; } tac; } nr_tai;
} smf_sess_t;
static struct { ogs_pfcp_node_t *pfcp_peer_list; } ctx;
static struct { struct { int no_pfcp_rr_select; } parameter; } conf;
#define ogs_pfcp_self() (&ctx)
#define ogs_global_conf() (&conf)
#define ogs_list_next(node) ((node)->next)
#define ogs_list_first(list) (*(list))
'''
suffix = r'''
int main(void) {
    ogs_pfcp_node_t a = {0}, b = {0}, c = {0};
    smf_sess_t s = {0};
    a.next = &b; b.next = &c; ctx.pfcp_peer_list = &a;
    a.sm = b.sm = c.sm = 1;
    a.dnn[0] = "internet"; b.dnn[0] = c.dnn[0] = "edge1";
    a.num_of_dnn = b.num_of_dnn = c.num_of_dnn = 1;
    s.session.name = "EDGE1";
    assert(selected_upf_node(&c, &s) == &b);
    assert(selected_upf_node(&b, &s) == &c);
    a.num_of_tac = 1; a.tac[0] = s.nr_tai.tac.v = 42;
    assert(selected_upf_node(&c, &s) == &a);
    a.num_of_tac = 0; s.session.name = "unknown";
    assert(selected_upf_node(&c, &s) == &a);
    a.sm = 0;
    assert(selected_upf_node(&c, &s) == &b);
    b.sm = c.sm = 0;
    assert(selected_upf_node(&c, &s) == &a);
    b.sm = 1; conf.parameter.no_pfcp_rr_select = 1;
    assert(selected_upf_node(&c, &s) == &a);
    s.session.name = "edge1";
    assert(selected_upf_node(&c, &s) == &b);
    puts("PASS: 8 assertions on extracted legacy selector");
    return 0;
}
'''
(work / "selector.c").write_text(prefix + selector + suffix)
subprocess.run(["cc", "-std=gnu89", "-Wall", "-Wextra", "-Werror",
                str(work / "selector.c"), "-o", str(work / "selector")], check=True)
subprocess.run([str(work / "selector")], check=True)

# A model of the proposed local configuration rules, not the Open5GS parser.
reader = r'''
import json, re, sys, yaml
rows = yaml.safe_load(open(sys.argv[1]))["upf"]["session"]
seen = set()
result = {}
for row in rows:
    if not row.get("dnn"):
        continue
    identity = row.get("pool_id")
    if not isinstance(identity, str) or not re.fullmatch(r"[A-Za-z0-9._:-]{1,64}", identity):
        raise ValueError("invalid pool_id")
    if identity in seen:
        raise ValueError("duplicate pool_id")
    seen.add(identity)
    result[row["subnet"]] = identity.encode("ascii").hex()
print(json.dumps(result, sort_keys=True))
'''
(work / "identity.py").write_text(reader)
rows = [dict(subnet="10.48.0.0/16", dnn="edge1", pool_id="edge1-v4-a"),
        dict(subnet="2001:db8:48::/48", dnn="edge1", pool_id="edge1-v6-a")]
def run(rows, seed, n4="127.0.0.7"):
    (work / "upf.yaml").write_text(yaml.safe_dump(
        dict(upf=dict(session=rows, pfcp=dict(server=[dict(address=n4)])))))
    return subprocess.run([sys.executable, str(work / "identity.py"),
                           str(work / "upf.yaml")], capture_output=True, text=True,
                          env=dict(os.environ, PYTHONHASHSEED=str(seed)))

first = run(rows, 1)
assert first.returncode == 0
assert run(rows, 29).stdout == first.stdout
assert run(list(reversed(rows)), 43, "127.0.0.8").stdout == first.stdout
changed = [dict(rows[0], pool_id="edge1-v4-replaced"), rows[1]]
assert run(changed, 1).returncode == 0 and run(changed, 1).stdout != first.stdout
for bad in ["", "x" * 65, None]:
    assert run([dict(rows[0], pool_id=bad)], 1).returncode != 0
assert run([rows[0], dict(rows[1], pool_id=rows[0]["pool_id"])], 1).returncode != 0
assert run([dict(rows[0], pool_id="x" * 64)], 1).returncode == 0
print("PASS: identity contract across fresh processes, reordering, N4 change,")
print("      explicit replacement, invalid/duplicate IDs and 64-byte boundary")
print("Artifacts:", work)
```
