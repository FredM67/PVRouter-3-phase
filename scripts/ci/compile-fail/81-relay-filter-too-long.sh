# 22 minutes at 5 s are 264 datalog periods: the EWMA of the relays takes the count as a
# uint8_t, it wrapped to 8 (a filter of 40 s).
relays_on
replace 'RELAY_FILTER_DELAY{ 2 }' 'RELAY_FILTER_DELAY{ 22 }'
expect="RELAY_FILTER_DELAY too long for DATALOG_PERIOD_IN_SECONDS"
