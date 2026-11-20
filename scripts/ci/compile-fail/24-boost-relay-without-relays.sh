# RELAY(0) while relay diversion is off.
dual_tariff_on
replace '{ LOAD(0) } } } }' '{ RELAY(0) } } } }'
expect='A scheduled boost forces something that is neither a local load nor a relay'
