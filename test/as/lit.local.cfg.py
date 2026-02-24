import subprocess


arch0 = None
if 'as_libnvrtc_usable' in config.available_features:
    # Determine the baseline SM version supported.
    r = subprocess.run(args = [config.target_as, '--query=libnvrtc_supported_archs'],
                       capture_output = True,
                       text = True)
    if r.returncode == 0 and not r.stderr and r.stdout:
        arch0 = r.stdout.splitlines()[0]
        config.substitutions.append(('%target_as_arch0',
                                     arch0))
    else:
        lit_config.fatal('Failed to determine the baseline SM version supported:'
                         f' {r.args=}, {r.returncode=}, {r.stderr=}, {r.stdout=}')

if arch0 and 'as_ptxas_usable' in config.available_features:
    config.substitutions.append(('%target_as_m_sm_arch0',
                                 '-m sm_' + arch0))
else:
    config.substitutions.append(('%target_as_m_sm_arch0',
                                 ''))
