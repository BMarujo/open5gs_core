/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Test executable only: actual SMF initialization, sockets, event loop, session
 * allocator and selector. Commands enter through stdin on that same poll thread.
 * No NAS/SBI policy or N4 session establishment is simulated by this driver. */
#include "smf/context.h"
#include <unistd.h>

int __real_smf_pfcp_open(void);
int __wrap_smf_pfcp_open(void);

static ogs_poll_t *control_poll;
static ogs_pool_id_t slots[16];
static ogs_pool_id_t ue_id;
static char test_supi[] = "imsi-001010000000001";

static void report(unsigned id)
{
    char v4[OGS_ADDRSTRLEN] = "", v6[OGS_ADDRSTRLEN] = "";
    smf_sess_t *s = smf_sess_find_by_id(slots[id]);
    if (!s) { printf("RESULT {\"slot\":%u,\"exists\":false}\n", id); return; }
    if (s->ipv4) OGS_INET_NTOP(&s->ipv4->addr, v4);
    if (s->ipv6) OGS_INET6_NTOP(&s->ipv6->addr, v6);
    printf("RESULT {\"slot\":%u,\"exists\":true,\"pid\":%d,"
            "\"selected\":\"%s\",\"static_count\":%u,\"generation\":%llu,"
            "\"associated\":%s,\"type\":%u,\"ipv4\":\"%s\",\"ipv6\":\"%s\"}\n",
            id, (int)getpid(), s->pfcp_node ?
                ogs_sockaddr_to_string_static(s->pfcp_node->addr_list) : "",
            s->pfcp_node ? s->pfcp_node->num_of_dnn : 0,
            s->pfcp_node ? (unsigned long long)s->pfcp_node->association_generation : 0,
            s->pfcp_node && OGS_FSM_CHECK(&s->pfcp_node->sm, smf_pfcp_state_associated) ? "true" : "false",
            s->session.session_type, v4, v6);
}

static void command(const char *line)
{
    char verb[16], dnn[OGS_MAX_DNN_LEN+1];
    unsigned id, type, tac = 999, cell = 999;
    int fields = sscanf(line, "%15s %u %100s %u %u %u", verb, &id, dnn, &type, &tac, &cell);
    smf_sess_t *s;
    smf_ue_t *ue;
    ogs_assert(fields >= 2 && id > 0 && id < OGS_ARRAY_SIZE(slots));
    s = smf_sess_find_by_id(slots[id]);
    if (!strcmp(verb, "select")) {
        ogs_assert(fields >= 4);
        if (s) smf_sess_remove(s);
        ue = smf_ue_find_by_id(ue_id);
        if (!ue) {
            ue = smf_ue_add_by_supi(test_supi);
            ogs_assert(ue); ue_id = ue->id;
        }
        s = smf_sess_add_by_psi(ue, id); ogs_assert(s);
        slots[id] = s->id;
        s->session.name = ogs_strdup(dnn);
        s->session.session_type = type;
        s->nr_tai.tac.v = tac; s->nr_cgi.cell_id = cell;
        /* Match the ordinary 5GC boundary: allocation precedes selection. */
        ogs_assert(smf_sess_set_ue_ip(s) == OGS_PFCP_CAUSE_REQUEST_ACCEPTED);
        smf_sess_select_upf(s);
    } else if (!strcmp(verb, "drop")) {
        if (s) smf_sess_remove(s);
        slots[id] = OGS_INVALID_POOL_ID;
    } else ogs_assert(!strcmp(verb, "inspect"));
    report(id); fflush(stdout);
}

static void control_cb(short when, ogs_socket_t fd, void *data)
{
    static char pending[1024];
    static size_t used;
    ssize_t n = read(fd, pending+used, sizeof(pending)-used-1);
    char *end;
    if (n <= 0) { ogs_pollset_remove(control_poll); control_poll = NULL; return; }
    used += n; pending[used] = '\0';
    while ((end = strchr(pending, '\n'))) {
        size_t consumed = end-pending+1;
        *end = '\0'; command(pending);
        used -= consumed; memmove(pending, end+1, used+1);
    }
    ogs_assert(used < sizeof(pending)-1);
}

int __wrap_smf_pfcp_open(void)
{
    int rv = __real_smf_pfcp_open();
    if (rv != OGS_OK) return rv;
    control_poll = ogs_pollset_add(ogs_app()->pollset, OGS_POLLIN,
            STDIN_FILENO, control_cb, NULL);
    ogs_assert(control_poll);
    return OGS_OK;
}

int app_initialize(const char *const argv[])
{
    int rv = smf_initialize();
    if (rv == OGS_OK) ogs_info("SMF selector driver initialize...done");
    return rv;
}

void app_terminate(void)
{
    smf_terminate();
    if (control_poll) { ogs_pollset_remove(control_poll); control_poll = NULL; }
}
