#include "exec.h"
#include "memory.h"

// There is no need to use append when instantiating, since everything we need (functions, imports, etc.) is known to us.
// see Note of https://webassembly.github.io/spec/core/exec/modules.html#instantiation

// todo: add externval(support imports)

// There must be enough space in S to allocate all functions.
funcaddr_t allocfunc(store_t *S, func_t *func, moduleinst_t *moduleinst) {
    static funcaddr_t next_addr = 0;

    funcinst_t *funcinst = &S->funcs[next_addr];
    functype_t *functype = &moduleinst->types[func->type];

    funcinst->type   = functype;
    funcinst->module = moduleinst;
    funcinst->code   = func;

    return next_addr++; 
}

moduleinst_t *allocmodule(store_t *S, module_t *module) {
    // allocate moduleinst
    moduleinst_t *moduleinst = malloc(sizeof(moduleinst_t));
    moduleinst->types = module->types.elem;
    moduleinst->funcaddrs = malloc(sizeof(funcaddr_t) *module->funcs.n);

    // allocate funcs
    // In this implementation, the index always matches the address.
    int idx = 0;
    VECTOR_FOR_EACH(func, &module->funcs, func_t) {
        // In this implementation, the index always matches the address.
        moduleinst->funcaddrs[idx++] = allocfunc(S, func, moduleinst);
    }

    // allocate exports
    moduleinst->exports = module->exports.elem;

     return moduleinst;
}

error_t instantiate(store_t **S, module_t *module) {
    // allocate store
    store_t *store = *S = malloc(sizeof(store_t));
    store->funcs = malloc(sizeof(funcinst_t) * module->funcs.n);

    // todo: validate module

    moduleinst_t *moduleinst = allocmodule(store, module);

    // todo: support start section

    return ERR_SUCCESS;
}