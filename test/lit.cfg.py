# lit configuration

import lit.formats
import os
import shutil
import subprocess


def which(cmd):
    # 'shutil.which' is "New in version 3.3".
    try:
        return shutil.which(cmd)
    except AttributeError:
        # <https://stackoverflow.com/a/9877856>
        for p in os.getenv('PATH').split(os.path.pathsep):
            p=os.path.join(p, cmd)
            if os.path.exists(p) and os.access(p, os.X_OK):
                return p

    return None


config.name = 'nvptx-tools'

config.test_format = lit.formats.ShTest(True)

config.test_source_root = os.path.dirname(__file__)

config.suffixes = ['.test']


# This one is optional.
config.ptxas = which('ptxas')


config.substitutions.append(('%target_ar_cmd', config.target_ar))


config.substitutions.append(('%target_as_cmd', config.target_as))

config.substitutions.append(('%dummy_ptxas_path', 'PATH=' + config.test_source_root + '/as/ptxas:$PATH'))

# Error diagnostics as emitted by the 'nvptx-none-as' minimalistic verification.
config.substitutions.append(('%r_target_as_missing_version_directive', '^nvptx-as: missing \.version directive at start of file'))
config.substitutions.append(('%r_target_as_malformed_version_directive', '^nvptx-as: malformed \.version directive at start of file'))
config.substitutions.append(('%r_target_as_missing_target_directive', '^nvptx-as: missing \.target directive at start of file'))
config.substitutions.append(('%r_target_as_malformed_target_directive', '^nvptx-as: malformed \.target directive at start of file'))
config.substitutions.append(('%r_target_as_unsupported_list_in_target_directive', '^nvptx-as: unsupported list in \.target directive at start of file'))

if config.ptxas:
    config.available_features.add('ptxas')


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
