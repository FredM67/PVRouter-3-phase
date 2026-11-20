# 1093 minutes do not fit in 16 bits of seconds: on AVR, the product wrapped to 44 seconds.
replace '{ unused_pin, 100, 200, 1, 1 }' '{ unused_pin, 100, 200, 1093, 1 }'
expect="A relay's minimum ON or OFF time cannot exceed 1092 minutes"
