# 31 s is beyond the longest datalog period (30 s).
replace 'DATALOG_PERIOD_IN_SECONDS{ 5 }' 'DATALOG_PERIOD_IN_SECONDS{ 31 }'
expect="Data log duration is too long and will lead to overflow"
