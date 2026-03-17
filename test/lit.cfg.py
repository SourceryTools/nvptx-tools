# lit configuration

import lit.formats
import os
import subprocess


config.name = 'nvptx-tools'

config.test_format = lit.formats.ShTest(True)

config.test_source_root = os.path.dirname(__file__)

config.suffixes = ['.test']


# <https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_03>
# states that "If 'PATH' is unset or is set to null, the path search is
# implementation-defined."  To avoid that, and to avoid any potentially
# undefined behavior when pointing to a non-existing directory, set up 'PATH'
# pointing to a directory that doesn't contain any meaningful executable files.
config.substitutions.append(('%empty_path', 'PATH=' + config.test_source_root))


config.substitutions.append(('%target_ar_cmd', config.target_ar))


config.substitutions.append(('%target_as_cmd', config.target_as))

if subprocess.call(args = [config.target_as, '--query=ptxas_usable']) == 0:
    config.available_features.add('as_ptxas_usable')

config.substitutions.append(('%dummy_ptxas_path', 'PATH=' + config.test_source_root + '/as/ptxas:$PATH'))

# Error diagnostics as emitted by the 'nvptx-none-as' minimalistic verification.
config.substitutions.append(('%r_target_as_missing_version_directive', '^nvptx-as: missing \.version directive at start of file'))
config.substitutions.append(('%r_target_as_malformed_version_directive', '^nvptx-as: malformed \.version directive at start of file'))
config.substitutions.append(('%r_target_as_missing_target_directive', '^nvptx-as: missing \.target directive at start of file'))
config.substitutions.append(('%r_target_as_malformed_target_directive', '^nvptx-as: malformed \.target directive at start of file'))
config.substitutions.append(('%r_target_as_unsupported_list_in_target_directive', '^nvptx-as: unsupported list in \.target directive at start of file'))


config.substitutions.append(('%target_ld_cmd', config.target_ld))


config.substitutions.append(('%target_nm_cmd', config.target_nm))

# Run with 'LC_COLLATE=C' to get deterministic output order.
config.substitutions.append(('%env_LC_COLLATE=C_target_nm_cmd', 'env LC_COLLATE=C ' + config.target_nm))


config.substitutions.append(('%target_ranlib_cmd', config.target_ranlib))


if config.target_run:
    config.available_features.add('target_run')

    config.substitutions.append(('%target_run_cmd', config.target_run))

    # If running the testsuite in parallel mode, we'll easily run out of device stack/heap memory.
    # Our usual testcases don't need much, so be arbitrarily conservative.
    config.substitutions.append(('%target_run_tiny_cmd', config.target_run + ' --stack-size 1024 --heap-size 1048576'))
