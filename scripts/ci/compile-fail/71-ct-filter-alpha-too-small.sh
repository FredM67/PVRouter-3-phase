# alpha too small for the resolution of its fixed point.
replace_calibration 'lpf_gain{ 0 }' 'lpf_gain{ 9 }'
replace_calibration 'alpha{ 0.002 }' 'alpha{ 0.00001 }'
expect='Wrong CT filter settings'
