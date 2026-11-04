/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "ogs-pfcp.h"
#include "core/abts.h"

abts_suite *test_smf_selection(abts_suite *suite);
int main(int argc, const char *const argv[])
{
    ogs_pkbuf_config_t config;
    const char *argv_out[argc+3];
    abts_suite *suite;
    int rv = abts_main(argc, argv, argv_out);
    if (rv != OGS_OK) return rv;
    ogs_core_initialize();
    ogs_pkbuf_default_init(&config);
    ogs_pkbuf_default_create(&config);
    suite = test_smf_selection(NULL);
    rv = abts_report(suite);
    ogs_pkbuf_default_destroy();
    ogs_core_terminate();
    talloc_disable_null_tracking();
    return rv;
}
