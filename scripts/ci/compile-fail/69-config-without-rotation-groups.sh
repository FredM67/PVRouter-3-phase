# A config.h from before rotationGroups, rotation OFF: the line is still needed.
replace 'inline constexpr RotationGroups rotationGroups{}; /**< the loads that take turns, {} without rotation */' ''
expect="'rotationGroups' was not declared"
