/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "ogs-pfcp.h"

static int id_character(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':' || c == '-';
}

int ogs_pfcp_validate_pool_config(void)
{
    ogs_pfcp_subnet_t *subnet, *other;
    unsigned count = 0;
    size_t i, len;
    uint8_t network[OGS_MAX_DNN_LEN+1];
    char dnn[OGS_MAX_DNN_LEN+1];
    ogs_tlv_octet_t ni;

    if (!ogs_pfcp_self()->advertise_ue_ip_pools) return OGS_OK;
    ogs_list_for_each(&ogs_pfcp_self()->subnet_list, subnet) {
        if (!subnet->dnn[0]) {
            ogs_warn("Pool announcement: skip generic session without DNN");
            continue;
        }
        len = strlen(subnet->pool_id);
        if (subnet->pool_announcement_invalid || !len ||
                len > OGS_PFCP_MAX_POOL_ID_LEN) goto invalid;
        for (i = 0; i < len; i++)
            if (!id_character(subnet->pool_id[i])) goto invalid;
        ogs_list_for_each(&ogs_pfcp_self()->subnet_list, other) {
            if (other != subnet && other->dnn[0] &&
                    !strcmp(subnet->pool_id, other->pool_id)) goto invalid;
        }
        if (++count > OGS_PFCP_MAX_POOL_INFORMATION ||
                (subnet->family != AF_INET && subnet->family != AF_INET6))
            goto invalid;
        memset(&ni, 0, sizeof(ni));
        ni.data = network;
        ni.len = ogs_fqdn_build((char *)network, subnet->dnn, strlen(subnet->dnn));
        if (ogs_pfcp_parse_pool_network_instance(dnn, sizeof(dnn), &ni) != OGS_OK)
            goto invalid;
    }
    return OGS_OK;
invalid:
    ogs_error("Invalid UPF pool announcement configuration: DNN[%s] pool_id[%s]",
            subnet->dnn, subnet->pool_id);
    return OGS_ERROR;
}

int ogs_pfcp_build_pool_information(
        ogs_pfcp_tlv_ue_ip_address_pool_information_t *groups,
        ogs_pfcp_pool_build_storage_t *storage)
{
    ogs_pfcp_subnet_t *subnet;
    unsigned i = 0;
    if (!ogs_pfcp_self()->advertise_ue_ip_pools) return OGS_OK;
    if (ogs_pfcp_validate_pool_config() != OGS_OK) return OGS_ERROR;
    ogs_list_for_each(&ogs_pfcp_self()->subnet_list, subnet) {
        ogs_pfcp_tlv_ue_ip_address_pool_information_t *g;
        if (!subnet->dnn[0]) continue;
        g = &groups[i];
        g->presence = 1;
        if (ogs_pfcp_build_pool_identity(&g->ue_ip_address_pool_identity[0],
                    subnet->pool_id, strlen(subnet->pool_id),
                    storage->identity[i], sizeof(storage->identity[i])) != OGS_OK)
            return OGS_ERROR;
        g->network_instance.presence = 1;
        g->network_instance.data = storage->network[i];
        g->network_instance.len = ogs_fqdn_build((char *)storage->network[i],
                subnet->dnn, strlen(subnet->dnn));
        storage->family[i] = subnet->family == AF_INET ?
            OGS_PFCP_POOL_IP_V4 : OGS_PFCP_POOL_IP_V6;
        g->ip_version.presence = 1;
        g->ip_version.data = &storage->family[i];
        g->ip_version.len = 1;
        i++;
    }
    return OGS_OK;
}

static bool same_context(const ogs_pfcp_learned_pool_t *a,
        const ogs_pfcp_learned_pool_t *b)
{
    return !ogs_strcasecmp(a->dnn, b->dnn) &&
        a->family_present == b->family_present && a->family == b->family &&
        a->num_slices == b->num_slices &&
        !memcmp(a->slices, b->slices, sizeof(a->slices));
}

static bool same_identity(const ogs_pfcp_pool_identity_t *a,
        const ogs_pfcp_pool_identity_t *b)
{
    return a->len == b->len && !memcmp(a->data, b->data, a->len);
}

static int compare_identity(const void *a, const void *b)
{
    const ogs_pfcp_pool_identity_t *x = a, *y = b;
    int cmp = memcmp(x->data, y->data, ogs_min(x->len, y->len));
    return cmp ? cmp : (int)x->len-(int)y->len;
}

static int compare_slice(const void *a, const void *b)
{
    return memcmp(a, b, 4);
}

ogs_pfcp_pool_snapshot_t *ogs_pfcp_pool_snapshot_prepare(
        const ogs_pfcp_tlv_ue_ip_address_pool_information_t *groups,
        const ogs_pfcp_pool_decode_result_t *result, uint8_t source)
{
    ogs_pfcp_pool_snapshot_t *s = ogs_calloc(1, sizeof(*s));
    unsigned i, j, k, a, b, usable = 0;
    bool omit[OGS_PFCP_MAX_POOL_INFORMATION] = {false};
    bool conflict[OGS_PFCP_MAX_POOL_INFORMATION] = {false};
    if (!s) return NULL;
    s->result = *result;
    s->source = source;
    if (result->status == OGS_PFCP_POOL_UNSUPPORTED ||
            result->status == OGS_PFCP_POOL_FRAMING_ERROR) return s;
    for (i = 0; i < OGS_PFCP_MAX_POOL_INFORMATION; i++) {
        const ogs_pfcp_tlv_ue_ip_address_pool_information_t *g = &groups[i];
        ogs_pfcp_learned_pool_t *p;
        if (!g->presence) continue;
        p = &s->groups[s->num_groups];
        for (j = 0; j < OGS_PFCP_MAX_POOL_IDENTITIES; j++) {
            const uint8_t *id;
            size_t len;
            if (!g->ue_ip_address_pool_identity[j].presence) continue;
            if (ogs_pfcp_parse_pool_identity(&g->ue_ip_address_pool_identity[j],
                        &id, &len) != OGS_OK || len > OGS_PFCP_MAX_POOL_ID_LEN)
                goto invalid_group;
            for (k = 0; k < p->num_ids; k++)
                if (p->ids[k].len == len && !memcmp(p->ids[k].data, id, len)) break;
            if (k != p->num_ids) continue;
            p->ids[k].len = len;
            memcpy(p->ids[k].data, id, len);
            p->num_ids++;
        }
        if (!p->num_ids) goto invalid_group;
        if (g->network_instance.presence &&
                ogs_pfcp_parse_pool_network_instance(p->dnn, sizeof(p->dnn),
                    &g->network_instance) != OGS_OK) goto invalid_group;
        if (g->ip_version.presence) {
            if (!g->ip_version.data || g->ip_version.len != 1 ||
                    !(*(uint8_t *)g->ip_version.data & 3)) goto invalid_group;
            p->family_present = true;
            p->family = *(uint8_t *)g->ip_version.data & 3;
        }
        for (j = 0; j < OGS_PFCP_MAX_POOL_SLICES; j++) {
            if (!g->s_nssai[j].presence) continue;
            if (!g->s_nssai[j].data || g->s_nssai[j].len != 4) goto invalid_group;
            memcpy(p->slices[p->num_slices++], g->s_nssai[j].data, 4);
        }
        p->usable = p->dnn[0] && !p->num_slices;
        qsort(p->ids, p->num_ids, sizeof(p->ids[0]), compare_identity);
        qsort(p->slices, p->num_slices, sizeof(p->slices[0]), compare_slice);
        s->num_groups++;
        continue;
invalid_group:
        memset(p, 0, sizeof(*p));
        s->result.groups_invalid++;
    }
    /* Identity reuse in distinct contexts is ambiguous. Remove both contexts,
     * including chains of conflicts; never let iteration order choose a winner. */
    for (i = 0; i < s->num_groups; i++) {
        for (j = i+1; j < s->num_groups; j++) {
            if (!memcmp(&s->groups[i], &s->groups[j], sizeof(s->groups[i]))) {
                omit[j] = true;
                continue;
            }
            if (same_context(&s->groups[i], &s->groups[j])) continue;
            for (a = 0; a < s->groups[i].num_ids; a++)
                for (b = 0; b < s->groups[j].num_ids; b++)
                    if (same_identity(&s->groups[i].ids[a], &s->groups[j].ids[b])) {
                        omit[i] = omit[j] = true;
                        conflict[i] = conflict[j] = true;
                    }
        }
    }
    for (i = j = 0; i < s->num_groups; i++) {
        if (omit[i]) { s->result.groups_invalid += conflict[i]; continue; }
        if (i != j) s->groups[j] = s->groups[i];
        usable += s->groups[j++].usable;
    }
    s->num_groups = j;
    s->result.groups_decoded = j;
    if (result->status != OGS_PFCP_POOL_ABSENT)
        s->result.status = !usable ? OGS_PFCP_POOL_UNUSABLE :
            s->result.groups_invalid ? OGS_PFCP_POOL_DEGRADED : OGS_PFCP_POOL_VALID;
    return s;
}

void ogs_pfcp_pool_snapshot_clear(ogs_pfcp_node_t *node)
{
    node->association_epoch++;
    if (node->learned_pools)
        ogs_debug("PFCP learned pools invalidated epoch[%llu]",
                (unsigned long long)node->association_epoch);
    if (node->learned_pools) ogs_free(node->learned_pools);
    node->learned_pools = NULL;
}

void ogs_pfcp_pool_snapshot_heartbeat(ogs_pfcp_node_t *node,
        uint32_t recovery, bool current_probe_response)
{
    ogs_pfcp_pool_snapshot_t *snapshot = node->learned_pools;
    if (!snapshot) return;
    if (snapshot->heartbeat_confirmed) {
        /* Invalidate information confirmed for an older boot. A delayed
         * old Heartbeat must not unlearn an already confirmed newer boot. */
        if (recovery > snapshot->heartbeat_recovery)
            ogs_pfcp_pool_snapshot_clear(node);
    } else if (current_probe_response && recovery >= node->remote_recovery) {
        /* A replacement Setup has already retired the previous snapshot.
         * The first current probe confirms the replacement, including when
         * Heartbeat detects the restart only AFTER that Setup was accepted.
         * Unsolicited requests and pre-Setup probe replies cannot bind it. */
        snapshot->heartbeat_recovery = recovery;
        snapshot->heartbeat_confirmed = true;
        ogs_debug("PFCP learned pools heartbeat confirmed peer[%s] "
                "generation[%llu] recovery[%u]",
                ogs_pfcp_node_id_to_string_static(&node->node_id),
                (unsigned long long)snapshot->generation, recovery);
    }
}

void ogs_pfcp_pool_snapshot_publish(ogs_pfcp_node_t *node,
        ogs_pfcp_pool_snapshot_t *snapshot)
{
    ogs_pfcp_pool_snapshot_t *old = node->learned_pools;
    unsigned i;
    node->association_epoch++;
    node->association_generation++;
    if (snapshot) snapshot->generation = node->association_generation;
    node->learned_pools = snapshot;
    if (old) ogs_free(old);
    if (!snapshot) return;
    ogs_debug("PFCP learned pools peer[%s] generation[%llu] source[%u] "
            "status[%d] groups[%u] ignored[%u]",
            ogs_pfcp_node_id_to_string_static(&node->node_id),
            (unsigned long long)snapshot->generation, snapshot->source, snapshot->result.status,
            snapshot->num_groups, snapshot->result.groups_invalid);
    for (i = 0; i < snapshot->num_groups; i++)
        ogs_debug("PFCP pool DNN[%s] family[%u] slices[%u] usable[%d]",
                snapshot->groups[i].dnn, snapshot->groups[i].family,
                snapshot->groups[i].num_slices, snapshot->groups[i].usable);
}

static bool valid_fqdn(const uint8_t *p, size_t len)
{
    size_t pos;
    if (!len || len > OGS_MAX_FQDN_LEN) return false;
    for (pos = 0; pos < len;) {
        unsigned n = p[pos++];
        if (!n || n > 63 || n > len-pos || memchr(p+pos, 0, n))
            return false;
        pos += n;
    }
    return true;
}

static bool valid_node_id(const ogs_pfcp_tlv_node_id_t *node)
{
    const uint8_t *p = node->data;
    if (!node->presence || !p || !node->len) return false;
    switch (p[0] & 15) {
    case OGS_PFCP_NODE_ID_IPV4: return node->len == 5;
    case OGS_PFCP_NODE_ID_IPV6: return node->len == 17;
    case OGS_PFCP_NODE_ID_FQDN:
        return valid_fqdn(p+1, node->len-1);
    default: return false;
    }
}

uint8_t ogs_pfcp_validate_setup(const ogs_pfcp_tlv_node_id_t *node,
        const ogs_pfcp_tlv_recovery_time_stamp_t *recovery,
        const ogs_pfcp_tlv_cause_t *cause)
{
    if (!node->presence || (cause && !cause->presence))
        return OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    if (!valid_node_id(node)) return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
    /* TS 29.244 7.2.3.2: success-only fields are not required on rejection. */
    if ((!cause || cause->u8 == OGS_PFCP_CAUSE_REQUEST_ACCEPTED) && !recovery->presence)
        return OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}

void ogs_pfcp_setup_response_error(ogs_pfcp_xact_t *xact)
{
    ogs_error("PFCP Association Setup failed peer[%s] transaction[%u]",
            ogs_sockaddr_to_string_static(xact->node->addr_list), xact->xid);
    if (xact->tm_response) ogs_timer_stop(xact->tm_response);
    if (xact->cb) xact->cb(xact, xact->data);
    ogs_pfcp_xact_delete(xact);
}

/* Raw validation retains information lost by numeric TLV decoding. */
static uint8_t validate_wire(const uint8_t *p, size_t len, bool response)
{
    ogs_pfcp_tlv_node_id_t node = {0};
    ogs_pfcp_tlv_recovery_time_stamp_t recovery = {0};
    ogs_pfcp_tlv_cause_t cause = {0};
    ogs_pfcp_pool_decode_result_t pools;
    bool bad_recovery = false, bad_cause = false;
    unsigned cp_flags = 0;
    bool cp_seen = false, set_id = false, valid_set_id = false;
    bool up_seen = false, un6dm = false, n6_protocols = false;
    if (ogs_pfcp_check_tlv_framing(response ?
                &ogs_pfcp_msg_desc_pfcp_association_setup_response :
                &ogs_pfcp_msg_desc_pfcp_association_setup_request, p, len) != OGS_OK ||
            ogs_pfcp_scan_pool_information(p, len, &pools) != OGS_OK)
        return OGS_PFCP_CAUSE_INVALID_LENGTH;
    while (len) {
        unsigned type = ((unsigned)p[0]<<8) | p[1];
        unsigned n = ((unsigned)p[2]<<8) | p[3];
        if (type == OGS_PFCP_NODE_ID_TYPE && !node.presence) {
            node.presence = 1; node.data = (void *)(p+4); node.len = n;
        } else if (type == OGS_PFCP_RECOVERY_TIME_STAMP_TYPE && !recovery.presence) {
            recovery.presence = 1; bad_recovery = n != 4;
        } else if (type == OGS_PFCP_CAUSE_TYPE && !cause.presence) {
            cause.presence = 1; bad_cause = n != 1;
            if (n) cause.u8 = p[4];
        } else if (type == OGS_PFCP_CP_FUNCTION_FEATURES_TYPE && !cp_seen) {
            cp_seen = true;
            if (n) cp_flags = p[4];
        } else if (type == OGS_PFCP_SMF_SET_ID_TYPE && !set_id) {
            set_id = true;
            valid_set_id = n > 1 && valid_fqdn(p+5, n-1);
        } else if (type == OGS_PFCP_UP_FUNCTION_FEATURES_TYPE && !up_seen) {
            up_seen = true;
            /* R19 octet 14 bit 2 (value index 9), beyond the legacy struct. */
            un6dm = n >= 10 && (p[4+9] & 2);
        } else if (type == OGS_PFCP_N6_DELAY_MEASUREMENT_PROTOCOLS_TYPE) {
            n6_protocols = n != 0;
        }
        p += n+4; len -= n+4;
    }
    if (bad_cause && response) return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
    if (bad_recovery && (!response || cause.u8 == OGS_PFCP_CAUSE_REQUEST_ACCEPTED))
        return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
    if ((!response || cause.u8 == OGS_PFCP_CAUSE_REQUEST_ACCEPTED) &&
            (cp_flags & 0x20)) {
        if (!set_id) return OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING;
        if (!valid_set_id) return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
    }
    /* The Setup table says "should", not "shall" for these protocols.
     * This implementation performs no N6 measurements; do not invent support
     * or reject an association solely because this recommendation was missed. */
    if (un6dm && !n6_protocols)
        ogs_warn("PFCP UN6DM advertised without recommended N6 measurement protocols");
    return ogs_pfcp_validate_setup(&node, &recovery, response ? &cause : NULL);
}

bool ogs_pfcp_setup_pre_receive(ogs_pkbuf_t *packet,
        ogs_sockaddr_t *from, bool cp)
{
    const uint8_t *p = packet->data;
    ogs_pfcp_node_t *node;
    ogs_pfcp_xact_t *xact;
    uint8_t cause;
    uint32_t xid;
    bool response;
    if (packet->len < 2 || (p[1] != OGS_PFCP_ASSOCIATION_SETUP_REQUEST_TYPE &&
                p[1] != OGS_PFCP_ASSOCIATION_SETUP_RESPONSE_TYPE)) return true;
    /* A complete non-SEID v1 header is necessary for safe correlation. */
    if (packet->len < 8) return false;
    response = p[1] == OGS_PFCP_ASSOCIATION_SETUP_RESPONSE_TYPE;
    if ((p[0] >> 5) != OGS_PFCP_VERSION) {
        /* recvfrom sends Version Not Supported before calling this path. */
        if (!response) return false;
        cause = OGS_PFCP_CAUSE_REQUEST_REJECTED;
    } else if ((p[0] & 1) || packet->len != 4+(((unsigned)p[2]<<8)|p[3]))
        cause = OGS_PFCP_CAUSE_INVALID_LENGTH;
    else cause = validate_wire(p+8, packet->len-8, response);
    if ((p[0] & 1) && packet->len < 16) return false;
    {
        const uint8_t *seq = p + ((p[0] & 1) ? 12 : 4);
        xid = ((uint32_t)seq[0]<<16) | ((uint32_t)seq[1]<<8) | seq[2];
    }
    node = ogs_pfcp_node_find(&ogs_pfcp_self()->pfcp_peer_list, NULL, from);
    if (cause == OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
        if (!response) return true;
        if (node) ogs_list_for_each(&node->local_list, xact) {
            if (xact->xid == xid && xact->step == 1 &&
                    xact->seq[0].type == OGS_PFCP_ASSOCIATION_SETUP_REQUEST_TYPE) {
                if (xact->association_epoch != node->association_epoch) {
                    ogs_pfcp_setup_response_error(xact);
                    return false;
                }
                return true;
            }
        }
        ogs_debug("Unmatched PFCP Setup response from[%s] transaction[%u]",
                ogs_sockaddr_to_string_static(from), xid);
        return false;
    }
    ogs_warn("Invalid PFCP Setup %s cause[%u] from[%s]",
            response ? "response" : "request", cause,
            ogs_sockaddr_to_string_static(from));
    if (response) {
        if (node) ogs_list_for_each(&node->local_list, xact) {
            if (xact->xid == xid && xact->step == 1 &&
                    xact->seq[0].type == OGS_PFCP_ASSOCIATION_SETUP_REQUEST_TYPE) {
                ogs_pfcp_setup_response_error(xact);
                break;
            }
        }
    } else {
        /* A temporary destination avoids creating/merging a peer from a rejected
         * identity. No transaction or node can outlive this synchronous reply. */
        ogs_pfcp_node_t destination;
        ogs_pkbuf_t *body = cp ?
            ogs_pfcp_cp_build_association_setup_response(6, cause) :
            ogs_pfcp_up_build_association_setup_response(6, cause);
        if (body) {
            uint8_t *h = ogs_pkbuf_push(body, 8);
            memset(h, 0, 8);
            h[0] = 0x20; h[1] = 6;
            h[2] = (body->len-4)>>8; h[3] = body->len-4;
            memcpy(h+4, p + ((p[0] & 1) ? 12 : 4), 3);
            memset(&destination, 0, sizeof(destination));
            destination.addr_list = from;
            ogs_expect(ogs_pfcp_sendto(&destination, body) == OGS_OK);
            ogs_pkbuf_free(body);
        }
    }
    return false;
}
