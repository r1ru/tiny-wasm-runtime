#pragma once

#include "error.h"

// inspired by wasm3(https://github.com/wasm3/wasm3)
#define __try                   error_t err = ERR_SUCCESS;
#define __throw(e)              {err = e; goto __catch;}
#define __throwif(e, cond)      if(cond) {__throw(e);}

#define __throwiferr(expr)          \
    do {                            \
        error_t __err = (expr);     \
        if(IS_ERROR(__err))         \
            __throw(__err);         \
    } while(0)
