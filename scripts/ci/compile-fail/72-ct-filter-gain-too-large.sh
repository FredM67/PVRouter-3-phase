# The gain of the CT filter must stay below 64.
replace_calibration 'lpf_gain{ 0 }' 'lpf_gain{ 64 }'
replace_calibration 'alpha{ 0.002 }' 'alpha{ 0.002 }'
expect='Wrong CT filter settings'
