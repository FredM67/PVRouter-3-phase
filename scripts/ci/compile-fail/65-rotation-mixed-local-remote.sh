# Two local loads and a remote one in one group.
replace 'PRIORITY_ROTATION{ RotationModes::OFF }' 'PRIORITY_ROTATION{ RotationModes::AUTO }'
replace 'Load::local(7)' 'Load::remote(1)'
replace 'rotationGroups{};' 'rotationGroups{ { ALL_LOADS() } };'
expect='A rotation group mixes local and remote loads, or loads of two remote units'
