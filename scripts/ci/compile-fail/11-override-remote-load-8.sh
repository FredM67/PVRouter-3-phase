# REMOTE_LOAD() goes up to 7.
replace 'overridePins{ { { 4, ALL_LOADS() } } }' 'overridePins{ { { 4, { REMOTE_LOAD(8) } } } }'
expect='An override pin forces a pin that is neither a local load nor a relay'
