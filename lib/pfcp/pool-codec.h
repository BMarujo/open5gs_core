/* SPDX-License-Identifier: AGPL-3.0-or-later */
#if !defined(OGS_PFCP_INSIDE) && !defined(OGS_PFCP_COMPILATION)
#error "This header cannot be included directly."
#endif

#ifndef OGS_PFCP_POOL_CODEC_H
#define OGS_PFCP_POOL_CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Local profile capacities, not TS 29.244 wire maxima. Keep generator in sync. */
#define OGS_PFCP_MAX_POOL_INFORMATION 16
#define OGS_PFCP_MAX_POOL_IDENTITIES 16
#define OGS_PFCP_MAX_POOL_SLICES 8
#define OGS_PFCP_MAX_POOL_ID_LEN 64
#define OGS_PFCP_POOL_IP_V4 0x01 /* TS 29.244 8.2.177, octet 5 bit 1 */
#define OGS_PFCP_POOL_IP_V6 0x02 /* octet 5 bit 2 */

typedef enum {
    OGS_PFCP_POOL_ABSENT,
    OGS_PFCP_POOL_VALID,
    OGS_PFCP_POOL_DEGRADED,
    OGS_PFCP_POOL_UNUSABLE,
    OGS_PFCP_POOL_UNSUPPORTED,
    OGS_PFCP_POOL_FRAMING_ERROR
} ogs_pfcp_pool_decode_status_t;

typedef struct {
    ogs_pfcp_pool_decode_status_t status;
    unsigned int groups_seen;
    unsigned int groups_invalid;
    unsigned int groups_decoded;
    uint16_t offending_ie; /* Only meaningful for structural failure. */
} ogs_pfcp_pool_decode_result_t;

/* These inspect a TLV body, without a PFCP header or association acceptance. */
int ogs_pfcp_scan_pool_information(const void *data, size_t len,
        ogs_pfcp_pool_decode_result_t *result);
int ogs_pfcp_check_tlv_framing(ogs_tlv_desc_t *desc,
        const void *data, size_t len);
/* Any allocated *storage belongs to the caller, including on parse failure. */
int ogs_pfcp_parse_pool_tlvs(void *msg, ogs_tlv_desc_t *desc,
        ogs_pkbuf_t *body, ogs_pfcp_pool_decode_result_t *result,
        ogs_pkbuf_t **storage);

/* Identity helpers borrow input / caller-provided output; never use strlen. */
int ogs_pfcp_build_pool_identity(ogs_tlv_octet_t *octet,
        const void *identity, size_t length, void *buffer, size_t capacity);
int ogs_pfcp_parse_pool_identity(const ogs_tlv_octet_t *octet,
        const uint8_t **identity, size_t *length);
int ogs_pfcp_parse_pool_network_instance(char *dnn, size_t capacity,
        const ogs_tlv_octet_t *octet);

#ifdef __cplusplus
}
#endif
#endif
