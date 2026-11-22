# A negative gain is no filter.
replace_calibration 'lpf_gain{ 0 }' 'lpf_gain{ -1 }'
replace_calibration 'alpha{ 0.002 }' 'alpha{ 0.002 }'
expect='Wrong CT filter settings'
