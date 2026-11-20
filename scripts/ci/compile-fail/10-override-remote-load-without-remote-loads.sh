# Bug 7: REMOTE_LOAD(0) with no remote loads.
replace 'overridePins{ { { 4, ALL_LOADS() } } }' 'overridePins{ { { 4, { REMOTE_LOAD(0) } } } }'
expect='An override pin forces a remote load that does not exist'
