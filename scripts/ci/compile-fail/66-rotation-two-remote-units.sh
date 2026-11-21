# One group across remote units 1 and 2.
replace 'PRIORITY_ROTATION{ RotationModes::OFF }' 'PRIORITY_ROTATION{ RotationModes::AUTO }'
replace 'Load::local(6)' 'Load::remote(1)'
replace 'Load::local(7)' 'Load::remote(2)'
replace 'rotationGroups{};' 'rotationGroups{ { LOAD(1), LOAD(2) } };'
expect='A rotation group mixes local and remote loads, or loads of two remote units'
