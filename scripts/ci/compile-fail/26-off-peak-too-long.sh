# 13 hours of off-peak.
dual_tariff_on
replace 'ul_OFF_PEAK_DURATION{ 8 }' 'ul_OFF_PEAK_DURATION{ 13 }'
expect='Off-peak duration cannot last more than 12 hours'
