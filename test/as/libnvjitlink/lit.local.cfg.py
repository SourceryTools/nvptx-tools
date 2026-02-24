# See '../ptxas/lit.local.cfg.py'.


# Apply '--inhibit=ptxas' to all, to avoid 'ptxas' interfere with nvJitLink
# library verification.

config.substitutions.insert(0, ('%target_as_local_cmd',
                                '%target_as_cmd'
                                ' --inhibit=ptxas'))

# We can't easily do like '../ptxas/lit.local.cfg.py':
# '%target_as_dummy_ptxas_cmd'.

# We can't easily do like '../ptxas/lit.local.cfg.py':
# '%target_as_empty_path_cmd'.
