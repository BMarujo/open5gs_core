/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Real NAS/NGAP integration driver. Public synthetic test credentials only.
 * Registration follows tests/slice/different-dnn-test.c (Sukchan Lee).
 * External runner owns all NFs; this driver never injects an SMF session. */
#include "test-app.h"
#include <arpa/inet.h>
#include <poll.h>

static bson_t *subscriber(test_ue_t *ue)
{
    const char *names[] = {"internet", "edge1", "nomatch"};
    bson_t *base = test_db_new_simple(ue), *doc = bson_new();
    bson_t arr, slice, sessions, prototype, item;
    bson_iter_t it, nested;
    const uint8_t *bytes;
    uint32_t size;
    unsigned i;
    ogs_assert(bson_iter_init(&it, base));
    ogs_assert(bson_iter_find_descendant(&it, "slice.0.session.0", &nested));
    bson_iter_document(&nested, &size, &bytes);
    ogs_assert(bson_init_static(&prototype, bytes, size));
    bson_copy_to_excluding_noinit(base, doc, "slice", NULL);
    BSON_APPEND_ARRAY_BEGIN(doc, "slice", &arr);
    BSON_APPEND_DOCUMENT_BEGIN(&arr, "0", &slice);
    BSON_APPEND_INT32(&slice, "sst", 1);
    BSON_APPEND_BOOL(&slice, "default_indicator", true);
    BSON_APPEND_ARRAY_BEGIN(&slice, "session", &sessions);
    for (i = 0; i < OGS_ARRAY_SIZE(names); i++) {
        char key[16];
        ogs_snprintf(key, sizeof key, "%u", i);
        bson_init(&item);
        bson_copy_to_excluding_noinit(&prototype, &item, "name", NULL);
        BSON_APPEND_UTF8(&item, "name", names[i]);
        BSON_APPEND_DOCUMENT(&sessions, key, &item);
        bson_destroy(&item);
    }
    bson_append_array_end(&slice, &sessions);
    bson_append_document_end(&arr, &slice);
    bson_append_array_end(doc, &arr);
    bson_destroy(base);
    return doc;
}
static void initialize(const char *const argv[])
{
    ogs_assert(ogs_app_initialize(NULL, NULL, argv) == OGS_OK);
    test_5gc_init();
}
int main(int argc, const char *const argv[])
{
    int rv;
    ogs_socknode_t *ngap, *gtpu;
    ogs_pkbuf_t *gmmbuf, *gsmbuf, *nasbuf, *sendbuf, *recvbuf;
    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_ue_t *test_ue;
    test_sess_t *sess;
    bson_t *doc;
    char line[128], command[16], dnn[64];
    unsigned psi;
    test_app_run(argc, argv, "slice.yaml", initialize);
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Setup Test UE & Session Context */
    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    test_ue = test_ue_add_by_suci(&mobile_identity_suci, "00004778");
    ogs_assert(test_ue);

    test_ue->nr_cgi.cell_id = 0x40001;

    test_ue->nas.registration.tsc = 0;
    test_ue->nas.registration.ksi = 0;
    test_ue->nas.registration.follow_on_request = 1;
    test_ue->nas.registration.value = OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    test_ue->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    test_ue->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    /* gNB connects to AMF */
    ngap = testngap_client(1, AF_INET);
    ogs_assert(ngap);

    /* gNB connects to UPF */
    gtpu = test_gtpu_server(1, AF_INET);
    ogs_assert(gtpu);

    /* Send NG-Setup Reqeust */
    sendbuf = testngap_build_ng_setup_request(0x102, 32);
    ogs_assert(sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ogs_assert((OGS_OK) == (rv));

    /* Receive NG-Setup Response */
    recvbuf = testgnb_ngap_read(ngap);
    ogs_assert(recvbuf);
    testngap_recv(test_ue, recvbuf);

    /********** Insert Subscriber in Database */
    doc = subscriber(test_ue);
    ogs_assert(doc);
    ogs_assert((OGS_OK) == (test_db_insert_ue(test_ue, doc)));

    /* Send Registration request */
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ogs_assert(gmmbuf);
    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ogs_assert(sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ogs_assert((OGS_OK) == (rv));

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ogs_assert(recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ogs_assert(gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ogs_assert(sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ogs_assert((OGS_OK) == (rv));

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ogs_assert(recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ogs_assert(nasbuf);
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ogs_assert(gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ogs_assert(sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ogs_assert((OGS_OK) == (rv));

    /* Receive Initial context setup request +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ogs_assert(recvbuf);
    testngap_recv(test_ue, recvbuf);
    ogs_assert((NGAP_ProcedureCode_id_InitialContextSetup) == (test_ue->ngap_procedure_code));

    /* Send UE radio capability info indication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ogs_assert(sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ogs_assert((OGS_OK) == (rv));

    /* Send Initial context setup response */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ogs_assert(sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ogs_assert((OGS_OK) == (rv));

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ogs_assert(gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ogs_assert(sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ogs_assert((OGS_OK) == (rv));

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ogs_assert(recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* test_db_insert_ue() consumes doc. */
    printf("RESULT {\"ready\":true,\"imsi\":\"%s\"}\n", test_ue->imsi);
    while (fgets(line, sizeof line, stdin)) {
        char ip[INET_ADDRSTRLEN], ip6[INET6_ADDRSTRLEN], n3[INET_ADDRSTRLEN];
        int fields = sscanf(line, "%15s %u %63s", command, &psi, dnn);
        if (fields < 2) break;
        if (!strcmp(command, "rs")) {
            ogs_gtp2_header_desc_t header;
            int offset;
            sess = test_sess_find_by_psi(test_ue, psi);
            ogs_assert(sess && sess->ue_ip.ipv6);
            ogs_assert(test_gtpu_send_slacc_rs(gtpu,
                test_qos_flow_find_by_qfi(sess, 1)) == OGS_OK);
            recvbuf = test_gtpu_read(gtpu);
            ogs_assert(recvbuf);
            offset = ogs_gtpu_parse_header(&header, recvbuf);
            ogs_assert(offset >= OGS_GTPV1U_HEADER_LEN);
            ogs_assert(recvbuf->len >= offset + 56);
            ogs_assert(recvbuf->data[offset+6] == IPPROTO_ICMPV6);
            ogs_assert(recvbuf->data[offset+40] == 134); /* RA */
            testgtpu_recv(test_ue, recvbuf); /* validates TEID/QFI; learns prefix */
            inet_ntop(AF_INET6, sess->ue_ip.addr6, ip6, sizeof ip6);
            printf("RESULT {\"rs\":%u,\"ue_ipv6\":\"%s\"}\n", psi, ip6);
            continue;
        }
        if (!strcmp(command, "ping") || !strcmp(command, "probe")) {
            ogs_gtp2_header_desc_t header;
            int offset;
            uint8_t *inner;
            struct in_addr gateway;
            struct in6_addr gateway6;
            ogs_assert(fields == 3);
            sess = test_sess_find_by_psi(test_ue, psi);
            ogs_assert(sess);
            ogs_assert(test_gtpu_send_ping(gtpu,
                test_qos_flow_find_by_qfi(sess, 1), dnn) == OGS_OK);
            if (!strcmp(command, "probe")) {
                struct pollfd pollfd = {gtpu->sock->fd, POLLIN, 0};
                ogs_assert(poll(&pollfd, 1, 1500) == 0);
                printf("RESULT {\"probe\":%u,\"received\":false}\n", psi);
                continue;
            }
            recvbuf = testgnb_gtpu_read(gtpu);
            ogs_assert(recvbuf);
            offset = ogs_gtpu_parse_header(&header, recvbuf);
            ogs_assert(offset >= OGS_GTPV1U_HEADER_LEN);
            ogs_assert(header.type == OGS_GTPU_MSGTYPE_GPDU);
            ogs_assert(header.teid == sess->gnb_n3_teid);
            ogs_assert(recvbuf->len >= offset + 28);
            inner = recvbuf->data + offset;
            if (strchr(dnn, ':')) {
                ogs_assert(recvbuf->len >= offset + 48);
                ogs_assert((inner[0] >> 4) == 6 && inner[6] == IPPROTO_ICMPV6);
                ogs_assert(inner[40] == 129 && inner[41] == 0); /* Echo reply */
                ogs_assert(inet_pton(AF_INET6, dnn, &gateway6) == 1);
                ogs_assert(!memcmp(inner + 8, &gateway6, 16));
                ogs_assert(!memcmp(inner + 24, sess->ue_ip.addr6, 16));
            } else {
                ogs_assert(inner[0] == 0x45 && inner[9] == IPPROTO_ICMP);
                ogs_assert(inner[20] == 0 && inner[21] == 0); /* Echo reply */
                ogs_assert(inet_pton(AF_INET, dnn, &gateway) == 1);
                ogs_assert(!memcmp(inner + 12, &gateway, 4));
                ogs_assert(!memcmp(inner + 16, &sess->ue_ip.addr, 4));
            }
            printf("RESULT {\"ping\":%u,\"bytes\":%u}\n", psi, recvbuf->len);
            ogs_pkbuf_free(recvbuf);
            continue;
        }
        ogs_assert((!strcmp(command, "establish") ||
                    !strcmp(command, "establish6") ||
                    !strcmp(command, "establish46")) && fields == 3);
        sess = test_sess_add_by_dnn_and_psi(test_ue, dnn, psi);
        ogs_assert(sess);
        sess->pti = 20 + psi;
        sess->pdu_session_type = !strcmp(command, "establish6") ?
            OGS_PDU_SESSION_TYPE_IPV6 : !strcmp(command, "establish46") ?
            OGS_PDU_SESSION_TYPE_IPV4V6 : OGS_PDU_SESSION_TYPE_IPV4;
        sess->ul_nas_transport_param.request_type = OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
        sess->ul_nas_transport_param.dnn = 1;
        sess->pdu_session_establishment_param.ssc_mode = 1;
        sess->pdu_session_establishment_param.epco = 1;
        gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
        gmmbuf = testgmm_build_ul_nas_transport(sess,
                OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
        sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
        ogs_assert(testgnb_ngap_send(ngap, sendbuf) == OGS_OK);
        recvbuf = testgnb_ngap_read(ngap);
        ogs_assert(recvbuf);
        testngap_recv(test_ue, recvbuf);
        if (test_ue->ngap_procedure_code != NGAP_ProcedureCode_id_PDUSessionResourceSetup) {
            ogs_assert(test_ue->gsm_message_type == OGS_NAS_5GS_PDU_SESSION_ESTABLISHMENT_REJECT);
            printf("RESULT {\"psi\":%u,\"accepted\":false,\"gsm_type\":%u,\"procedure\":%ld}\n",
                    psi, test_ue->gsm_message_type, (long)test_ue->ngap_procedure_code);
            continue;
        }
        sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
        ogs_assert(testgnb_ngap_send(ngap, sendbuf) == OGS_OK);
        inet_ntop(AF_INET, &sess->ue_ip.addr, ip, sizeof ip);
        inet_ntop(AF_INET, &sess->upf_n3_ip.addr, n3, sizeof n3);
        printf("RESULT {\"psi\":%u,\"accepted\":true,\"dnn\":\"%s\",\"ue_ip\":\"%s\",\"n3\":\"%s\",\"upf_teid\":%u,\"gnb_teid\":%u,\"ipv4\":%s,\"ipv6\":%s}\n",
                psi, dnn, ip, n3, sess->upf_n3_teid, sess->gnb_n3_teid,
                sess->ue_ip.ipv4 ? "true" : "false", sess->ue_ip.ipv6 ? "true" : "false");
    }
    ogs_assert(test_db_remove_ue(test_ue) == OGS_OK);
    test_ue_remove(test_ue);
    testgnb_gtpu_close(gtpu);
    testgnb_ngap_close(ngap);
    test_5gc_final();
    ogs_app_terminate();
    return 0;
}
