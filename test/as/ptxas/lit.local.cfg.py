# Normally, such definitions would live as 'DEFINE: [...]' in the specific
# '*.test' files, but that's supported only as of lit that comes with LLVM 16:
# <https://releases.llvm.org/16.0.0/docs/TestingGuide.html#test-specific-substitutions>.
# We'd like to stay compatible with lit that comes with LLVM 14 (as available
# in Debian "bookworm", or Ubuntu 22.04 "jammy", for example), so we instead
# define the following, in here.


# Apply '--inhibit=libnvjitlink' to all, to avoid nvJitLink library interfere
# with 'ptxas' verification.  Apply '--inhibit=libnvrtc' to all, to avoid use
# of the NVRTC library with 'ptxas' verification.

config.substitutions.insert(0, ('%target_as_local_cmd',
                                '%target_as_cmd'
                                ' --inhibit=libnvjitlink'
                                ' --inhibit=libnvrtc'))

config.substitutions.insert(0, ('%target_as_dummy_ptxas_cmd',
                                '%dummy_ptxas_path'
                                ' DUMMY_PTXAS_LOG=%t.dummy_ptxas_log'
                                ' %target_as_local_cmd'))

config.substitutions.insert(0, ('%target_as_empty_path_cmd',
                                '%empty_path'
                                ' %target_as_local_cmd'))
