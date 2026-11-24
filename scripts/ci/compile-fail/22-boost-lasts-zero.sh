# A boost of 0 minutes never runs.
dual_tariff_on
replace 'FOR(HOURS(2))' 'FOR(MINUTES(0))'
expect='A scheduled boost starts or ends outside the off-peak period'
