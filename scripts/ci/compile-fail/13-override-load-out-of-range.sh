# LOAD(3) with 3 loads (LOAD() counts from 0).
replace 'overridePins{ { { 4, ALL_LOADS() } } }' 'overridePins{ { { 4, { LOAD(3) } } } }'
expect='An override pin forces a pin that is neither a local load nor a relay'
