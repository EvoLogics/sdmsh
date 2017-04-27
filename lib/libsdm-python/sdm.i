%module sdm
%include "typemaps.i"

uint16_t *sdm_load_samples(char *, size_t *OUTPUT);

%{
 #include <sdm.h>
%}

%include <sdm.h>

