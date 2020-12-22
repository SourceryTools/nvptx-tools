# lit configuration

import lit.formats


config.name = 'nvptx-tools'

config.test_format = lit.formats.ShTest(True)

config.test_source_root = os.path.dirname(__file__)

config.suffixes = ['.test']


config.substitutions.append(('%target_as_cmd', config.target_as))


config.substitutions.append(('%target_ld_cmd', config.target_ld))


if config.target_run:
    config.available_features.add('target_run')

    config.substitutions.append(('%target_run_cmd', config.target_run))
