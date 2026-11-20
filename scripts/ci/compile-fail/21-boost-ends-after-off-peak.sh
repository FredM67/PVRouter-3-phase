# From hour 7 for 2 hours: ends an hour after the 8-hour off-peak period.
dual_tariff_on
replace 'BEFORE_END(HOURS(3)), FOR(HOURS(2))' 'FROM_START(HOURS(7)), FOR(HOURS(2))'
expect='A scheduled boost starts or ends outside the off-peak period'
