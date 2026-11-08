/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "ogs-pfcp.h"
#include "core/abts.h"
#include "pfcp-pool-fixtures.h"

static unsigned failures;
static void failed(ogs_pfcp_xact_t *xact, void *data)
{
    unsigned *count = data;
    (*count)++;
}

static ogs_pkbuf_t *wire_packet(const uint8_t *wire, size_t len)
{
    ogs_pkbuf_t *p = ogs_pkbuf_alloc(NULL, ogs_max(len, 1));
    memcpy(ogs_pkbuf_put(p, len), wire, len);
    return p;
}

static size_t put_ie(uint8_t *p, unsigned type, const void *value, size_t len)
{
    p[0] = type>>8; p[1] = type; p[2] = len>>8; p[3] = len;
    if (len) memcpy(p+4, value, len);
    return len+4;
}

static ogs_pkbuf_t *setup_packet(unsigned type, uint32_t xid,
        const uint8_t *body, size_t len)
{
    ogs_pkbuf_t *p = ogs_pkbuf_alloc(NULL, len+8);
    uint8_t *h = ogs_pkbuf_put(p, 8);
    memset(h, 0, 8); h[0] = 0x20; h[1] = type;
    h[2] = (len+4)>>8; h[3] = len+4;
    h[4] = xid>>16; h[5] = xid>>8; h[6] = xid;
    if (len) memcpy(ogs_pkbuf_put(p, len), body, len);
    return p;
}

static ogs_pfcp_node_t *new_node(void)
{
    ogs_pfcp_node_t *node = ogs_pfcp_node_new(NULL);
    ogs_assert(node);
    ogs_assert(ogs_getaddrinfo(&node->addr_list, AF_INET, "127.0.0.8", 18805, 0) == OGS_OK);
    ogs_list_add(&ogs_pfcp_self()->pfcp_peer_list, node);
    node->dnn[0] = "static-example"; node->num_of_dnn = 1;
    return node;
}

static ogs_pfcp_xact_t *outstanding(ogs_pfcp_node_t *node)
{
    ogs_pfcp_xact_t *x = ogs_pfcp_xact_local_create(node, failed, &failures);
    ogs_pfcp_header_t h;
    ogs_pkbuf_t *body = ogs_pfcp_cp_build_association_setup_request(5);
    memset(&h, 0, sizeof(h)); h.type = 5;
    ogs_assert(x && body);
    ogs_assert(ogs_pfcp_xact_update_tx(x, &h, body) == OGS_OK);
    ogs_assert(ogs_pfcp_xact_commit(x) == OGS_OK);
    return x;
}

static bool deliver_request(ogs_pfcp_node_t *node, ogs_pfcp_message_t *m)
{
    ogs_pfcp_xact_t *x;
    ogs_assert(ogs_pfcp_xact_receive(node, &m->h, &x) == OGS_OK);
    return ogs_pfcp_cp_handle_association_setup_request(node, x,
            &m->pfcp_association_setup_request, &m->pool_information);
}

static bool deliver_response(ogs_pfcp_node_t *node, ogs_pfcp_message_t *m)
{
    ogs_pfcp_xact_t *x;
    ogs_assert(ogs_pfcp_xact_receive(node, &m->h, &x) == OGS_OK);
    return ogs_pfcp_cp_handle_association_setup_response(node, x,
            &m->pfcp_association_setup_response, &m->pool_information);
}

static ogs_pfcp_message_t *fixture_message(bool response, uint32_t xid)
{
    ogs_pkbuf_t *p = wire_packet(response ? pool_response_fixture : pool_request_fixture,
            response ? sizeof(pool_response_fixture) : sizeof(pool_request_fixture));
    ogs_pfcp_message_t *m;
    uint8_t *wire = p->data;
    wire[4] = xid>>16; wire[5] = xid>>8; wire[6] = xid;
    m = ogs_pfcp_parse_msg(p);
    ogs_assert(m);
    ogs_pkbuf_free(p); /* Message owns its filtered bytes. */
    return m;
}

static ogs_pfcp_xact_t *heartbeat_probe(ogs_pfcp_node_t *node)
{
    ogs_pfcp_xact_t *x = ogs_pfcp_xact_local_create(node, failed, &failures);
    ogs_pfcp_header_t h = {0};
    ogs_pkbuf_t *body = ogs_pfcp_build_heartbeat_request(1);
    h.type = 1;
    ogs_assert(x && body);
    ogs_assert(ogs_pfcp_xact_update_tx(x, &h, body) == OGS_OK);
    ogs_assert(ogs_pfcp_xact_commit(x) == OGS_OK);
    return x;
}

static bool heartbeat_reply(ogs_pfcp_node_t *node, ogs_pfcp_xact_t *x,
        uint32_t recovery)
{
    ogs_pfcp_header_t h = {0};
    ogs_pfcp_heartbeat_response_t hb = {0};
    h.type = 2; h.sqn = OGS_PFCP_XID_TO_SQN(x->xid);
    hb.recovery_time_stamp.presence = 1;
    hb.recovery_time_stamp.u32 = recovery;
    ogs_assert(ogs_pfcp_xact_receive(node, &h, &x) == OGS_OK);
    return ogs_pfcp_handle_heartbeat_response(node, x, &hb);
}

static bool heartbeat_request(ogs_pfcp_node_t *node, uint32_t xid,
        uint32_t recovery)
{
    ogs_pfcp_header_t h = {0};
    ogs_pfcp_heartbeat_request_t hb = {0};
    ogs_pfcp_xact_t *x;
    h.type = 1; h.sqn = OGS_PFCP_XID_TO_SQN(xid);
    hb.recovery_time_stamp.presence = 1;
    hb.recovery_time_stamp.u32 = recovery;
    ogs_assert(ogs_pfcp_xact_receive(node, &h, &x) == OGS_OK);
    return ogs_pfcp_handle_heartbeat_request(node, x, &hb);
}

static void test_restart_learning(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *node = new_node();
    ogs_pfcp_message_t *m;
    ogs_pfcp_pool_snapshot_t *fresh;
    ogs_pfcp_xact_t *probe, *old_probe;
    uint64_t generation;
    ogs_pfcp_self()->learn_upf_dnn = true;
    node->t_no_heartbeat = ogs_timer_add(ogs_app()->timer_mgr, NULL, NULL);
    ogs_assert(node->t_no_heartbeat);
    node->remote_recovery = 10;

    m = fixture_message(false, 601);
    /* Setup timestamps cannot establish/reset Heartbeat restart history. */
    m->pfcp_association_setup_request.recovery_time_stamp.u32 = 9999;
    ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    fresh = node->learned_pools; generation = node->association_generation;
    ABTS_INT_EQUAL(tc, 10, node->remote_recovery);
    probe = heartbeat_probe(node);
    ABTS_TRUE(tc, heartbeat_request(node, 1601, 11));
    ABTS_PTR_EQUAL(tc, fresh, node->learned_pools);
    ABTS_TRUE(tc, node->restoration_required);
    ABTS_TRUE(tc, heartbeat_reply(node, probe, 11));
    ABTS_PTR_EQUAL(tc, fresh, node->learned_pools);
    ABTS_INT_EQUAL(tc, generation, node->association_generation);
    /* An old delayed request must not unlearn the confirmed current boot. */
    ABTS_TRUE(tc, heartbeat_request(node, 1602, 10));
    ABTS_TRUE(tc, heartbeat_request(node, 1603, 11));
    ABTS_PTR_EQUAL(tc, fresh, node->learned_pools);
    /* A subsequent real restart without another Setup still invalidates. */
    ABTS_TRUE(tc, heartbeat_request(node, 1604, 12));
    ABTS_TRUE(tc, node->learned_pools == NULL);

    old_probe = heartbeat_probe(node);
    m = fixture_message(false, 602);
    ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    fresh = node->learned_pools;
    /* A pre-Setup probe reply cannot bind the replacement to the old boot. */
    ABTS_TRUE(tc, heartbeat_reply(node, old_probe, 12));
    ABTS_TRUE(tc, heartbeat_request(node, 1605, 13));
    ABTS_PTR_EQUAL(tc, fresh, node->learned_pools);
    ABTS_TRUE(tc, heartbeat_reply(node, heartbeat_probe(node), 13));
    ABTS_TRUE(tc, heartbeat_request(node, 1606, 14));
    ABTS_TRUE(tc, node->learned_pools == NULL);

    /* CP-initiated response learning obeys the same lifetime rule. */
    probe = outstanding(node);
    m = fixture_message(true, probe->xid);
    m->pfcp_association_setup_response.recovery_time_stamp.u32 = 1;
    ABTS_TRUE(tc, deliver_response(node, m));
    ogs_pfcp_message_free(m);
    fresh = node->learned_pools;
    ABTS_INT_EQUAL(tc, 14, node->remote_recovery);
    node->restoration_required = false;
    ABTS_TRUE(tc, heartbeat_reply(node, heartbeat_probe(node), 15));
    ABTS_PTR_EQUAL(tc, fresh, node->learned_pools);
    ABTS_TRUE(tc, node->restoration_required);
    /* A rejected replacement cannot reset the confirmation of old data. */
    probe = outstanding(node);
    m = fixture_message(true, probe->xid);
    m->pfcp_association_setup_response.cause.u8 = 64;
    ABTS_TRUE(tc, !deliver_response(node, m));
    ogs_pfcp_message_free(m);
    ABTS_PTR_EQUAL(tc, fresh, node->learned_pools);
    ABTS_TRUE(tc, heartbeat_request(node, 1608, 16));
    ABTS_TRUE(tc, node->learned_pools == NULL);
    ogs_timer_delete(node->t_no_heartbeat);
    node->t_no_heartbeat = NULL;
    ogs_pfcp_node_remove(&ogs_pfcp_self()->pfcp_peer_list, node);
    ogs_pfcp_self()->learn_upf_dnn = false;
}

static void test_learning(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *node = new_node();
    ogs_pfcp_xact_t *x;
    ogs_pfcp_message_t *m;
    ogs_pfcp_pool_snapshot_t *old;
    ogs_sock_t *sock;
    unsigned before;
    uint64_t generation;
    ogs_pfcp_self()->learn_upf_dnn = true;

    /* UP initiator, then CP initiator, with no surviving packet pointers. */
    m = fixture_message(false, 101);
    ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    ABTS_PTR_NOTNULL(tc, node->learned_pools);
    ABTS_INT_EQUAL(tc, 2, node->learned_pools->num_groups);
    ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_VALID, node->learned_pools->result.status);
    ABTS_STR_EQUAL(tc, "internet", node->learned_pools->groups[1].dnn);
    ABTS_INT_EQUAL(tc, 3, node->learned_pools->groups[0].family);
    ABTS_INT_EQUAL(tc, 2, node->learned_pools->groups[0].num_slices);
    ABTS_INT_EQUAL(tc, 0, node->learned_pools->groups[0].usable);
    ABTS_INT_EQUAL(tc, 3, node->learned_pools->groups[0].ids[0].len);
    ABTS_INT_EQUAL(tc, 0, node->learned_pools->groups[0].ids[0].data[1]);
    ABTS_INT_EQUAL(tc, 1, node->association_generation);
    ABTS_STR_EQUAL(tc, "static-example", node->dnn[0]);

    /* Transport retransmission returns RETRY; no handler/snapshot publication. */
    m = fixture_message(false, 101);
    ABTS_INT_EQUAL(tc, OGS_RETRY, ogs_pfcp_xact_receive(node, &m->h, &x));
    ABTS_INT_EQUAL(tc, 1, node->association_generation);
    ogs_pfcp_message_free(m);

    x = outstanding(node);
    m = fixture_message(true, x->xid);
    ABTS_TRUE(tc, deliver_response(node, m));
    ogs_pfcp_message_free(m);
    ABTS_INT_EQUAL(tc, 2, node->association_generation);
    ABTS_INT_EQUAL(tc, 6, node->learned_pools->source);
    ABTS_TRUE(tc, ogs_list_empty(&node->local_list));
    /* Repeated success is a replacement, never accumulation. */
    m = fixture_message(false, 102);
    ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    ABTS_INT_EQUAL(tc, 2, node->learned_pools->num_groups);

    old = node->learned_pools; generation = node->association_generation;
    /* Rejected replacement, including missing success-only timestamp. */
    x = outstanding(node); m = fixture_message(true, x->xid);
    m->pfcp_association_setup_response.cause.u8 = 64;
    m->pfcp_association_setup_response.recovery_time_stamp.presence = 0;
    before = failures;
    ABTS_TRUE(tc, !deliver_response(node, m));
    ABTS_INT_EQUAL(tc, before+1, failures);
    ABTS_PTR_EQUAL(tc, old, node->learned_pools);
    ABTS_INT_EQUAL(tc, generation, node->association_generation);
    ogs_pfcp_message_free(m);

    /* Incoming rejection and failed Accepted send preserve current snapshot. */
    m = fixture_message(false, 103);
    m->pfcp_association_setup_request.recovery_time_stamp.presence = 0;
    ABTS_TRUE(tc, !deliver_request(node, m));
    ABTS_PTR_EQUAL(tc, old, node->learned_pools);
    ogs_pfcp_message_free(m);
    m = fixture_message(false, 104);
    sock = ogs_pfcp_self()->pfcp_sock; ogs_pfcp_self()->pfcp_sock = NULL;
    ABTS_TRUE(tc, !deliver_request(node, m));
    ogs_pfcp_self()->pfcp_sock = sock;
    ABTS_PTR_EQUAL(tc, old, node->learned_pools);
    ABTS_INT_EQUAL(tc, generation, node->association_generation);
    /* Retry after failed send must re-enter handler, rather than cache Accepted. */
    ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);

    /* Absent announcement clears the previous list and stale feature bytes. */
    m = fixture_message(false, 105);
    memset(m->pfcp_association_setup_request.ue_ip_address_pool_information, 0,
            sizeof(m->pfcp_association_setup_request.ue_ip_address_pool_information));
    memset(&m->pool_information, 0, sizeof(m->pool_information));
    m->pfcp_association_setup_request.up_function_features.len = 1;
    ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_ABSENT, node->learned_pools->result.status);
    ABTS_INT_EQUAL(tc, 0, node->learned_pools->num_groups);
    ABTS_INT_EQUAL(tc, 0, node->up_function_features.octet11);
    ABTS_INT_EQUAL(tc, 1, node->up_function_features_len);

    /* Heartbeat restart invalidates an old confirmed snapshot even without
     * a state exit; a new accepted Setup is covered separately below. */
    m = fixture_message(false, 106); ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    {
        ogs_pfcp_header_t h = {0};
        ogs_pfcp_heartbeat_request_t hb = {0};
        node->remote_recovery = 10;
        node->t_no_heartbeat = ogs_timer_add(ogs_app()->timer_mgr, NULL, NULL);
        ogs_assert(node->t_no_heartbeat);
        ABTS_TRUE(tc, heartbeat_reply(node, heartbeat_probe(node), 10));
        h.type = 1; h.sqn = OGS_PFCP_XID_TO_SQN(777);
        ogs_assert(ogs_pfcp_xact_receive(node, &h, &x) == OGS_OK);
        hb.recovery_time_stamp.presence = 1; hb.recovery_time_stamp.u32 = 11;
        ABTS_TRUE(tc, ogs_pfcp_handle_heartbeat_request(node, x, &hb));
        ABTS_TRUE(tc, node->learned_pools == NULL);
        ABTS_TRUE(tc, node->restoration_required);
        ogs_timer_delete(node->t_no_heartbeat);
        node->t_no_heartbeat = NULL;
    }
    ABTS_STR_EQUAL(tc, "static-example", node->dnn[0]);
    /* Destruction frees a populated snapshot and all outstanding transactions. */
    m = fixture_message(false, 107); ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    ogs_pfcp_node_remove(&ogs_pfcp_self()->pfcp_peer_list, node);
    ogs_pfcp_self()->learn_upf_dnn = false;
}

static void test_snapshot_semantics(abts_case *tc, void *data)
{
    ogs_pfcp_message_t *m = fixture_message(false, 1);
    ogs_pfcp_tlv_ue_ip_address_pool_information_t *g =
        m->pfcp_association_setup_request.ue_ip_address_pool_information;
    ogs_pfcp_pool_snapshot_t *s;
    /* Identical records deduplicate, independent records remain owned. */
    g[2] = g[1];
    s = ogs_pfcp_pool_snapshot_prepare(g, &m->pool_information, 5);
    ABTS_INT_EQUAL(tc, 2, s->num_groups);
    ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_VALID, s->result.status); ogs_free(s);
    /* Same identity, incompatible DNN context: omit BOTH, never choose first. */
    g[2].network_instance = g[0].network_instance;
    s = ogs_pfcp_pool_snapshot_prepare(g, &m->pool_information, 5);
    ABTS_INT_EQUAL(tc, 1, s->num_groups);
    ABTS_STR_EQUAL(tc, "edge1", s->groups[0].dnn);
    ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_UNUSABLE, s->result.status); ogs_free(s);
    memset(&g[2], 0, sizeof(g[2]));
    g[1].network_instance.presence = 0;
    s = ogs_pfcp_pool_snapshot_prepare(g, &m->pool_information, 5);
    ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_UNUSABLE, s->result.status);
    ABTS_INT_EQUAL(tc, 2, s->num_groups); ogs_free(s);
    m->pool_information.status = OGS_PFCP_POOL_UNSUPPORTED;
    s = ogs_pfcp_pool_snapshot_prepare(g, &m->pool_information, 5);
    ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_UNSUPPORTED, s->result.status);
    ABTS_INT_EQUAL(tc, 0, s->num_groups); ogs_free(s);
    ogs_pfcp_message_free(m);
}

static void test_late_response(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *node = new_node();
    ogs_pfcp_xact_t *x;
    ogs_pfcp_message_t *m;
    unsigned before;
    uint64_t generation;
    ogs_pfcp_self()->learn_upf_dnn = true;
    m = fixture_message(false, 201);
    ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    x = outstanding(node);
    m = fixture_message(true, x->xid);
    generation = node->association_generation;
    ogs_pfcp_pool_snapshot_clear(node); /* Actual timeout/restart invalidator. */
    before = failures;
    ABTS_TRUE(tc, !deliver_response(node, m));
    ABTS_INT_EQUAL(tc, before+1, failures);
    ABTS_INT_EQUAL(tc, generation, node->association_generation);
    ABTS_TRUE(tc, node->learned_pools == NULL);
    ogs_pfcp_message_free(m);
    /* An accepted inbound replacement also obsoletes an outstanding Setup. */
    x = outstanding(node);
    m = fixture_message(false, 202);
    ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    m = fixture_message(true, x->xid);
    generation = node->association_generation;
    ABTS_TRUE(tc, !deliver_response(node, m));
    ABTS_INT_EQUAL(tc, generation, node->association_generation);
    ABTS_INT_EQUAL(tc, 2, node->learned_pools->num_groups);
    ogs_pfcp_message_free(m);
    ogs_pfcp_node_remove(&ogs_pfcp_self()->pfcp_peer_list, node);
    ogs_pfcp_self()->learn_upf_dnn = false;
}

static void test_advertisement(abts_case *tc, void *data)
{
    ogs_pfcp_subnet_t *v4, *v6, *generic;
    ogs_pfcp_message_t *m;
    ogs_pkbuf_t *body, *wire;
    ogs_pfcp_pool_snapshot_t *s;
    unsigned type;
    v4 = ogs_pfcp_subnet_add("10.48.0.0", "24", "10.48.0.1", "edge1", "testtun");
    v6 = ogs_pfcp_subnet_add("2001:db8:48::", "64", "2001:db8:48::1", "edge1", "testtun");
    generic = ogs_pfcp_subnet_add("10.49.0.0", "24", "10.49.0.1", NULL, "testtun");
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_validate_pool_config());
    ogs_pfcp_self()->advertise_ue_ip_pools = true;
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_validate_pool_config());
    strcpy(v4->pool_id, "edge1-v4-a"); strcpy(v6->pool_id, "edge1-v6-a");
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_validate_pool_config());
    strcpy(v6->pool_id, v4->pool_id);
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_validate_pool_config());
    strcpy(v6->pool_id, "edge1 v6");
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_validate_pool_config());
    strcpy(v6->pool_id, "edge1-v6-a");
    v6->pool_announcement_invalid = true;
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_validate_pool_config());
    v6->pool_announcement_invalid = false;
    for (type = 5; type <= 6; type++) {
        body = type == 5 ? ogs_pfcp_up_build_association_setup_request(5) :
            ogs_pfcp_up_build_association_setup_response(6, 1);
        ABTS_PTR_NOTNULL(tc, body);
        wire = setup_packet(type, 33, body->data, body->len);
        m = ogs_pfcp_parse_msg(wire);
        ogs_pkbuf_free(body); ogs_pkbuf_free(wire);
        ABTS_PTR_NOTNULL(tc, m);
        s = ogs_pfcp_pool_snapshot_prepare(type == 5 ?
                m->pfcp_association_setup_request.ue_ip_address_pool_information :
                m->pfcp_association_setup_response.ue_ip_address_pool_information,
                &m->pool_information, type);
        ogs_pfcp_message_free(m);
        ABTS_INT_EQUAL(tc, 2, s->num_groups);
        ABTS_STR_EQUAL(tc, "edge1", s->groups[0].dnn);
        ABTS_INT_EQUAL(tc, 1, s->groups[0].family); /* R19 V4 bit 1 */
        ABTS_INT_EQUAL(tc, 2, s->groups[1].family); /* R19 V6 bit 2 */
        ABTS_INT_EQUAL(tc, 10, s->groups[0].ids[0].len);
        ABTS_TRUE(tc, !memcmp(s->groups[0].ids[0].data, "edge1-v4-a", 10));
        ABTS_INT_EQUAL(tc, 0, s->groups[0].num_slices); ogs_free(s);
    }
    /* Rejection is reduced to Node ID + Cause, even with announcement enabled. */
    body = ogs_pfcp_up_build_association_setup_response(6, 64);
    wire = setup_packet(6, 34, body->data, body->len);
    m = ogs_pfcp_parse_msg(wire);
    ABTS_INT_EQUAL(tc, 0, m->pfcp_association_setup_response.recovery_time_stamp.presence);
    ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_ABSENT, m->pool_information.status);
    ogs_pfcp_message_free(m); ogs_pkbuf_free(wire); ogs_pkbuf_free(body);
    ogs_pfcp_self()->advertise_ue_ip_pools = false;
    ogs_pfcp_subnet_remove(generic); ogs_pfcp_subnet_remove(v6); ogs_pfcp_subnet_remove(v4);
}

static void test_erroneous_response(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *node = new_node();
    uint8_t body[64], id[] = {0,127,0,0,8}, timestamp[] = {0,0,0,1}, cause = 1;
    unsigned test, before, state;
    for (state = 0; state < 2; state++) {
        ogs_pfcp_pool_snapshot_t *old;
        if (state) {
            ogs_pfcp_message_t *m = fixture_message(false, 300);
            ogs_pfcp_self()->learn_upf_dnn = true;
            ABTS_TRUE(tc, deliver_request(node, m));
            ogs_pfcp_message_free(m);
        }
        old = node->learned_pools;
        for (test = 0; test < 13; test++) {
            ogs_pfcp_xact_t *x = outstanding(node);
            ogs_pool_id_t xact_id = x->id;
            ogs_pkbuf_t *p;
            size_t len = 0;
            before = failures;
            if (test != 0) len += put_ie(body+len, 60, id, test == 1 ? 4 : sizeof(id));
            if (test != 2) len += put_ie(body+len, 19, &cause, test == 3 ? 0 : 1);
            if (test != 4) len += put_ie(body+len, 96, timestamp, test == 5 ? 3 : 4);
            if (test == 6) len += put_ie(body+len, 233, "\0", 1);
            if (test == 10 || test == 11) { /* MPAS -> SMF Set ID */
                uint8_t flags = 0x20; len += put_ie(body+len, 89, &flags, 1);
                if (test == 11) len += put_ie(body+len, 180, "\0\10x", 3);
            }
            p = setup_packet(6, x->xid, body, len);
            if (test == 7) ((uint8_t *)p->data)[3]++; /* header length mismatch */
            if (test == 8) ogs_pkbuf_trim(p, 7); /* Cannot identify full header. */
            if (test == 9) ((uint8_t *)p->data)[6]++; /* Unmatched, never publish. */
            if (test == 12) ((uint8_t *)p->data)[0] = 0x40; /* Unsupported version. */
            ABTS_TRUE(tc, !ogs_pfcp_setup_pre_receive(p, node->addr_list, true));
            ogs_pkbuf_free(p);
            if (test == 8 || test == 9) {
                ABTS_PTR_NOTNULL(tc, ogs_pfcp_xact_find_by_id(xact_id));
                ABTS_TRUE(tc, x->tm_response->running);
                ABTS_INT_EQUAL(tc, before, failures);
                ogs_pfcp_xact_delete(x);
            } else {
                ABTS_TRUE(tc, ogs_pfcp_xact_find_by_id(xact_id) == NULL);
                ABTS_INT_EQUAL(tc, before+1, failures);
                ABTS_TRUE(tc, ogs_list_empty(&node->local_list));
            }
            ABTS_INT_EQUAL(tc, state, node->association_generation);
            ABTS_PTR_EQUAL(tc, old, node->learned_pools);
        }
    }
    ogs_pfcp_node_remove(&ogs_pfcp_self()->pfcp_peer_list, node);
    ogs_pfcp_self()->learn_upf_dnn = false;
}

static void test_up_handlers(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *node = new_node();
    ogs_pfcp_xact_t *x = outstanding(node);
    ogs_pfcp_message_t *m = fixture_message(true, x->xid);
    unsigned before = failures;
    m->pfcp_association_setup_response.cause.u8 = 64;
    ABTS_TRUE(tc, !ogs_pfcp_up_handle_association_setup_response(node, x,
                &m->pfcp_association_setup_response));
    ABTS_INT_EQUAL(tc, before+1, failures);
    ABTS_INT_EQUAL(tc, 0, node->association_generation);
    ogs_pfcp_message_free(m);
    x = outstanding(node); m = fixture_message(true, x->xid);
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_xact_receive(node, &m->h, &x));
    ABTS_TRUE(tc, ogs_pfcp_up_handle_association_setup_response(node, x,
                &m->pfcp_association_setup_response));
    ABTS_INT_EQUAL(tc, 1, node->association_generation);
    ogs_pfcp_message_free(m);
    m = fixture_message(false, 400);
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_xact_receive(node, &m->h, &x));
    m->pfcp_association_setup_request.recovery_time_stamp.presence = 0;
    ABTS_TRUE(tc, !ogs_pfcp_up_handle_association_setup_request(node, x,
                &m->pfcp_association_setup_request, false));
    ABTS_INT_EQUAL(tc, 1, node->association_generation);
    ogs_pfcp_message_free(m);
    m = fixture_message(false, 401);
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_xact_receive(node, &m->h, &x));
    ogs_pfcp_self()->cp_function_features.octet5 = 0xff;
    ABTS_TRUE(tc, ogs_pfcp_up_handle_association_setup_request(node, x,
                &m->pfcp_association_setup_request, false));
    ABTS_INT_EQUAL(tc, 0, ogs_pfcp_self()->cp_function_features.octet5);
    ABTS_INT_EQUAL(tc, 2, node->association_generation);
    ABTS_TRUE(tc, node->learned_pools == NULL);
    ogs_pfcp_message_free(m);
    ogs_pfcp_node_remove(&ogs_pfcp_self()->pfcp_peer_list, node);
}

static void test_extendable_setup(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *node = new_node();
    uint8_t body[128], id[] = {0,127,0,0,8,0xfe};
    uint8_t id6[18] = {1};
    uint8_t ts[] = {0,0,0,1,0xfe,0xed};
    uint8_t group[64];
    size_t group_len = put_ie(group, 177, "\0\1x\xfe", 4);
    unsigned type, node_type;
    id6[16] = 8; id6[17] = 0xfe;
    group_len += put_ie(group+group_len, 22, "\5edge1", 6);
    group_len += put_ie(group+group_len, 258, "\3\xfe", 2);
    ogs_pfcp_self()->learn_upf_dnn = true;
    for (node_type = 0; node_type < 2; node_type++)
    for (type = 5; type <= 6; type++) {
        ogs_pfcp_xact_t *x = type == 6 ? outstanding(node) : NULL;
        size_t len = put_ie(body, 60, node_type ? id6 : id,
                node_type ? sizeof(id6) : sizeof(id));
        ogs_pkbuf_t *p;
        ogs_pfcp_message_t *m;
        ogs_pfcp_node_id_t parsed_id;
        bool accepted;
        len += put_ie(body+len, 96, ts, sizeof(ts));
        len += put_ie(body+len, 233, group, group_len);
        if (type == 6) len += put_ie(body+len, 19, "\1", 1);
        p = setup_packet(type, x ? x->xid : 500+node_type, body, len);
        accepted = ogs_pfcp_setup_pre_receive(p, node->addr_list, true);
        ABTS_TRUE(tc, accepted);
        if (accepted) {
            m = ogs_pfcp_parse_msg(p);
            ABTS_PTR_NOTNULL(tc, m);
            if (m) {
                ABTS_INT_EQUAL(tc, OGS_PFCP_STATUS_SUCCESS,
                        ogs_pfcp_extract_node_id(m, &parsed_id));
                ABTS_TRUE(tc, type == 5 ? deliver_request(node, m) :
                        deliver_response(node, m));
                ABTS_INT_EQUAL(tc, 1, node->learned_pools->num_groups);
                ABTS_STR_EQUAL(tc, "edge1", node->learned_pools->groups[0].dnn);
                ABTS_INT_EQUAL(tc, 3, node->learned_pools->groups[0].family);
                ABTS_INT_EQUAL(tc, 1, node->learned_pools->groups[0].ids[0].len);
                ogs_pfcp_message_free(m);
            }
        }
        ogs_pkbuf_free(p);
    }
    ABTS_INT_EQUAL(tc, 4, node->association_generation);
    ogs_pfcp_self()->learn_upf_dnn = false;
    ogs_pfcp_node_remove(&ogs_pfcp_self()->pfcp_peer_list, node);
}

static void test_optional_association(abts_case *tc, void *data)
{
    ogs_pfcp_node_t *node = new_node();
    ogs_pfcp_xact_t *x;
    ogs_pkbuf_t *p;
    ogs_pfcp_message_t *m;
    uint8_t body[256], id[] = {0,127,0,0,8}, ts[] = {0,0,0,1};
    uint8_t f[12] = {0};
    unsigned i;
    size_t len;
    ogs_pfcp_self()->learn_upf_dnn = true;
    for (i = 0; i < 5; i++) {
        len = put_ie(body, 60, id, 5);
        len += put_ie(body+len, 96, ts, 4);
        if (i == 0) len += put_ie(body+len, 233, NULL, 0); /* Invalid optional group. */
        if (i == 1) len += put_ie(body+len, 89, NULL, 0); /* Invalid optional feature. */
        if (i == 2) len += put_ie(body+len, 89, "\0\x80", 2); /* Future CP prefix. */
        if (i == 3) { f[9] = 2; len += put_ie(body+len, 43, f, 12); } /* UN6DM, should */
        if (i == 4) len += put_ie(body+len, 253, "\0", 1); /* availability unknowable */
        p = setup_packet(5, 120+i, body, len);
        ABTS_TRUE(tc, ogs_pfcp_setup_pre_receive(p, node->addr_list, true));
        m = ogs_pfcp_parse_msg(p); ABTS_PTR_NOTNULL(tc, m);
        ABTS_TRUE(tc, deliver_request(node, m));
        ogs_pkbuf_free(p); ogs_pfcp_message_free(m);
        ABTS_INT_EQUAL(tc, i == 0 ? OGS_PFCP_POOL_UNUSABLE : OGS_PFCP_POOL_ABSENT,
                node->learned_pools->result.status);
    }
    /* Unknown/missing UPF Instance ID and Packet Inspection do not prove an
     * association error; this NF does not implement measurement/inspection. */
    ogs_pfcp_self()->learn_upf_dnn = false;
    m = fixture_message(false, 130); ABTS_TRUE(tc, deliver_request(node, m));
    ogs_pfcp_message_free(m);
    ABTS_TRUE(tc, node->learned_pools == NULL);
    /* Late/unsolicited valid response cannot create a transaction. */
    m = fixture_message(true, 0xdead);
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_xact_receive(node, &m->h, &x));
    ogs_pfcp_message_free(m);
    ogs_pfcp_node_remove(&ogs_pfcp_self()->pfcp_peer_list, node);
}

/* Inspect literal TLVs independently of the encoder/decoder round trip. */
static const uint8_t *body_ie(const ogs_pkbuf_t *body, unsigned type,
        unsigned *length)
{
    const uint8_t *p = body->data;
    size_t pos = 0;
    while (pos+4 <= body->len) {
        unsigned t = (p[pos]<<8) | p[pos+1];
        unsigned n = (p[pos+2]<<8) | p[pos+3];
        ogs_assert(n <= body->len-pos-4);
        if (t == type) { *length = n; return p+pos+4; }
        pos += 4+n;
    }
    return NULL;
}

static void test_retention_encoding(abts_case *tc, void *data)
{
    static const uint8_t repeated_retention[] = {
        0,60,0,5,0,127,0,0,4, 0,96,0,4,0,0,0,1,
        0,183,0,0, 0,183,0,9,0,185,0,5,1,127,0,0,4
    };
    uint8_t reversed[sizeof(repeated_retention)];
    ogs_pkbuf_t *body, *wire;
    ogs_pfcp_message_t *m;
    const uint8_t *value;
    unsigned len = 999;
    body = ogs_pfcp_cp_build_association_setup_request(5);
    ABTS_TRUE(tc, body_ie(body, 183, &len) == NULL);
    ogs_pkbuf_free(body);
    body = ogs_pfcp_cp_build_association_setup_request_with_retention(5, true);
    value = body_ie(body, 183, &len);
    ABTS_PTR_NOTNULL(tc, value);
    ABTS_INT_EQUAL(tc, 0, len); /* Literal IE: 00 b7 00 00. */
    wire = setup_packet(5, 2001, body->data, body->len);
    m = ogs_pfcp_parse_msg(wire);
    ABTS_PTR_NOTNULL(tc, m);
    ABTS_TRUE(tc, m->pfcp_association_setup_request.pfcp_session_retention_information.presence);
    ABTS_TRUE(tc, !m->pfcp_association_setup_request.pfcp_session_retention_information.cp_pfcp_entity_ip_address.presence);
    ogs_pfcp_message_free(m); ogs_pkbuf_free(wire); ogs_pkbuf_free(body);
    body = ogs_pfcp_up_build_association_setup_response_with_retention(6, 1, true);
    value = body_ie(body, 184, &len);
    ABTS_PTR_NOTNULL(tc, value);
    ABTS_INT_EQUAL(tc, 1, len);
    ABTS_INT_EQUAL(tc, 1, value[0]); /* Literal IE: 00 b8 00 01 01. */
    ogs_pkbuf_free(body);
    body = ogs_pfcp_up_build_association_setup_response_with_retention(6, 64, true);
    ABTS_TRUE(tc, body_ie(body, 184, &len) == NULL);
    ogs_pkbuf_free(body);
    body = ogs_pfcp_up_build_association_setup_response(6, 1);
    ABTS_TRUE(tc, body_ie(body, 184, &len) == NULL);
    ogs_pkbuf_free(body);
    /* Independent literal bytes: first singleton wins in either order. */
    wire = setup_packet(5, 2002, repeated_retention, sizeof(repeated_retention));
    m = ogs_pfcp_parse_msg(wire);
    ABTS_PTR_NOTNULL(tc, m);
    ABTS_TRUE(tc, m->pfcp_association_setup_request.pfcp_session_retention_information.presence);
    ABTS_TRUE(tc, !m->pfcp_association_setup_request.pfcp_session_retention_information.cp_pfcp_entity_ip_address.presence);
    ogs_pfcp_message_free(m); ogs_pkbuf_free(wire);
    memcpy(reversed, repeated_retention, 17);
    memcpy(reversed+17, repeated_retention+21, 13);
    memcpy(reversed+30, repeated_retention+17, 4);
    wire = setup_packet(5, 2003, reversed, sizeof(reversed));
    m = ogs_pfcp_parse_msg(wire);
    ABTS_PTR_NOTNULL(tc, m);
    /* A selective first group is unsupported, never broadened to the later
     * all-session group, and does not reject the otherwise valid Setup. */
    ABTS_TRUE(tc, !m->pfcp_association_setup_request.pfcp_session_retention_information.presence);
    ogs_pfcp_message_free(m); ogs_pkbuf_free(wire);
    /* An unknown optional child must not become an empty/all-session request. */
    reversed[19] = 0; reversed[20] = 4;
    reversed[21] = 255; reversed[22] = 255;
    reversed[23] = 0; reversed[24] = 0;
    wire = setup_packet(5, 2004, reversed, 25);
    m = ogs_pfcp_parse_msg(wire);
    ABTS_PTR_NOTNULL(tc, m);
    ABTS_TRUE(tc, !m->pfcp_association_setup_request.pfcp_session_retention_information.presence);
    ogs_pfcp_message_free(m); ogs_pkbuf_free(wire);
    wire = setup_packet(5, 2005, reversed, 24); /* Truncated child framing. */
    ABTS_TRUE(tc, ogs_pfcp_parse_msg(wire) == NULL);
    ogs_pkbuf_free(wire);
}

abts_suite *test_pfcp_association(abts_suite *suite)
{
    ogs_sockaddr_t *addr;
    ogs_sock_t *socket;
    int id;
    ogs_log_level_e level;
    ogs_app_context_init();
    ogs_app()->pool.nf = 8; ogs_app()->pool.sess = 8; ogs_app()->pool.xact = 64;
    ogs_app()->timer_mgr = ogs_timer_mgr_create(256);
    ogs_pfcp_context_init(); ogs_pfcp_xact_init();
    ogs_assert(ogs_getaddrinfo(&addr, AF_INET, "127.0.0.4", 0, 0) == OGS_OK);
    socket = ogs_sock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ogs_assert(socket);
    ogs_pfcp_self()->pfcp_sock = socket; ogs_pfcp_self()->pfcp_addr = addr;
    ogs_pfcp_self()->up_function_features_len = 4;
    ogs_pfcp_self()->up_function_features.ftup = 1;
    ogs_local_conf()->time.message.pfcp.t1_response_duration = ogs_time_from_sec(30);
    ogs_local_conf()->time.message.pfcp.t1_holding_duration = ogs_time_from_sec(30);
    ogs_local_conf()->time.message.pfcp.n1_response_rcount = 3;
    ogs_local_conf()->time.message.pfcp.n1_holding_rcount = 1;
    ogs_local_conf()->time.message.pfcp.no_heartbeat_duration = ogs_time_from_sec(30);
    suite = ADD_SUITE(suite);
    id = ogs_log_get_domain_id("pfcp"); level = ogs_log_get_domain_level(id);
    ogs_log_set_domain_level(id, OGS_LOG_NONE);
    abts_run_test(suite, test_learning, NULL);
    abts_run_test(suite, test_restart_learning, NULL);
    abts_run_test(suite, test_snapshot_semantics, NULL);
    abts_run_test(suite, test_late_response, NULL);
    abts_run_test(suite, test_advertisement, NULL);
    abts_run_test(suite, test_erroneous_response, NULL);
    abts_run_test(suite, test_up_handlers, NULL);
    abts_run_test(suite, test_extendable_setup, NULL);
    abts_run_test(suite, test_optional_association, NULL);
    abts_run_test(suite, test_retention_encoding, NULL);
    ogs_log_set_domain_level(id, level);
    ogs_sock_destroy(socket); ogs_freeaddrinfo(addr);
    ogs_pfcp_context_final(); ogs_pfcp_xact_final(); ogs_app_context_final();
    return suite;
}
