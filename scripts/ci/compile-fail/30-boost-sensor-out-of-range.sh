# Sensor 2 with two sensors (counted from 0).
dual_tariff_on
temperature_on
replace 'BOOST_TEMP_SENSOR{ 0 }' 'BOOST_TEMP_SENSOR{ 2 }'
expect='BOOST_TEMP_SENSOR must be one of the sensors in temperatureSensing'
