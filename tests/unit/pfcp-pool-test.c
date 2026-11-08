/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "ogs-pfcp.h"
#include "core/abts.h"
#include "pfcp-pool-fixtures.h"

/* Independent TLV assembler: literal wire types, never the encoder under test. */
static size_t put_ie(uint8_t *p, unsigned type, const void *value, size_t len)
{
    p[0] = type >> 8; p[1] = type;
    p[2] = len >> 8; p[3] = len;
    if (len) memcpy(p+4, value, len);
    return len+4;
}

static ogs_pfcp_message_t *parse_wire(const uint8_t *wire, size_t len)
{
    ogs_pfcp_message_t *msg;
    ogs_pkbuf_t *packet = ogs_pkbuf_alloc(NULL, ogs_max(len, 1));
    memcpy(ogs_pkbuf_put(packet, len), wire, len);
    msg = ogs_pfcp_parse_msg(packet);
    /* Association fields must remain valid in message-owned filtered storage. */
    ogs_pkbuf_free(packet);
    return msg;
}

static ogs_pkbuf_t *body_packet(unsigned type, const void *body, size_t len)
{
    uint8_t wire[65539];
    ogs_pkbuf_t *packet;
    size_t header_len = type >= 50 ? 16 : 8;
    ogs_assert(len+header_len <= sizeof(wire));
    memset(wire, 0, header_len);
    wire[0] = type >= 50 ? 0x21 : 0x20; wire[1] = type;
    wire[2] = (len+header_len-4) >> 8; wire[3] = len+header_len-4;
    memcpy(wire+header_len, body, len);
    packet = ogs_pkbuf_alloc(NULL, len+header_len);
    memcpy(ogs_pkbuf_put(packet, len+header_len), wire, len+header_len);
    return packet;
}

static ogs_pfcp_message_t *parse_body(unsigned type, const void *body, size_t len)
{
    ogs_pkbuf_t *packet = body_packet(type, body, len);
    ogs_pfcp_message_t *msg = ogs_pfcp_parse_msg(packet);
    ogs_pkbuf_free(packet);
    return msg;
}

static void check_body(abts_case *tc, const uint8_t *body, size_t len,
        ogs_pfcp_pool_decode_status_t status, unsigned groups, unsigned invalid)
{
    unsigned type;
    ogs_pfcp_pool_decode_result_t result;
    ogs_pfcp_message_t *msg;
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_scan_pool_information(body, len, &result));
    ABTS_INT_EQUAL(tc, status, result.status);
    ABTS_INT_EQUAL(tc, groups, result.groups_decoded);
    ABTS_INT_EQUAL(tc, invalid, result.groups_invalid);
    for (type = 5; type <= 7; type++) {
        msg = parse_body(type, body, len);
        ABTS_PTR_NOTNULL(tc, msg);
        if (!msg) continue;
        ABTS_INT_EQUAL(tc, status, msg->pool_information.status);
        ABTS_INT_EQUAL(tc, groups, msg->pool_information.groups_decoded);
        if (!groups) {
            ogs_pfcp_tlv_ue_ip_address_pool_information_t *pool = type == 5 ?
                msg->pfcp_association_setup_request.ue_ip_address_pool_information :
                type == 6 ? msg->pfcp_association_setup_response.ue_ip_address_pool_information :
                msg->pfcp_association_update_request.ue_ip_address_pool_information;
            ABTS_INT_EQUAL(tc, 0, pool[0].presence);
        }
        ogs_pfcp_message_free(msg);
    }
}

static void test_fixtures(abts_case *tc, void *data)
{
    const uint8_t *fixtures[] = {pool_request_fixture, pool_response_fixture};
    const size_t lengths[] = {sizeof(pool_request_fixture), sizeof(pool_response_fixture)};
    unsigned i;
    size_t n;
    const uint8_t *identity;
    char dnn[101];
    ogs_pfcp_message_t *msg;
    ogs_pkbuf_t *encoded;
    ogs_pfcp_tlv_ue_ip_address_pool_information_t *pools;
    ogs_tlv_octet_t *features;

    for (i = 0; i < OGS_ARRAY_SIZE(fixtures); i++) {
        msg = parse_wire(fixtures[i], lengths[i]);
        ABTS_PTR_NOTNULL(tc, msg);
        if (!msg) continue;
        ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_VALID, msg->pool_information.status);
        ABTS_INT_EQUAL(tc, 2, msg->pool_information.groups_seen);
        pools = i == 0 ? msg->pfcp_association_setup_request.ue_ip_address_pool_information :
            msg->pfcp_association_setup_response.ue_ip_address_pool_information;
        features = i == 0 ? &msg->pfcp_association_setup_request.up_function_features :
            &msg->pfcp_association_setup_response.up_function_features;
        ABTS_INT_EQUAL(tc, 12, features->len);
        ABTS_INT_EQUAL(tc, 0x80, ((uint8_t *)features->data)[11]);
        ABTS_INT_EQUAL(tc, 1, pools[0].presence);
        ABTS_INT_EQUAL(tc, 1, pools[1].presence);
        ABTS_INT_EQUAL(tc, 0, pools[2].presence);
        ABTS_INT_EQUAL(tc, 1, pools[0].s_nssai[1].presence);
        ABTS_INT_EQUAL(tc, 0, pools[0].s_nssai[2].presence);
        ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_identity(
                    &pools[0].ue_ip_address_pool_identity[0], &identity, &n));
        ABTS_SIZE_EQUAL(tc, 10, n);
        ABTS_TRUE(tc, memcmp(identity, "edge1-v4-a", n) == 0);
        ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_identity(
                    &pools[0].ue_ip_address_pool_identity[1], &identity, &n));
        ABTS_SIZE_EQUAL(tc, 3, n);
        ABTS_TRUE(tc, memcmp(identity, "A\0B", n) == 0);
        ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_network_instance(
                    dnn, sizeof(dnn), &pools[1].network_instance));
        ABTS_STR_EQUAL(tc, "internet", dnn);
        encoded = ogs_pfcp_build_msg(msg); /* Returns TLV body, as before. */
        ABTS_PTR_NOTNULL(tc, encoded);
        if (encoded) {
            ABTS_SIZE_EQUAL(tc, lengths[i]-8, encoded->len);
            ABTS_TRUE(tc, encoded->len == lengths[i]-8 &&
                    memcmp(encoded->data, fixtures[i]+8, encoded->len) == 0);
            ogs_pkbuf_free(encoded);
        }
        ogs_pfcp_message_free(msg);
    }
}

static void test_identity_helpers(abts_case *tc, void *data)
{
    const uint8_t expected[] = {0, 3, 'A', 0, 'B'};
    uint8_t buffer[70], long_id[65];
    ogs_tlv_octet_t octet;
    const uint8_t *identity;
    size_t n;
    memset(&octet, 0, sizeof(octet));
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_build_pool_identity(
                &octet, "A\0B", 3, buffer, sizeof(buffer)));
    ABTS_SIZE_EQUAL(tc, sizeof(expected), octet.len);
    ABTS_TRUE(tc, memcmp(buffer, expected, sizeof(expected)) == 0);
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_identity(&octet, &identity, &n));
    ABTS_SIZE_EQUAL(tc, 3, n);
    ABTS_PTR_EQUAL(tc, buffer+2, identity);
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_build_pool_identity(&octet, "a", 1, buffer, 2));
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_build_pool_identity(&octet, "a", 0, buffer, 2));
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_build_pool_identity(&octet, "a", 65534, buffer, sizeof(buffer)));
    memset(long_id, 0x80, sizeof(long_id));
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_build_pool_identity(
                &octet, long_id, 65, buffer, sizeof(buffer)));
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_identity(&octet, &identity, &n));
    ABTS_SIZE_EQUAL(tc, 65, n); /* Wire helper, distinct from local profile. */
    buffer[1]--;
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_identity(&octet, &identity, &n));
    ABTS_SIZE_EQUAL(tc, 64, n); /* Remaining octet is an unknown extension. */
    buffer[1] = 66;
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_parse_pool_identity(&octet, &identity, &n));
    {
        uint8_t *large = ogs_malloc(65535), *large_id = ogs_malloc(65533);
        memset(large_id, 0x80, 65533);
        ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_build_pool_identity(
                    &octet, large_id, 65533, large, 65535));
        ABTS_INT_EQUAL(tc, 0xff, large[0]);
        ABTS_INT_EQUAL(tc, 0xfd, large[1]);
        ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_identity(&octet, &identity, &n));
        ABTS_SIZE_EQUAL(tc, 65533, n);
        ogs_free(large_id); ogs_free(large);
    }
}

static void test_optional_semantics(abts_case *tc, void *data)
{
    const uint8_t id[] = {0, 1, 'x'};
    const uint8_t ni[] = {5, 'e', 'd', 'g', 'e', '1'};
    const uint8_t bad_ni[] = {5, 'e', 0, 'g', 'e', '1'};
    const uint8_t slice[] = {1, 0xff, 0xff, 0xff, 0xff};
    const uint8_t ip[] = {1, 2, 3, 0x83, 0};
    uint8_t group[256], body[1024];
    size_t n, b;
    unsigned i;

    /* Missing mandatory child of optional parent is not an association error. */
    n = put_ie(group, 22, ni, sizeof(ni));
    b = put_ie(body, 233, group, n);
    check_body(tc, body, b, OGS_PFCP_POOL_UNUSABLE, 0, 1);
    b = put_ie(body, 233, NULL, 0);
    check_body(tc, body, b, OGS_PFCP_POOL_UNUSABLE, 0, 1);
    n = put_ie(group, 177, id, sizeof(id));
    b = put_ie(body, 233, group, n);
    check_body(tc, body, b, OGS_PFCP_POOL_VALID, 1, 0); /* Optional NI/family/slice. */
    b += put_ie(body+b, 233, NULL, 0);
    check_body(tc, body, b, OGS_PFCP_POOL_DEGRADED, 1, 1);
    for (i = 0; i < OGS_ARRAY_SIZE(ip); i++) {
        n = put_ie(group, 177, id, sizeof(id));
        n += put_ie(group+n, 258, &ip[i], 1);
        b = put_ie(body, 233, group, n);
        check_body(tc, body, b, i == 4 ? OGS_PFCP_POOL_UNUSABLE : OGS_PFCP_POOL_VALID,
                i == 4 ? 0 : 1, i == 4 ? 1 : 0);
    }
    n = put_ie(group, 177, id, sizeof(id));
    n += put_ie(group+n, 22, ni, sizeof(ni));
    n += put_ie(group+n, 22, bad_ni, sizeof(bad_ni));
    n += put_ie(group+n, 258, ip, 1);
    n += put_ie(group+n, 258, &ip[4], 1);
    n += put_ie(group+n, 65280, id, sizeof(id));
    b = put_ie(body, 233, group, n);
    b += put_ie(body+b, 65000, id, sizeof(id));
    check_body(tc, body, b, OGS_PFCP_POOL_VALID, 1, 0); /* First singleton wins. */
    {
        ogs_pfcp_message_t *msg = parse_body(5, body, b);
        char dnn[101];
        ABTS_PTR_NOTNULL(tc, msg);
        if (msg) {
            ogs_pfcp_tlv_ue_ip_address_pool_information_t *pool =
                msg->pfcp_association_setup_request.ue_ip_address_pool_information;
            ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_network_instance(
                        dnn, sizeof(dnn), &pool[0].network_instance));
            ABTS_STR_EQUAL(tc, "edge1", dnn);
            ABTS_INT_EQUAL(tc, 1, ((uint8_t *)pool[0].ip_version.data)[0]);
            ogs_pfcp_message_free(msg);
        }
    }
    n = put_ie(group, 177, id, sizeof(id));
    n += put_ie(group+n, 22, bad_ni, sizeof(bad_ni));
    n += put_ie(group+n, 22, ni, sizeof(ni));
    b = put_ie(body, 233, group, n);
    check_body(tc, body, b, OGS_PFCP_POOL_UNUSABLE, 0, 1); /* Later cannot repair first. */
    for (i = 0; i <= 5; i++) {
        n = put_ie(group, 177, id, sizeof(id));
        n += put_ie(group+n, 257, slice, i);
        b = put_ie(body, 233, group, n);
        check_body(tc, body, b, i == 4 ? OGS_PFCP_POOL_VALID : OGS_PFCP_POOL_UNUSABLE,
                i == 4 ? 1 : 0, i == 4 ? 0 : 1);
    }
    n = put_ie(group, 177, id, 2); /* Internal length says 1, no identity byte. */
    b = put_ie(body, 233, group, n);
    check_body(tc, body, b, OGS_PFCP_POOL_UNUSABLE, 0, 1);
    n = put_ie(group, 177, "\0\0", 2);
    b = put_ie(body, 233, group, n);
    check_body(tc, body, b, OGS_PFCP_POOL_UNUSABLE, 0, 1);
    b = put_ie(body, 60, "\0\x7f\0\0\x08", 5);
    check_body(tc, body, b, OGS_PFCP_POOL_ABSENT, 0, 0);
}

static void test_extensions(abts_case *tc, void *data)
{
    /* R19 8.1.2: 177 and 258 are extendable; 257 is fixed length.
     * Literal independent wire bytes, including unknown trailing octets. */
    const uint8_t id[] = {0, 1, 'x', 0xfe, 0xed};
    const uint8_t ip[] = {0x83, 0xfe, 0xed};
    uint8_t group[64], body[128];
    size_t n = put_ie(group, 177, id, sizeof(id));
    size_t b;
    ogs_tlv_octet_t octet = {0};
    const uint8_t *identity = NULL;
    size_t length = 0;
    octet.data = (void *)id; octet.len = sizeof(id);
    ABTS_INT_EQUAL(tc, OGS_OK,
            ogs_pfcp_parse_pool_identity(&octet, &identity, &length));
    ABTS_SIZE_EQUAL(tc, 1, length);
    ABTS_PTR_EQUAL(tc, id+2, identity);
    n += put_ie(group+n, 258, ip, sizeof(ip));
    b = put_ie(body, 233, group, n);
    check_body(tc, body, b, OGS_PFCP_POOL_VALID, 1, 0);
    n += put_ie(group+n, 257, "\1\0\0\1\xff", 5);
    b = put_ie(body, 233, group, n);
    check_body(tc, body, b, OGS_PFCP_POOL_UNUSABLE, 0, 1);
}

static void test_unknowns(abts_case *tc, void *data)
{
    uint8_t group[2048], body[4096];
    size_t n = put_ie(group, 177, "\0\1x", 3), b;
    unsigned int i;
    for (i = 0; i < 300; i++)
        n += put_ie(group+n, 1000+i, NULL, 0);
    b = put_ie(body, 233, group, n);
    for (i = 0; i < 300; i++)
        b += put_ie(body+b, 2000+i, NULL, 0);
    check_body(tc, body, b, OGS_PFCP_POOL_VALID, 1, 0);
}

static void test_capacities(abts_case *tc, void *data)
{
    const uint8_t id[] = {0, 1, 'x'};
    const uint8_t slice[] = {1, 0xff, 0xff, 0xff};
    uint8_t group[2048], body[32768], identity[67];
    unsigned i, count, dimension;
    size_t n, b;
    for (dimension = 0; dimension < 3; dimension++) {
        unsigned maximum = dimension == 2 ? 8 : 16;
        for (count = maximum; count <= maximum+1; count++) {
            n = b = 0;
            if (dimension != 1) n = put_ie(group, 177, id, sizeof(id));
            if (dimension == 1)
                for (i = 0; i < count; i++) n += put_ie(group+n, 177, id, sizeof(id));
            if (dimension == 2)
                for (i = 0; i < count; i++) n += put_ie(group+n, 257, slice, sizeof(slice));
            for (i = 0; i < (dimension == 0 ? count : 1); i++)
                b += put_ie(body+b, 233, group, n);
            check_body(tc, body, b, count > maximum ? OGS_PFCP_POOL_UNSUPPORTED : OGS_PFCP_POOL_VALID,
                    count > maximum ? 0 : dimension == 0 ? count : 1, 0);
            if (count == maximum) {
                ogs_pfcp_message_t *msg = parse_body(5, body, b);
                ABTS_PTR_NOTNULL(tc, msg);
                if (msg) {
                    ogs_pfcp_tlv_ue_ip_address_pool_information_t *pool =
                        msg->pfcp_association_setup_request.ue_ip_address_pool_information;
                    ABTS_INT_EQUAL(tc, 1, dimension == 0 ? pool[15].presence : dimension == 1 ?
                            pool[0].ue_ip_address_pool_identity[15].presence : pool[0].s_nssai[7].presence);
                    ogs_pfcp_message_free(msg);
                }
            }
        }
    }
    memset(identity, 0x80, sizeof(identity)); identity[0] = 0;
    for (count = 64; count <= 65; count++) {
        identity[1] = count;
        n = put_ie(group, 177, identity, count+2);
        b = put_ie(body, 233, group, n);
        check_body(tc, body, b, count == 64 ? OGS_PFCP_POOL_VALID : OGS_PFCP_POOL_UNSUPPORTED,
                count == 64 ? 1 : 0, 0);
    }
    /* Exercise all local maxima simultaneously, including full opaque IDs. */
    identity[1] = 64; n = b = 0;
    for (i = 0; i < 16; i++) n += put_ie(group+n, 177, identity, 66);
    {
        uint8_t ni[100];
        memset(ni, 'a', sizeof(ni)); ni[0] = 63; ni[64] = 35;
        n += put_ie(group+n, 22, ni, sizeof(ni));
    }
    for (i = 0; i < 8; i++) n += put_ie(group+n, 257, slice, sizeof(slice));
    n += put_ie(group+n, 258, "\3", 1);
    for (i = 0; i < 16; i++) b += put_ie(body+b, 233, group, n);
    check_body(tc, body, b, OGS_PFCP_POOL_VALID, 16, 0);
    {
        ogs_pfcp_message_t *msg = parse_body(5, body, b);
        ABTS_PTR_NOTNULL(tc, msg);
        if (msg) {
            ogs_pkbuf_t *encoded = ogs_pfcp_build_msg(msg);
            ABTS_INT_EQUAL(tc, 1, msg->pfcp_association_setup_request.
                    ue_ip_address_pool_information[15].ue_ip_address_pool_identity[15].presence);
            ABTS_INT_EQUAL(tc, 1, msg->pfcp_association_setup_request.
                    ue_ip_address_pool_information[15].s_nssai[7].presence);
            ABTS_PTR_NOTNULL(tc, encoded);
            if (encoded) {
                ABTS_SIZE_EQUAL(tc, b, encoded->len);
                ABTS_TRUE(tc, encoded->len == b && memcmp(encoded->data, body, b) == 0);
                ogs_pkbuf_free(encoded);
            }
            ogs_pfcp_message_free(msg);
        }
    }
    /* Capacity excess cannot conceal a later structural truncation. */
    n = put_ie(group, 177, id, sizeof(id)); b = 0;
    for (i = 0; i < 17; i++) b += put_ie(body+b, 233, group, n);
    memset(body+b, 0, 3);
    {
        ogs_pfcp_pool_decode_result_t result;
        ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_scan_pool_information(body, b+3, &result));
        ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_FRAMING_ERROR, result.status);
        ABTS_INT_EQUAL(tc, 0, result.offending_ie);
    }
}

static void test_framing(abts_case *tc, void *data)
{
    uint8_t body[] = {0, 233, 0, 7, 0, 177, 0, 4, 0, 1, 'x'};
    const uint8_t short_seid_header[] = {0x21, 50, 0, 4, 0, 0, 0, 0};
    ogs_pfcp_pool_decode_result_t result;
    ogs_pfcp_message_t *msg;
    unsigned i;
    ABTS_PTR_EQUAL(tc, NULL, parse_wire(short_seid_header, sizeof(short_seid_header)));
    for (i = 0; i < sizeof(pool_request_fixture); i++) {
        msg = parse_wire(pool_request_fixture, i);
        ABTS_PTR_EQUAL(tc, NULL, msg);
        if (msg) ogs_pfcp_message_free(msg);
    }
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_scan_pool_information(body, sizeof(body), &result));
    ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_FRAMING_ERROR, result.status);
    ABTS_INT_EQUAL(tc, 177, result.offending_ie);
    for (i = 5; i <= 7; i++)
        ABTS_PTR_EQUAL(tc, NULL, parse_body(i, body, sizeof(body)));
    body[7] = 3;
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_scan_pool_information(body, sizeof(body), &result));
    /* Malformed tail after an otherwise valid group. */
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_scan_pool_information(body, sizeof(body)-1, &result));
    body[3] = 1;
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_scan_pool_information(body, sizeof(body), &result));
    ABTS_INT_EQUAL(tc, OGS_PFCP_POOL_FRAMING_ERROR, result.status);
}

static void test_network_instance(abts_case *tc, void *data)
{
    uint8_t ni[101];
    char dnn[101], small[4] = "xxx";
    ogs_tlv_octet_t octet;
    memset(&octet, 0, sizeof(octet));
    octet.data = "\x05" "Edge1" "\x06" "MNC001";
    octet.len = 13;
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_network_instance(dnn, sizeof(dnn), &octet));
    ABTS_STR_EQUAL(tc, "Edge1.MNC001", dnn); /* No guessed OI stripping/case loss. */
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_parse_pool_network_instance(small, sizeof(small), &octet));
    ABTS_STR_EQUAL(tc, "xxx", small);
    memset(ni, 'a', sizeof(ni)); ni[0] = 63; ni[64] = 35;
    octet.data = ni; octet.len = 100;
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pfcp_parse_pool_network_instance(dnn, sizeof(dnn), &octet));
    octet.len = 101;
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_parse_pool_network_instance(dnn, sizeof(dnn), &octet));
    octet.len = 100; ni[0] = 64;
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_parse_pool_network_instance(dnn, sizeof(dnn), &octet));
    ni[0] = 0;
    ABTS_INT_EQUAL(tc, OGS_ERROR, ogs_pfcp_parse_pool_network_instance(dnn, sizeof(dnn), &octet));
}

/* The same normalization bug affected Create PDR and Usage Information. Their
 * identity multiplicity is a pair for Create PDR, singleton for usage. */
static void test_alias_contexts(abts_case *tc, void *data)
{
    uint8_t identities[64], body[128];
    size_t n, b;
    ogs_pfcp_message_t *msg;
    ogs_pkbuf_t *packet;
    n = put_ie(identities, 177, "\0\1a", 3);
    n += put_ie(identities+n, 177, "\0\1b", 3);
    n += put_ie(identities+n, 177, "\0\1c", 3); /* Only specified pair is handled. */
    b = put_ie(body, 1, identities, n);
    packet = body_packet(50, body, b);
    msg = ogs_pfcp_parse_msg(packet);
    ABTS_PTR_NOTNULL(tc, msg);
    if (msg) {
        ABTS_INT_EQUAL(tc, 1, msg->pfcp_session_establishment_request.create_pdr[0].ue_ip_address_pool_identity[1].presence);
        ABTS_INT_EQUAL(tc, 'b', ((uint8_t *)msg->pfcp_session_establishment_request.create_pdr[0].ue_ip_address_pool_identity[1].data)[2]);
        ogs_pfcp_message_free(msg);
    }
    ogs_pkbuf_free(packet);
    b = put_ie(body, 267, identities, n);
    packet = body_packet(8, body, b);
    msg = ogs_pfcp_parse_msg(packet);
    ABTS_PTR_NOTNULL(tc, msg);
    if (msg) {
        ABTS_INT_EQUAL(tc, 1, msg->pfcp_association_update_response.ue_ip_address_usage_information.ue_ip_address_pool_id.presence);
        ABTS_INT_EQUAL(tc, 'a', ((uint8_t *)msg->pfcp_association_update_response.ue_ip_address_usage_information.ue_ip_address_pool_id.data)[2]);
        ogs_pfcp_message_free(msg);
    }
    ogs_pkbuf_free(packet);
}

abts_suite *test_pfcp_pool(abts_suite *suite)
{
    suite = ADD_SUITE(suite);
    abts_run_test(suite, test_fixtures, NULL);
    abts_run_test(suite, test_identity_helpers, NULL);
    abts_run_test(suite, test_optional_semantics, NULL);
    abts_run_test(suite, test_extensions, NULL);
    abts_run_test(suite, test_unknowns, NULL);
    abts_run_test(suite, test_capacities, NULL);
    abts_run_test(suite, test_framing, NULL);
    abts_run_test(suite, test_network_instance, NULL);
    abts_run_test(suite, test_alias_contexts, NULL);
    return suite;
}
