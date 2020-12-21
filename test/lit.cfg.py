# lit configuration

import lit.formats


config.name = 'nvptx-tools'

config.test_format = lit.formats.ShTest(True)

config.test_source_root = os.path.dirname(__file__)

config.suffixes = ['.test']
