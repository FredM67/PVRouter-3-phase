# The CT filter: alpha from 0.00005 to below 0.0625 (fixed point, ct_filter.h).
replace_calibration 'lpf_gain{ 0 }' 'lpf_gain{ 9 }'
replace_calibration 'alpha{ 0.002 }' 'alpha{ 0.1 }'
expect='Wrong CT filter settings'
