# REMOTE_LOAD(0) with no remote loads.
dual_tariff_on
replace '{ LOAD(0) } } } }' '{ REMOTE_LOAD(0) } } } }'
expect='A scheduled boost forces a remote load that does not exist'
