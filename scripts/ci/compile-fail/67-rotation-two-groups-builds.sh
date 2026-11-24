# Two groups: two local loads, and two loads on remote unit 1.
replace 'PRIORITY_ROTATION{ RotationModes::OFF }' 'PRIORITY_ROTATION{ RotationModes::AUTO }'
replace 'NO_OF_DUMPLOADS{ 3 }' 'NO_OF_DUMPLOADS{ 4 }'
replace 'Load::local(7)' $'Load::remote(1),\n  Load::remote(1)'
replace 'loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1, 2 }' 'loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1, 2, 3 }'
replace 'rotationGroups{};' 'rotationGroups{ { LOAD(0), LOAD(1) }, { LOAD(2), LOAD(3) } };'
expect=''
