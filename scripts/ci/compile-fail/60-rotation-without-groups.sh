# Rotation on, no group: nothing says which loads take turns.
replace 'PRIORITY_ROTATION{ RotationModes::OFF }' 'PRIORITY_ROTATION{ RotationModes::AUTO }'
expect='Priority rotation is on but rotationGroups is empty'
