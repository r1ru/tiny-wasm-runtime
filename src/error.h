#pragma once

typedef int error_t;

#define ERR_CODE(c) (-c)

#define ERR_SUCCESS     0
#define ERR_FAILED      ERR_CODE(1)