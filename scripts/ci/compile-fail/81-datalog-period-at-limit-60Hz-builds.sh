# 40 s at 60 Hz (26 sample sets per cycle, up to 30: 72000 sets) builds too.
replace 'SUPPLY_FREQUENCY{ 50 }' 'SUPPLY_FREQUENCY{ 60 }'
replace 'DATALOG_PERIOD_IN_SECONDS{ 5 }' 'DATALOG_PERIOD_IN_SECONDS{ 40 }'
