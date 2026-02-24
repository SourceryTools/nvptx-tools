# See '../ptxas/lit.local.cfg.py'.


# Apply '--inhibit=libnvjitlink' to all, to avoid nvJitLink library interfere
# with 'ptxas' verification.

config.substitutions.insert(0, ('%target_as_local_cmd',
                                '%target_as_cmd'
                                ' --inhibit=libnvjitlink'))

config.substitutions.insert(0, ('%target_as_dummy_ptxas_cmd',
                                '%dummy_ptxas_path'
                                ' DUMMY_PTXAS_LOG=%t.dummy_ptxas_log'
                                ' %target_as_local_cmd'))

config.substitutions.insert(0, ('%target_as_empty_path_cmd',
                                '%empty_path'
                                ' %target_as_local_cmd'))
