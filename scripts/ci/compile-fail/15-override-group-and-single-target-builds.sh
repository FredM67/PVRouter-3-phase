# A group and single targets in one list, which the former bitmask helpers could not mix, builds.
replace 'overridePins{ { { 4, ALL_LOADS() } } }' 'overridePins{ { { 4, { ALL_LOCAL_LOADS(), LOAD(1) } } } }'
