# Robin's values for the old boards (single-phase calibration.h): the filter is on, and builds.
replace_calibration 'lpf_gain{ 0 }' 'lpf_gain{ 9 }'
replace_calibration 'alpha{ 0.002 }' 'alpha{ 0.0011 }'
expect=
