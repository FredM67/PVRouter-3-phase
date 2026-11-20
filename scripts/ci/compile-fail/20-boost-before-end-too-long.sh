# Starts 9 hours before the end of an 8-hour off-peak period.
dual_tariff_on
replace 'BEFORE_END(HOURS(3)), FOR(HOURS(2))' 'BEFORE_END(HOURS(9)), FOR(HOURS(2))'
expect='A scheduled boost starts or ends outside the off-peak period'
