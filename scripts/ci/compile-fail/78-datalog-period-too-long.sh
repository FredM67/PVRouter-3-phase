# 41 s is beyond the longest datalog period (40 s).
replace 'DATALOG_PERIOD_IN_SECONDS{ 5 }' 'DATALOG_PERIOD_IN_SECONDS{ 41 }'
expect="Data log duration is too long and will lead to overflow"
