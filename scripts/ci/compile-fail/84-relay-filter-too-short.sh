# 2 minutes at 30 s are 4 datalog periods: the triple EMA would shift by a negative count.
relays_on
replace 'DATALOG_PERIOD_IN_SECONDS{ 5 }' 'DATALOG_PERIOD_IN_SECONDS{ 30 }'
expect="RELAY_FILTER_DELAY too short for DATALOG_PERIOD_IN_SECONDS"
