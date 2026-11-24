# Inner braces forgotten: two groups of one load each.
replace 'PRIORITY_ROTATION{ RotationModes::OFF }' 'PRIORITY_ROTATION{ RotationModes::AUTO }'
replace 'rotationGroups{};' 'rotationGroups{ LOAD(0), LOAD(1) };'
expect='A rotation group needs at least 2 loads'
