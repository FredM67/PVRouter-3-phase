# A group of one load.
replace 'PRIORITY_ROTATION{ RotationModes::OFF }' 'PRIORITY_ROTATION{ RotationModes::AUTO }'
replace 'rotationGroups{};' 'rotationGroups{ { LOAD(0) } };'
expect='A rotation group needs at least 2 loads'
