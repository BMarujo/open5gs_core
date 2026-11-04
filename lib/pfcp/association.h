/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef OGS_PFCP_ASSOCIATION_H
#define OGS_PFCP_ASSOCIATION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t len;
    uint8_t data[OGS_PFCP_MAX_POOL_ID_LEN];
} ogs_pfcp_pool_identity_t;

typedef struct {
    unsigned num_ids, num_slices;
    ogs_pfcp_pool_identity_t ids[OGS_PFCP_MAX_POOL_IDENTITIES];
    char dnn[OGS_MAX_DNN_LEN+1];
    bool family_present;
    uint8_t family; /* wire V4=1, V6=2; zero means unknown */
    uint8_t slices[OGS_PFCP_MAX_POOL_SLICES][4];
    bool usable; /* Local profile only; not association acceptance. */
} ogs_pfcp_learned_pool_t;

typedef struct ogs_pfcp_pool_snapshot_s {
    ogs_pfcp_pool_decode_result_t result;
    uint64_t generation;
    /* Bound only by a reply to a Heartbeat sent after this Setup. Never use
     * the Setup Recovery Time Stamp to establish restart history. */
    bool heartbeat_confirmed;
    uint32_t heartbeat_recovery;
    uint8_t source; /* Association Setup Request or Response */
    unsigned num_groups;
    ogs_pfcp_learned_pool_t groups[OGS_PFCP_MAX_POOL_INFORMATION];
} ogs_pfcp_pool_snapshot_t;

typedef struct {
    uint8_t identity[OGS_PFCP_MAX_POOL_INFORMATION][OGS_PFCP_MAX_POOL_ID_LEN+2];
    uint8_t network[OGS_PFCP_MAX_POOL_INFORMATION][OGS_MAX_DNN_LEN+1];
    uint8_t family[OGS_PFCP_MAX_POOL_INFORMATION];
} ogs_pfcp_pool_build_storage_t;

int ogs_pfcp_validate_pool_config(void);
int ogs_pfcp_build_pool_information(
        ogs_pfcp_tlv_ue_ip_address_pool_information_t *groups,
        ogs_pfcp_pool_build_storage_t *storage);
ogs_pfcp_pool_snapshot_t *ogs_pfcp_pool_snapshot_prepare(
        const ogs_pfcp_tlv_ue_ip_address_pool_information_t *groups,
        const ogs_pfcp_pool_decode_result_t *result, uint8_t source);
void ogs_pfcp_pool_snapshot_publish(ogs_pfcp_node_t *node,
        ogs_pfcp_pool_snapshot_t *snapshot);
void ogs_pfcp_pool_snapshot_clear(ogs_pfcp_node_t *node);
void ogs_pfcp_pool_snapshot_heartbeat(ogs_pfcp_node_t *node,
        uint32_t recovery, bool current_probe_response);

/* Validate before generic decoding can lose lengths or before Node-ID lookup.
 * Returns false after handling a Setup error; other messages pass through. */
bool ogs_pfcp_setup_pre_receive(ogs_pkbuf_t *packet,
        ogs_sockaddr_t *from, bool cp);
uint8_t ogs_pfcp_validate_setup(const ogs_pfcp_tlv_node_id_t *node_id,
        const ogs_pfcp_tlv_recovery_time_stamp_t *recovery,
        const ogs_pfcp_tlv_cause_t *cause);
/* Stop a matched erroneous Setup reply and notify the transaction application. */
void ogs_pfcp_setup_response_error(ogs_pfcp_xact_t *xact);

#ifdef __cplusplus
}
#endif
#endif
