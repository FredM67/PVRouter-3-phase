# RELAY(0) while relay diversion is off.
replace 'overridePins{ { { 4, ALL_LOADS() } } }' 'overridePins{ { { 4, { RELAY(0) } } } }'
expect='An override pin forces a pin that is neither a local load nor a relay'
