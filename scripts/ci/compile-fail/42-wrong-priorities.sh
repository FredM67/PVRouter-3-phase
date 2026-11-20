# The same priority twice.
replace 'loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1, 2 }' 'loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1, 1 }'
expect='Load Priorities wrong'
