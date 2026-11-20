# Probes on, but no pin for them.
replace 'TEMP_SENSOR_PRESENT{ false }' 'TEMP_SENSOR_PRESENT{ true }'
expect='Wrong pin value for temperature sensor(s)'
