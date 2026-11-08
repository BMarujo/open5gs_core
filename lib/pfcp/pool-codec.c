/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "ogs-pfcp.h"

static uint16_t read_u16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

int ogs_pfcp_build_pool_identity(ogs_tlv_octet_t *octet,
        const void *identity, size_t length, void *buffer, size_t capacity)
{
    uint8_t *p = buffer;

    if (!octet || !identity || !buffer || !length || length > UINT16_MAX-2 ||
            capacity < length+2)
        return OGS_ERROR;
    p[0] = length >> 8;
    p[1] = length & 0xff;
    memcpy(p+2, identity, length);
    octet->presence = 1;
    octet->data = buffer;
    octet->len = length+2;
    return OGS_OK;
}

int ogs_pfcp_parse_pool_identity(const ogs_tlv_octet_t *octet,
        const uint8_t **identity, size_t *length)
{
    const uint8_t *p;
    size_t n;

    if (!octet || !identity || !length || !octet->data || octet->len < 3)
        return OGS_ERROR;
    p = octet->data;
    n = read_u16(p);
    /* R19 8.1.2 / 8.2.128: the internal length delimits the identity;
     * unknown trailing extension octets do not belong to that identity. */
    if (!n || n > octet->len-2)
        return OGS_ERROR;
    *identity = p+2;
    *length = n;
    return OGS_OK;
}

static int label_alnum(uint8_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9');
}

int ogs_pfcp_parse_pool_network_instance(char *dnn, size_t capacity,
        const ogs_tlv_octet_t *octet)
{
    const uint8_t *p;
    size_t pos = 0, written = 0, n, i;

    if (!octet || !octet->data || !dnn || !capacity || !octet->len ||
            octet->len > OGS_MAX_DNN_LEN)
        return OGS_ERROR;
    p = octet->data;
    /* Validate completely before writing a string or calling legacy helpers. */
    while (pos < octet->len) {
        n = p[pos++];
        if (!n || n > 63 || n > octet->len-pos ||
                !label_alnum(p[pos]) || !label_alnum(p[pos+n-1]))
            return OGS_ERROR;
        for (i = 0; i < n; i++)
            if (!label_alnum(p[pos+i]) && p[pos+i] != '-')
                return OGS_ERROR;
        written += n + (pos > 1 ? 1 : 0);
        pos += n;
    }
    if (written >= capacity)
        return OGS_ERROR;
    pos = written = 0;
    while (pos < octet->len) {
        n = p[pos++];
        if (written) dnn[written++] = '.';
        memcpy(dnn+written, p+pos, n);
        written += n;
        pos += n;
    }
    dnn[written] = 0;
    return OGS_OK;
}

/* Walk only length-delimited spans. Semantic defects do not stop framing checks,
 * even after a capacity excess or an earlier invalid optional child. */
static int scan_group(const uint8_t *p, size_t len, int *valid,
        int *unsupported, uint16_t *offending)
{
    unsigned int identities = 0, slices = 0;
    int seen_ni = 0, seen_ip = 0;
    uint16_t type, n;
    ogs_tlv_octet_t octet;
    const uint8_t *identity;
    size_t identity_len;
    char dnn[OGS_MAX_DNN_LEN+1];

    *valid = 1;
    while (len) {
        if (len < 4) {
            *offending = OGS_PFCP_UE_IP_ADDRESS_POOL_INFORMATION_TYPE;
            return OGS_ERROR;
        }
        type = read_u16(p);
        n = read_u16(p+2);
        if (n > len-4) {
            *offending = type;
            return OGS_ERROR;
        }
        memset(&octet, 0, sizeof(octet));
        octet.data = (void *)(p+4);
        octet.len = n;
        switch (type) {
        case OGS_PFCP_UE_IP_ADDRESS_POOL_IDENTITY_TYPE:
            identities++;
            if (identities > OGS_PFCP_MAX_POOL_IDENTITIES)
                *unsupported = 1;
            if (ogs_pfcp_parse_pool_identity(&octet,
                        &identity, &identity_len) != OGS_OK)
                *valid = 0;
            else if (identity_len > OGS_PFCP_MAX_POOL_ID_LEN)
                *unsupported = 1;
            break;
        case OGS_PFCP_NETWORK_INSTANCE_TYPE:
            if (!seen_ni++ && ogs_pfcp_parse_pool_network_instance(
                        dnn, sizeof(dnn), &octet) != OGS_OK)
                *valid = 0;
            break;
        case OGS_PFCP_S_NSSAI_TYPE:
            if (++slices > OGS_PFCP_MAX_POOL_SLICES)
                *unsupported = 1;
            if (n != 4) *valid = 0;
            break;
        case OGS_PFCP_IP_VERSION_TYPE:
            if (!seen_ip++ && (!n || !(p[4] & 3)))
                *valid = 0;
            break;
        default:
            /* Unknown/unexpected optional children do not erase known data. */
            break;
        }
        p += 4+n;
        len -= 4+n;
    }
    if (!identities) *valid = 0;
    return OGS_OK;
}

int ogs_pfcp_scan_pool_information(const void *data, size_t len,
        ogs_pfcp_pool_decode_result_t *result)
{
    const uint8_t *p = data;
    uint16_t type, n;
    int valid, unsupported = 0;

    if (!result || (!data && len)) return OGS_ERROR;
    memset(result, 0, sizeof(*result));
    while (len) {
        if (len < 4) {
            result->offending_ie = len >= 2 ? read_u16(p) : 0;
            goto framing_error;
        }
        type = read_u16(p);
        n = read_u16(p+2);
        result->offending_ie = type;
        if (n > len-4) goto framing_error;
        if (type == OGS_PFCP_UE_IP_ADDRESS_POOL_INFORMATION_TYPE) {
            result->groups_seen++;
            if (result->groups_seen > OGS_PFCP_MAX_POOL_INFORMATION)
                unsupported = 1;
            if (scan_group(p+4, n, &valid, &unsupported,
                        &result->offending_ie) != OGS_OK)
                goto framing_error;
            if (!valid) result->groups_invalid++;
        }
        p += 4+n;
        len -= 4+n;
    }
    result->offending_ie = 0;
    if (!result->groups_seen)
        result->status = OGS_PFCP_POOL_ABSENT;
    else if (unsupported)
        result->status = OGS_PFCP_POOL_UNSUPPORTED;
    else {
        result->groups_decoded = result->groups_seen-result->groups_invalid;
        result->status = !result->groups_invalid ? OGS_PFCP_POOL_VALID :
            result->groups_decoded ? OGS_PFCP_POOL_DEGRADED : OGS_PFCP_POOL_UNUSABLE;
    }
    return OGS_OK;

framing_error:
    result->status = OGS_PFCP_POOL_FRAMING_ERROR;
    result->groups_decoded = 0;
    return OGS_ERROR;
}

static ogs_tlv_desc_t *find_type(ogs_tlv_desc_t *desc, uint16_t type,
        unsigned *index)
{
    unsigned int i;
    for (i = 0; desc->child_descs[i]; i++) {
        ogs_tlv_desc_t *child = desc->child_descs[i];
        if (child->ctype != OGS_TLV_MORE && child->type == type)
            { if (index) *index = i; return child; }
    }
    return NULL;
}

int ogs_pfcp_check_tlv_framing(ogs_tlv_desc_t *desc,
        const void *data, size_t len)
{
    const uint8_t *p = data;
    while (len) {
        unsigned n;
        ogs_tlv_desc_t *child;
        if (len < 4) return OGS_ERROR;
        n = read_u16(p+2);
        if (n > len-4) return OGS_ERROR;
        child = find_type(desc, read_u16(p), NULL);
        if (child && child->ctype == OGS_TLV_COMPOUND &&
                ogs_pfcp_check_tlv_framing(child, p+4, n) != OGS_OK)
            return OGS_ERROR;
        p += n+4; len -= n+4;
    }
    return OGS_OK;
}

/* Optional scalar defects must not make the enclosing association fail.
 * Filter unknown children recursively before generic descriptor bookkeeping.
 * The first singleton wins even if unusable; later copies cannot repair it. */
static void copy_known_tlvs(ogs_pkbuf_t *out, ogs_tlv_desc_t *desc,
        const uint8_t *p, size_t len)
{
    bool seen[OGS_TLV_MAX_CHILD_DESC] = {false};
    while (len) {
        unsigned type = read_u16(p), n = read_u16(p+2), i = 0;
        ogs_tlv_desc_t *child = find_type(desc, type, &i);
        bool keep = child != NULL;
        unsigned copied_length = n;
        if (child) {
            bool first = !seen[i];
            ogs_tlv_desc_t *next = desc->child_descs[i+1];
            bool repeated = next && next->ctype == OGS_TLV_MORE;
            if (seen[i] && !repeated) keep = false;
            seen[i] = true;
            if (child->ctype == OGS_TLV_UINT8 && n != 1) keep = false;
            if (child->ctype == OGS_TLV_UINT16 && n != 2) keep = false;
            if (child->ctype == OGS_TLV_UINT24 && n != 3) keep = false;
            if (child->ctype == OGS_TLV_UINT32 && n != 4) keep = false;
            if (child->ctype == OGS_TLV_FIXED_STR && n != child->length) keep = false;
            if (type == OGS_PFCP_UP_FUNCTION_FEATURES_TYPE && !n) keep = false;
            /* CP Function Features has grown since the legacy uint8 descriptor.
             * Consume the supported prefix and retain forward compatibility. */
            if (type == OGS_PFCP_CP_FUNCTION_FEATURES_TYPE && n &&
                    child->ctype == OGS_TLV_UINT8) {
                keep = first;
                copied_length = 1;
            }
            /* Recovery Time Stamp is extendable (R19 8.1.2 / 8.2.65),
             * although the legacy numeric descriptor stores only its prefix. */
            if (type == OGS_PFCP_RECOVERY_TIME_STAMP_TYPE && n >= 4 &&
                    child->ctype == OGS_TLV_UINT32) {
                keep = first;
                copied_length = 4;
            }
        }
        if (keep) {
            uint8_t *h = ogs_pkbuf_put(out, 4);
            size_t start = out->len;
            memcpy(h, p, 4);
            if (child->ctype == OGS_TLV_COMPOUND)
                copy_known_tlvs(out, child, p+4, n);
            else memcpy(ogs_pkbuf_put(out, copied_length), p+4, copied_length);
            h[2] = (out->len-start)>>8; h[3] = out->len-start;
        }
        p += n+4; len -= n+4;
    }
}

static void copy_group(ogs_pkbuf_t *filtered, const uint8_t *p, size_t len)
{
    uint8_t *header = ogs_pkbuf_put(filtered, 4);
    uint16_t type, n;
    size_t copied = 0;
    int seen_ni = 0, seen_ip = 0, keep;

    header[0] = 0; header[1] = 233;
    while (len) {
        type = read_u16(p);
        n = read_u16(p+2);
        keep = type == OGS_PFCP_UE_IP_ADDRESS_POOL_IDENTITY_TYPE ||
            type == OGS_PFCP_S_NSSAI_TYPE;
        if (type == OGS_PFCP_NETWORK_INSTANCE_TYPE) keep = !seen_ni++;
        if (type == OGS_PFCP_IP_VERSION_TYPE) keep = !seen_ip++;
        if (keep) {
            memcpy(ogs_pkbuf_put(filtered, n+4), p, n+4);
            copied += n+4;
        }
        p += n+4;
        len -= n+4;
    }
    header[2] = copied >> 8; header[3] = copied;
}

int ogs_pfcp_parse_pool_tlvs(void *msg, ogs_tlv_desc_t *desc,
        ogs_pkbuf_t *body, ogs_pfcp_pool_decode_result_t *result,
        ogs_pkbuf_t **storage)
{
    const uint8_t *p;
    size_t remaining;
    uint16_t type, n, offending;
    int valid, unsupported;
    bool retention_seen = false;
    ogs_pkbuf_t *filtered;

    if (!msg || !desc || !body || !result || !storage || *storage)
        return OGS_ERROR;
    if (ogs_pfcp_scan_pool_information(body->data, body->len, result) != OGS_OK)
        return OGS_ERROR;
    if (ogs_pfcp_check_tlv_framing(desc, body->data, body->len) != OGS_OK)
        return OGS_ERROR;
    if (result->status == OGS_PFCP_POOL_ABSENT &&
            desc != &ogs_pfcp_msg_desc_pfcp_association_setup_request &&
            desc != &ogs_pfcp_msg_desc_pfcp_association_setup_response)
        return ogs_tlv_parse_msg(msg, desc, body, OGS_TLV_MODE_T2_L2);

    /* Keep filtered storage until message_free: TLV octets borrow these bytes.
     * Never modify the caller's packet, and never expose a truncated pool list.
     * No shared GTP/core TLV behavior is changed by this PFCP adapter. */
    filtered = ogs_pkbuf_alloc(NULL, body->len);
    if (!filtered) return OGS_ERROR;
    *storage = filtered;
    p = body->data;
    remaining = body->len;
    while (remaining) {
        type = read_u16(p);
        n = read_u16(p+2);
        valid = 1;
        unsupported = 0;
        if (desc == &ogs_pfcp_msg_desc_pfcp_association_setup_request &&
                type == OGS_PFCP_PFCP_SESSION_RETENTION_INFORMATION_WITHIN_PFCP_ASSOCIATION_SETUP_REQUEST_TYPE) {
            if (retention_seen) {
                valid = 0; /* First singleton governs, including empty. */
            } else {
                retention_seen = true;
                if (!n) {
                    ogs_pfcp_association_setup_request_t *req = msg;
                    /* R19 table 7.4.4.1-2 permits no child addresses.
                     * Generic compound decoding requires a child; preserve
                     * presence separately rather than fabricating one. */
                    req->pfcp_session_retention_information.presence = 1;
                }
                /* This recovery profile requests/supports all-session
                 * retention only. Selective/unknown children are unsupported:
                 * ignore the optional group without turning filtered-empty
                 * content into an all-session request or rejecting Setup. */
                valid = 0;
            }
        }
        if (type == OGS_PFCP_UE_IP_ADDRESS_POOL_INFORMATION_TYPE) {
            if (result->status == OGS_PFCP_POOL_UNSUPPORTED)
                valid = 0;
            else if (scan_group(p+4, n, &valid, &unsupported,
                        &offending) != OGS_OK)
                return OGS_ERROR; /* Already checked; defensive only. */
        }
        /* Discard unknown children before the shared parser's descriptor-count
         * bookkeeping; many distinct unknown types must not consume its limit. */
        if (valid && type == OGS_PFCP_UE_IP_ADDRESS_POOL_INFORMATION_TYPE)
            copy_group(filtered, p+4, n);
        else if (valid && find_type(desc, type, NULL))
            memcpy(ogs_pkbuf_put(filtered, n+4), p, n+4);
        p += n+4;
        remaining -= n+4;
    }
    if (!filtered->len) return OGS_OK;
    /* Apply scalar/unknown filtering to the complete body, preserving the
     * special pool filtering above and all permitted repeated pool children. */
    {
        ogs_pkbuf_t *clean = ogs_pkbuf_alloc(NULL, filtered->len);
        if (!clean) return OGS_ERROR;
        copy_known_tlvs(clean, desc, filtered->data, filtered->len);
        ogs_pkbuf_free(filtered);
        *storage = clean;
        if (!clean->len) return OGS_OK;
        return ogs_tlv_parse_msg(msg, desc, clean, OGS_TLV_MODE_T2_L2);
    }
}
