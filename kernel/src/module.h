#pragma once

// ref: https://webassembly.github.io/spec/core/

#include <stdint.h>
#include "vector.h"

typedef struct {
    vector_t rt1;
    vector_t rt2;
} functype_t;

struct section {
    union {
        // typesec
        vector_t functypes;
        
        // funcsec
        vector_t typeidxes;
    };
};

struct module {
    struct section *known_sections[12];
};