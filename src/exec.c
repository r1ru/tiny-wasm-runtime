#include "exec.h"

// There is no need to use append when instantiating, since everything we need (functions, imports, etc.) is known to us.
// see Note of https://webassembly.github.io/spec/core/exec/modules.html#instantiation

// todo: add externval(support imports)

// There must be enough space in S to allocate all functions.
funcaddr_t allocfunc(store_t *S, func_t *func, moduleinst_t *moduleinst) {
    static funcaddr_t next_addr = 0;

    funcinst_t *funcinst = VECTOR_ELEM(&S->funcs, next_addr);
    functype_t *functype = VECTOR_ELEM(&moduleinst->types, func->type);

    funcinst->type   = functype;
    funcinst->module = moduleinst;
    funcinst->code   = func;

    return next_addr++; 
}

moduleinst_t *allocmodule(store_t *S, module_t *module) {
    // allocate moduleinst
    moduleinst_t *moduleinst = malloc(sizeof(moduleinst_t));
   
    VECTOR_COPY(&moduleinst->types, &module->types, functype_t);
    VECTOR_INIT(&moduleinst->fncaddrs, module->funcs.n, funcaddr_t);

    // allocate funcs
    // In this implementation, the index always matches the address.
    int idx = 0;
    VECTOR_FOR_EACH(func, &module->funcs, func_t) {
        // In this implementation, the index always matches the address.
        *VECTOR_ELEM(&moduleinst->fncaddrs, idx++) = allocfunc(S, func, moduleinst);
    }

    // allocate exports
    VECTOR_COPY(&moduleinst->exports, &module->exports, export_t);

     return moduleinst;
}

error_t instantiate(store_t **S, module_t *module) {
    // allocate store
    store_t *store = *S = malloc(sizeof(store_t));
    
    VECTOR_INIT(&store->funcs, module->funcs.n, funcinst_t);

    // todo: validate module
    
    moduleinst_t *moduleinst = allocmodule(store, module);

    // todo: support start section

    return ERR_SUCCESS;
}