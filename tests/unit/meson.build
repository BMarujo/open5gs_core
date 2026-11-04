# Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>

# This file is part of Open5GS.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

testunit_unit_sources = files('''
    abts-main.c
    proto-message-test.c
    s1ap-message-test.c
    nas-message-test.c
    gtp-message-test.c
    ngap-message-test.c
    sbi-message-test.c
    security-test.c
    nrf-discovery-test.c
    crash-test.c
    mme-dns-select-test.c
    ipfw-test.c
    pfcp-rule-test.c
    pfcp-pool-test.c
    pfcp-association-test.c
'''.split())

testunit_unit_exe = executable('unit',
    sources : testunit_unit_sources,
    c_args : [testunit_core_cc_flags, sbi_cc_flags],
    include_directories : srcinc,
    dependencies : [libs1ap_dep,
                    libgtp_dep,
                    libngap_dep,
                    libnas_eps_dep,
                    libsbi_dep,
                    libipfw_dep,
                    libpfcp_dep,
                    libmme_dep])

test('unit', testunit_unit_exe, is_parallel : false, suite: 'unit')

# Link the actual SMF context/selector, without copying production functions.
smf_selection_exe = executable('smf-selection',
    sources : ['smf-selection-main.c', 'smf-selection-test.c'],
    c_args : testunit_core_cc_flags,
    include_directories : [srcinc, include_directories('../../src/smf')],
    dependencies : libsmf_dep)
test('smf-selection', smf_selection_exe, is_parallel : false, suite: 'unit')

# Test-only pipe commands run on the real SMF poll thread. Register before the
# thread starts; no production API or NF behavior is added by this executable.
if cc.get_id() in ['gcc', 'clang'] and host_machine.system() == 'linux'
    executable('smf-selector-driver',
        sources : ['smf-selector-driver.c', '../../src/main.c'],
        c_args : '-DDEFAULT_CONFIG_FILENAME="smf.yaml"',
        include_directories : [srcinc, include_directories('../../src/smf')],
        dependencies : libsmf_dep,
        link_args : '-Wl,--wrap=smf_pfcp_open')
endif
