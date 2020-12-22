# lit configuration

import lit.formats


config.name = 'nvptx-tools'

config.test_format = lit.formats.ShTest(True)

config.test_source_root = os.path.dirname(__file__)

config.suffixes = ['.test']


config.substitutions.append(('%target_ar_cmd', config.target_ar))


config.substitutions.append(('%target_as_cmd', config.target_as))

config.substitutions.append(('%dummy_ptxas_path', 'PATH=' + config.test_source_root + '/as/ptxas:$PATH'))


config.substitutions.append(('%target_ld_cmd', config.target_ld))


config.substitutions.append(('%target_ranlib_cmd', config.target_ranlib))


if config.target_run:
    config.available_features.add('target_run')

    config.substitutions.append(('%target_run_cmd', config.target_run))

    # If running the testsuite in parallel mode, we'll easily run out of device stack/heap memory.
    # Our usual testcases don't need much, so be arbitrarily conservative.
    config.substitutions.append(('%target_run_tiny_cmd', config.target_run + ' --stack-size 1024 --heap-size 1048576'))
