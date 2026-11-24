# Groups given while rotation is OFF.
replace 'rotationGroups{};' 'rotationGroups{ { ALL_LOCAL_LOADS() } };'
expect='rotationGroups is set but priority rotation is OFF'
