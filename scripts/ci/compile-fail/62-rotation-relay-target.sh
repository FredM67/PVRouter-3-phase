# A relay in a rotation group.
replace 'PRIORITY_ROTATION{ RotationModes::OFF }' 'PRIORITY_ROTATION{ RotationModes::AUTO }'
replace 'rotationGroups{};' 'rotationGroups{ { LOAD(0), RELAY(0) } };'
expect='A rotation group names something that is not a load'
