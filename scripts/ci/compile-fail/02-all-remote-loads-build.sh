# Bug 6: every load remote and no override pin, so the router uses no pin at all.
replace $'Load::local(5),\n  Load::local(6),\n  Load::local(7)' $'Load::remote(1),\n  Load::remote(1),\n  Load::remote(2)'
replace 'OVERRIDE_PIN_PRESENT{ true }' 'OVERRIDE_PIN_PRESENT{ false }'
