# The override pin is also a load pin.
replace 'overridePins{ { { 4, ALL_LOADS() } } }' 'overridePins{ { { 5, ALL_LOADS() } } }'
expect='Duplicate pin definition'
