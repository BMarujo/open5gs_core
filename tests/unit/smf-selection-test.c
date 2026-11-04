/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "smf/context.h"
#include "core/abts.h"

static ogs_pfcp_node_t *peer(unsigned n, const char *dnn)
{
    ogs_pfcp_node_t *node = ogs_pfcp_node_new(NULL);
    char ip[32];
    ogs_assert(node);
    ogs_snprintf(ip, sizeof(ip), "127.0.0.%u", n);
    ogs_assert(ogs_getaddrinfo(&node->addr_list, AF_INET, ip, 8805, 0) == OGS_OK);
    ogs_list_add(&ogs_pfcp_self()->pfcp_peer_list, node);
    OGS_FSM_TRAN(&node->sm, smf_pfcp_state_associated);
    if (dnn) { node->dnn[0] = dnn; node->num_of_dnn = 1; }
    return node;
}

static ogs_pfcp_pool_snapshot_t *snapshot(ogs_pfcp_node_t *node,
        ogs_pfcp_pool_decode_status_t status, const char *dnn, unsigned family)
{
    ogs_pfcp_pool_snapshot_t *s = ogs_calloc(1, sizeof(*s));
    ogs_assert(s);
    s->result.status = status; s->source = 5;
    if (status != OGS_PFCP_POOL_ABSENT) s->result.groups_seen = 1;
    if (dnn) {
        s->num_groups = 1; s->groups[0].usable = true;
        ogs_cpystrn(s->groups[0].dnn, dnn, sizeof(s->groups[0].dnn));
        s->groups[0].family_present = family != 0;
        s->groups[0].family = family;
    }
    ogs_pfcp_pool_snapshot_publish(node, s);
    return s;
}

static smf_sess_t *session(const char *dnn, unsigned type)
{
    smf_sess_t *s = ogs_calloc(1, sizeof(*s));
    ogs_assert(s);
    s->session.name = (char *)dnn; s->session.session_type = type;
    /* No configured location lists in DNN-only cases. */
    s->nr_tai.tac.v = 999; s->nr_cgi.cell_id = 999;
    return s;
}

static void clear_peers(void)
{
    ogs_pfcp_node_remove_all(&ogs_pfcp_self()->pfcp_peer_list);
    ogs_pfcp_self()->pfcp_node = NULL;
    ogs_global_conf()->parameter.no_pfcp_rr_select = 0;
    ogs_pfcp_self()->learn_upf_dnn = true;
}

static void learned_rotation(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *a = peer(7, NULL), *b = peer(8, NULL), *c = peer(9, NULL);
    smf_sess_t *s = session("EdGe1", 1);
    snapshot(a, OGS_PFCP_POOL_VALID, "internet", 1);
    snapshot(b, OGS_PFCP_POOL_VALID, "edge1", 1);
    snapshot(c, OGS_PFCP_POOL_VALID, "edge1", 1);
    /* First selection starts at A: fallback would incorrectly select A. */
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, b, s->pfcp_node);
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, c, s->pfcp_node);
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, b, s->pfcp_node);
    s->session.name = "internet";
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, a, s->pfcp_node);
    ABTS_INT_EQUAL(tc, 0, b->num_of_dnn);
    ogs_free(s); clear_peers();
}

static void source_precedence(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *a = peer(7, "edge1"), *b = peer(8, "other");
    smf_sess_t *s = session("edge1", 1);
    unsigned i;
    snapshot(a, OGS_PFCP_POOL_VALID, "other", 1);
    snapshot(b, OGS_PFCP_POOL_VALID, "edge1", 1);
    for (i = 0; i < 2; i++) {
        ogs_pfcp_self()->learn_upf_dnn = i;
        ogs_pfcp_self()->pfcp_node = b;
        smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, i ? b : a, s->pfcp_node);
    }
    snapshot(a, OGS_PFCP_POOL_ABSENT, NULL, 0);
    ogs_pfcp_self()->pfcp_node = b;
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, a, s->pfcp_node);
    /* Invalidation is not ABSENT, even if the FSM remains associated. */
    ogs_pfcp_pool_snapshot_clear(a);
    ogs_pfcp_self()->pfcp_node = b;
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, b, s->pfcp_node);
    for (i = OGS_PFCP_POOL_UNUSABLE; i <= OGS_PFCP_POOL_FRAMING_ERROR; i++) {
        snapshot(a, i, NULL, 0);
        ogs_pfcp_self()->pfcp_node = b;
        smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, b, s->pfcp_node);
    }
    snapshot(a, OGS_PFCP_POOL_DEGRADED, "edge1", 1);
    ogs_pfcp_self()->pfcp_node = b;
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, a, s->pfcp_node);
    ABTS_STR_EQUAL(tc, "edge1", a->dnn[0]);
    ogs_free(s); clear_peers();
}

static void group_context(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *a = peer(7, "edge1"), *b = peer(8, NULL);
    smf_sess_t *s = session("edge1", 1);
    ogs_pfcp_pool_snapshot_t *p;
    unsigned family, type;
    for (family = 0; family <= 3; family++) {
        p = snapshot(a, OGS_PFCP_POOL_VALID, "edge1", family);
        snapshot(b, OGS_PFCP_POOL_VALID, "edge1", 0);
        for (type = 1; type <= 3; type++) {
            s->session.session_type = type;
            ogs_pfcp_self()->pfcp_node = b;
            smf_sess_select_upf(s);
            ABTS_PTR_EQUAL(tc, (!family || (family & type) == type) ? a : b,
                    s->pfcp_node);
        }
        /* Slice restrictions cannot become universal DNN authorization,
         * even if session S-NSSAI happens to match. */
        p->groups[0].num_slices = 1;
        p->groups[0].slices[0][0] = s->s_nssai.sst = 1;
        s->session.session_type = 1;
        ogs_pfcp_self()->pfcp_node = b;
        smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, b, s->pfcp_node);
    }
    p = snapshot(a, OGS_PFCP_POOL_VALID, "edge1", 1);
    p->num_groups = 2; p->groups[1] = p->groups[0]; p->groups[1].family = 2;
    s->session.session_type = 3; ogs_pfcp_self()->pfcp_node = b;
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, a, s->pfcp_node);
    p->groups[1].num_slices = 1; ogs_pfcp_self()->pfcp_node = b;
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, b, s->pfcp_node);
    /* Unknown/unstructured/Ethernet types never use learned DNNs; the
     * alternative retains legacy static matching irrespective of type. */
    b->dnn[0] = "edge1"; b->num_of_dnn = 1;
    snapshot(b, OGS_PFCP_POOL_ABSENT, NULL, 0);
    for (type = 0; type <= 5; type++) {
        if (type >= 1 && type <= 3) continue;
        s->session.session_type = type; ogs_pfcp_self()->pfcp_node = b;
        smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, b, s->pfcp_node);
    }
    ogs_free(s); clear_peers();
}

static void replacement_and_lifetime(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *a = peer(7, NULL), *b = peer(8, NULL);
    smf_sess_t *old = session("edge1", 1), *fresh = session("edge1", 1);
    smf_sess_t *before = ogs_malloc(sizeof(*before));
    snapshot(a, OGS_PFCP_POOL_VALID, "edge1", 1);
    snapshot(b, OGS_PFCP_POOL_VALID, "edge1", 1);
    smf_sess_select_upf(old); ABTS_PTR_EQUAL(tc, a, old->pfcp_node);
    memcpy(before, old, sizeof(*before));
    snapshot(a, OGS_PFCP_POOL_VALID, "internet", 1);
    ogs_pfcp_self()->pfcp_node = b;
    smf_sess_select_upf(fresh); ABTS_PTR_EQUAL(tc, b, fresh->pfcp_node);
    ABTS_TRUE(tc, !memcmp(before, old, sizeof(*old)));
    ogs_pfcp_pool_snapshot_clear(a);
    ABTS_TRUE(tc, !memcmp(before, old, sizeof(*old)));
    snapshot(a, OGS_PFCP_POOL_VALID, "edge1", 1);
    ogs_pfcp_self()->pfcp_node = b;
    smf_sess_select_upf(fresh); ABTS_PTR_EQUAL(tc, a, fresh->pfcp_node);
    ogs_free(before); ogs_free(old); ogs_free(fresh); clear_peers();
}

static void legacy_location_and_fallback(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *a = peer(7, NULL), *b = peer(8, NULL);
    smf_sess_t *s = session("edge1", 1);
    unsigned kind;
    snapshot(a, OGS_PFCP_POOL_UNUSABLE, NULL, 0);
    snapshot(b, OGS_PFCP_POOL_VALID, "edge1", 1);
    for (kind = 0; kind < 4; kind++) {
        a->num_of_e_cell_id = a->num_of_nr_cell_id = a->num_of_tac = 0;
        s->gtp_rat_type = OGS_GTP2_RAT_TYPE_EUTRAN;
        if (kind == 0) { a->num_of_e_cell_id = 1; a->e_cell_id[0] = s->e_cgi.cell_id = 42; }
        if (kind == 1) { a->num_of_nr_cell_id = 1; a->nr_cell_id[0] = s->nr_cgi.cell_id = 42; }
        if (kind == 2) { a->num_of_tac = 1; a->tac[0] = s->e_tai.tac = 42; }
        if (kind == 3) { a->num_of_tac = 1; a->tac[0] = s->nr_tai.tac.v = 43; s->gtp_rat_type = 0; }
        ogs_pfcp_self()->pfcp_node = b;
        smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, a, s->pfcp_node);
    }
    a->num_of_e_cell_id = a->num_of_nr_cell_id = a->num_of_tac = 0;
    s->session.name = "missing";
    ogs_pfcp_self()->pfcp_node = a;
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, b, s->pfcp_node); /* RR */
    ogs_global_conf()->parameter.no_pfcp_rr_select = 1;
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, a, s->pfcp_node); /* first */
    ogs_global_conf()->parameter.no_pfcp_rr_select = 0;
    OGS_FSM_TRAN(&a->sm, smf_pfcp_state_will_associate);
    s->session.name = "edge1";
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, b, s->pfcp_node);
    OGS_FSM_TRAN(&b->sm, smf_pfcp_state_will_associate);
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, a, s->pfcp_node); /* unavailable */
    OGS_FSM_TRAN(&a->sm, smf_pfcp_state_associated);
    snapshot(a, OGS_PFCP_POOL_VALID, "edge1", 1);
    smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, a, s->pfcp_node);
    ogs_free(s); clear_peers();
}

/* Frozen pre-4A policy oracle. Compare actual public selector results over
 * independent static/location/state/cursor combinations, feature off and
 * accepted ABSENT. This guards ordering/OR/fallback, not learned projection. */
static bool baseline_match(ogs_pfcp_node_t *n, smf_sess_t *s)
{
    unsigned i;
    for (i=0;i<n->num_of_dnn;i++)
        if (!ogs_strcasecmp(n->dnn[i],s->session.name)) return true;
    for (i=0;i<n->num_of_e_cell_id;i++)
        if (s->gtp_rat_type == OGS_GTP2_RAT_TYPE_EUTRAN && n->e_cell_id[i] == s->e_cgi.cell_id) return true;
    for (i=0;i<n->num_of_nr_cell_id;i++)
        if (n->nr_cell_id[i] == s->nr_cgi.cell_id) return true;
    for (i=0;i<n->num_of_tac;i++)
        if ((s->gtp_rat_type == OGS_GTP2_RAT_TYPE_EUTRAN && n->tac[i] == s->e_tai.tac) || n->tac[i] == s->nr_tai.tac.v) return true;
    return false;
}

static void baseline_differential(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *n[4];
    const char *dnns[] = {"internet", "edge1", "EDGE1", "missing"};
    smf_sess_t *s = session("edge1", 1);
    unsigned mask, rr, current, d, location, mode, i, step;
    for (i=0;i<4;i++) {
        n[i] = peer(7+i, i == 0 ? "internet" : i == 1 ? "edge1" : NULL);
        snapshot(n[i], OGS_PFCP_POOL_ABSENT, NULL, 0);
    }
    n[2]->num_of_e_cell_id=1; n[2]->e_cell_id[0]=42;
    n[2]->num_of_nr_cell_id=1; n[2]->nr_cell_id[0]=43;
    n[3]->num_of_tac=1; n[3]->tac[0]=44;
    for (mask=0;mask<16;mask++) for (rr=0;rr<2;rr++)
    for (current=0;current<4;current++) for (d=0;d<4;d++)
    for (location=0;location<6;location++) for (mode=0;mode<2;mode++) {
        ogs_pfcp_node_t *expected = NULL;
        for (i=0;i<4;i++) OGS_FSM_TRAN(&n[i]->sm,
                mask & (1<<i) ? smf_pfcp_state_associated : smf_pfcp_state_will_associate);
        ogs_global_conf()->parameter.no_pfcp_rr_select=rr;
        ogs_pfcp_self()->learn_upf_dnn=mode;
        ogs_pfcp_self()->pfcp_node=n[current];
        s->session.name=(char *)dnns[d];
        s->gtp_rat_type=location == 5 ? 0 : OGS_GTP2_RAT_TYPE_EUTRAN;
        s->e_cgi.cell_id=location == 1 ? 42 : 99;
        s->nr_cgi.cell_id=location == 2 ? 43 : 99;
        s->e_tai.tac=location == 3 || location == 5 ? 44 : 99;
        s->nr_tai.tac.v=location == 4 ? 44 : 99;
        for (step=1;step<=4;step++) {
            i=(current+step)%4;
            if ((mask & (1<<i)) && baseline_match(n[i],s)) { expected=n[i];break; }
        }
        if (!expected && !rr) for (step=1;step<=4;step++) {
            i=(current+step)%4;
            if (mask & (1<<i)) { expected=n[i];break; }
        }
        if (!expected) expected=n[0];
        smf_sess_select_upf(s); ABTS_PTR_EQUAL(tc, expected, s->pfcp_node);
    }
    ogs_free(s); clear_peers();
}

abts_suite *test_smf_selection(abts_suite *suite)
{
    ogs_app_context_init();
    ogs_app()->pool.nf = 8; ogs_app()->pool.sess = 8;
    ogs_pfcp_context_init();
    ogs_log_install_domain(&__smf_log_domain, "smf", OGS_LOG_NONE);
    ogs_log_set_domain_level(ogs_log_get_domain_id("pfcp"), OGS_LOG_NONE);
    ogs_pfcp_self()->learn_upf_dnn = true;
    suite = ADD_SUITE(suite);
    abts_run_test(suite, learned_rotation, NULL);
    abts_run_test(suite, source_precedence, NULL);
    abts_run_test(suite, group_context, NULL);
    abts_run_test(suite, replacement_and_lifetime, NULL);
    abts_run_test(suite, legacy_location_and_fallback, NULL);
    abts_run_test(suite, baseline_differential, NULL);
    ogs_pfcp_context_final(); ogs_app_context_final();
    return suite;
}
