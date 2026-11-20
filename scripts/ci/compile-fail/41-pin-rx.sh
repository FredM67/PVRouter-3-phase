# Pin 0 is RX.
replace 'overridePins{ { { 4, ALL_LOADS() } } }' 'overridePins{ { { 0, ALL_LOADS() } } }'
expect='Pins 0 & 1 are reserved for RX/TX'
