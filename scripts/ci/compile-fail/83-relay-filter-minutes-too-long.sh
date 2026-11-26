# The check takes the minutes relays carries, not the RELAY_FILTER_DELAY constant: MINUTES(25)
# with RELAY_FILTER_DELAY 2 built, with a filter count of 44.
relays_on
replace 'MINUTES(RELAY_FILTER_DELAY)' 'MINUTES(25)'
expect="RELAY_FILTER_DELAY too long for DATALOG_PERIOD_IN_SECONDS"
