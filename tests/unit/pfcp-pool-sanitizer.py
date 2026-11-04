#!/usr/bin/env python3
"""Run the existing ABTS pool cases against an ASan/UBSan Meson libpfcp build.

Usage: python3 pfcp-pool-sanitizer.py SANITIZED_BUILD OUTPUT_DIRECTORY
First build lib/pfcp/libogspfcp.so.2.8.0 with b_sanitize=address,undefined.
The small runner avoids building the unrelated SBI/ASN.1 unit dependencies.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

DRIVER = '''#include "ogs-pfcp.h"
#include "core/abts.h"
abts_suite *test_pfcp_pool(abts_suite *suite);
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
    ogs_log_install_domain(&__ogs_pfcp_domain, "pfcp", OGS_LOG_ERROR);
    suite = test_pfcp_pool(NULL);
    rv = abts_report(suite);
    ogs_pkbuf_default_destroy();
    ogs_core_terminate();
    talloc_disable_null_tracking(); /* release harness-global tracking root */
    return rv;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--association', action='store_true', help='Run stage 3 association/lifetime suite')
    parser.add_argument('build', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    entries = json.loads((build / 'compile_commands.json').read_text())
    suite = 'pfcp-association' if args.association else 'pfcp-pool'
    entry = next(c for c in entries if c['file'].endswith('/' + suite + '-test.c'))
    command = shlex.split(entry['command'])
    flags = command[:command.index('-MD')]
    if '-fsanitize=address,undefined' not in flags:
        parser.error('build must use b_sanitize=address,undefined')
    driver = output / 'main.c'
    driver.write_text(DRIVER.replace('test_pfcp_pool', 'test_pfcp_association') if args.association else DRIVER)
    objects = [output / 'cases.o', output / 'main.o']
    for source, obj in zip([entry['file'], str(driver)], objects):
        subprocess.run(flags + ['-c', source, '-o', str(obj)], cwd=entry['directory'], check=True)
    env = os.environ.copy()
    libdirs = sorted({str(p.parent) for p in build.rglob('*.so*') if p.is_file()})
    env['LD_LIBRARY_PATH'] = ':'.join(libdirs)
    binary = output / 'pfcp-pool-asan'
    subprocess.run(['cc', '-fsanitize=address,undefined', *map(str, objects),
                    '-L' + str(build / 'lib/pfcp'), '-logspfcp',
                    '-L' + str(build / 'lib/core'), '-logscore',
                    '-L' + str(build / 'lib/app'), '-logsapp', '-ltalloc',
                    '-o', str(binary)], env=env, check=True)
    env['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    subprocess.run([str(binary)], env=env, check=True)


if __name__ == '__main__':
    main()
