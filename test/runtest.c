#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <parson.h>
#include <vector.h>
#include <error.h>
#include <print.h>
#include <exception.h>
#include <decode.h>
#include <validate.h>
#include <exec.h>

typedef struct {
    list_elem_t link;
    const char  *name;
    instance_t  *instance;
} test_ctx_t;

static test_ctx_t *current_ctx = NULL;

list_t test_ctxs = {.prev = &test_ctxs, .next = &test_ctxs};

static const char *error_msg[] = {
    [-ERR_SUCCESS]                                              = "success",
    [-ERR_FAILED]                                               = "failed",
    [-ERR_UNEXPECTED_END]                                       = "unexpected end",
    [-ERR_LENGTH_OUT_OF_BOUNDS]                                 = "length out of bounds",
    [-ERR_MALFORMED_SECTION_ID]                                 = "malformed section id",
    [-ERR_FUNCTION_AND_CODE_SECTION_HAVE_INCOSISTENT_LENGTH]    = "function and code section have inconsistent lengths",
    [-ERR_DATA_COUNT_AND_DATA_SECTION_HAVE_INCOSISTENT_LENGTH]  = "data count and data section have inconsistent lengths",
    [-ERR_TYPE_MISMATCH]                                        = "type mismatch",
    [-ERR_UNKNOWN_LOCAL]                                        = "unknown local",
    [-ERR_UNKNOWN_LABEL]                                        = "unknown label",
    [-ERR_UNKNOWN_FUNC]                                         = "unknown function",
    [-ERR_UNKNOWN_TABLE]                                        = "unknown table",
    [-ERR_UNKNOWN_TYPE]                                         = "unknown type",
    [-ERR_UNKNOWN_GLOBAL]                                       = "unknown global",
    [-ERR_UNKNOWN_MEMORY]                                       = "unknown memory",
    [-ERR_UNKNOWN_DATA_SEGMENT]                                 = "unknown data segment",
    [-ERR_UNKNOWN_ELEM_SEGMENT]                                 = "unknown elem segment",
    [-ERR_INVALID_RESULT_ARITY]                                 = "invalid result arity",
    [-ERR_ALIGNMENT_MUST_NOT_BE_LARGER_THAN_NATURAL]            = "alignment must not be larger than natural",
    [-ERR_UNDECLARED_FUNCTIION_REFERENCE]                       = "undeclared function reference",
    [-ERR_TRAP_INTERGER_DIVIDE_BY_ZERO]                         = "integer divide by zero",
    [-ERR_TRAP_INTERGET_OVERFLOW]                               = "integer overflow",
    [-ERR_TRAP_INVALID_CONVERSION_TO_INTERGER]                  = "invalid conversion to integer",
    [-ERR_TRAP_UNDEFINED_ELEMENT]                               = "undefined element",
    [-ERR_TRAP_UNREACHABLE]                                     = "unreachable",
    [-ERR_TRAP_INDIRECT_CALL_TYPE_MISMATCH]                     = "indirect call type mismatch",
    [-ERR_TRAP_UNINITIALIZED_ELEMENT]                           = "uninitialized element",
    [-ERR_TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS]                     = "out of bounds memory access",
    [-ERR_TRAP_OUT_OF_BOUNDS_TABLE_ACCESS]                      = "out of bounds table access",
    [-ERR_TRAP_CALL_STACK_EXHAUSTED]                            = "call stack exhausted",
    [-ERR_INCOMPATIBLE_IMPORT_TYPE]                             = "incompatible import type",
};

// helpers
static exportinst_t *find_exportinst(const char *name) {
    VECTOR_FOR_EACH(e, &current_ctx->instance->moduleinst->exports) {
        if(strcmp(e->name, name) == 0) {
            return e;
        }
    }
    return NULL;
}

error_t lookup_func_by_name(const char *name, funcaddr_t *addr) {
    VECTOR_FOR_EACH(e, &current_ctx->instance->moduleinst->exports) {
        if(strcmp(e->name, name) == 0 && e->value.kind == EXTERN_FUNC) {
            *addr = e->value.func;
            return ERR_SUCCESS;
        }
    }
    return ERR_FAILED;
}

// always success
static test_ctx_t *find_test_ctx(const char *name) {
    LIST_FOR_EACH(ctx, &test_ctxs, test_ctx_t, link) {
        if(strcmp(ctx->name, name) == 0) {
            return ctx;
        }
    }
}

error_t decode_module_from_fpath(const char *fpath, module_t **mod) {
    __try {
        int fd = open(fpath, O_RDONLY);
        __throwif(ERR_FAILED, fd == -1);

        struct stat s;
        __throwif(ERR_FAILED, fstat(fd, &s) == -1);

        size_t size = s.st_size;
        uint8_t *image =  mmap(
            NULL,
            size,
            PROT_READ,
            MAP_PRIVATE,
            fd, 
            0
        );
        __throwif(ERR_FAILED, image == MAP_FAILED);

        __throwiferr(decode_module(mod, image, size));
    }
    __catch:
        err;
}

error_t register_imports(void) {
    __try {
        module_t *module;
        instance_t *instance;

        // decode
        __throwiferr(decode_module_from_fpath("./spectest.wasm", &module));

        // validate
        __throwiferr(validate_module(module));

        // instantiate
        __throwiferr(instantiate(&instance, module));
        
        register_module(instance, "spectest");
    }
    __catch:
        return err;
}

// ref: https://github.com/WebAssembly/wabt/blob/main/docs/wast2json.md
// used only in test
#define TYPE_NAN_CANONICAL  (1<<7)
#define TYPE_NAN_ARITHMETIC (1<<7 | 1<<1)

static inline bool is_canonical_nan(arg_t *arg) {
    switch(arg->type) {
        case TYPE_NUM_F32:
            return (arg->val.num.i32 & 0x7fffffff) == 0x7fc00000;
        case TYPE_NUM_F64:
            return (arg->val.num.i64 & 0x7fffffffffffffff) == 0x7ff8000000000000;
    }
}

static inline bool is_arithmetic_nan(arg_t *arg) {
    switch(arg->type) {
        case TYPE_NUM_F32:
            return (arg->val.num.i32 & 0x00400000) == 0x00400000;
        case TYPE_NUM_F64:
            return (arg->val.num.i64 & 0x0008000000000000) == 0x0008000000000000;
    }
}

static void convert_to_arg(arg_t *arg, JSON_Object *obj) {
    const char *ty  =  json_object_get_string(obj, "type");
    const char *val = json_object_get_string(obj, "value");

    if(strcmp(ty, "i32") == 0) {
        arg->type           = TYPE_NUM_I32;
        arg->val.num.i32    = strtoimax(val, NULL, 0);
    }
    else if(strcmp(ty, "i64") == 0) {
        arg->type = TYPE_NUM_I64;
        arg->val.num.i64 = strtoumax(val, NULL, 0);
    }
    else if(strcmp(ty, "f32") == 0) {
        // ref: https://github.com/WebAssembly/wabt/blob/main/docs/wast2json.md
        if(strcmp(val, "nan:canonical") == 0)
            arg->type = TYPE_NAN_CANONICAL|TYPE_NUM_F32;
        else if(strcmp(val, "nan:arithmetic") == 0)
            arg->type = TYPE_NAN_ARITHMETIC|TYPE_NUM_F32;
        else {
            arg->type = TYPE_NUM_F32;
            arg->val.num.i32 = strtoimax(val, NULL, 0);
        }
    }
    else if(strcmp(ty, "f64") == 0) {
        // ref: https://github.com/WebAssembly/wabt/blob/main/docs/wast2json.md
        if(strcmp(val, "nan:canonical") == 0)
            arg->type = TYPE_NAN_CANONICAL|TYPE_NUM_F64;
        else if(strcmp(val, "nan:arithmetic") == 0)
            arg->type = TYPE_NAN_ARITHMETIC|TYPE_NUM_F64;
        else {
            arg->type = TYPE_NUM_F64;
            arg->val.num.i64 = strtoumax(val, NULL, 0);
        }
    }
    else if(strcmp(ty, "externref") == 0) {
        arg->type = TYPE_EXTENREF;
        if(strcmp(val, "null") == 0)
            arg->val.ref = -1;
        else
            arg->val.ref = strtoimax(val, NULL, 0);
    }
    else if(strcmp(ty, "funcref") == 0) {
        arg->type = TYPE_FUNCREF;
        if(strcmp(val, "null") == 0)
            arg->val.ref = -1;
        else
            arg->val.ref = strtoimax(val, NULL, 0);
    }
    else {
        PANIC("unknown arg type: %s", ty);
    }
    // todo: add here
}

static void convert_to_args(args_t *args, JSON_Array *array) {
    VECTOR_NEW(args, json_array_get_count(array));

    size_t idx = 0;
    VECTOR_FOR_EACH(arg, args) {
        convert_to_arg(arg, json_array_get_object(array, idx++));
    }
}

static error_t run_command(JSON_Object *command) {
    const char *type    = json_object_get_string(command, "type");
    const double line   = json_object_get_number(command, "line");
    __try {
        if(strcmp(type, "module") == 0) {
            // create new test context
            test_ctx_t *ctx = malloc(sizeof(test_ctx_t));
            module_t *module;
            // *.wasm files expected in the same directory as the json.
            const char *fpath = json_object_get_string(command, "filename");
            __throwiferr(decode_module_from_fpath(fpath, &module));

            const char *name = json_object_get_string(command, "name");
            if(name) {
                ctx->name = name;
                list_push_back(&test_ctxs, &ctx->link);
            }
            // validate
            __throwiferr(validate_module(module));

            // instantiate
            __throwiferr(instantiate(&ctx->instance, module));
            
            current_ctx = ctx;
        }
        else if(strcmp(type, "register") == 0) {
            const char *as    = json_object_get_string(command, "as");
            // todo: fix this
            register_module(current_ctx->instance, as);
        }
        else if(strcmp(type, "assert_return") == 0 || strcmp(type, "assert_trap") == 0 || \
                strcmp(type, "action") == 0  || strcmp(type, "assert_exhaustion") == 0) {
            
            JSON_Object *action = json_object_get_object(command, "action");
            const char *action_type = json_object_get_string(action, "type");

            if(strcmp(action_type, "invoke") == 0) {
                const char *module = json_object_get_string(action, "module");
                if(module) {
                    current_ctx = find_test_ctx(module);
                }

                const char *func = json_object_get_string(action, "field");

                funcaddr_t addr;
                __throwiferr(lookup_func_by_name(func, &addr));
                
                args_t args;
                convert_to_args(&args, json_object_get_array(action, "args"));

                if(strcmp(type, "assert_return") == 0) {
                    __throwiferr(invoke(current_ctx->instance, addr, &args));

                    args_t expects;
                    convert_to_args(&expects, json_object_get_array(command, "expected"));
                    
                    size_t idx = 0;
                    VECTOR_FOR_EACH(expect, &expects) {
                        // todo: validate type?
                        arg_t *ret = VECTOR_ELEM(&args, idx++);

                        switch(expect->type) {
                            case TYPE_NUM_I32:
                                __throwif(ERR_FAILED, ret->val.num.i32 != expect->val.num.i32);
                                break;
                            
                            case TYPE_NUM_I64:
                                __throwif(ERR_FAILED, ret->val.num.i64 != expect->val.num.i64);
                                break;
                            
                            case TYPE_NUM_F32|TYPE_NAN_ARITHMETIC:
                            case TYPE_NUM_F64|TYPE_NAN_ARITHMETIC:
                                __throwif(ERR_FAILED, !is_arithmetic_nan(ret));
                                break;
                            
                            case TYPE_NUM_F32|TYPE_NAN_CANONICAL:
                            case TYPE_NUM_F64|TYPE_NAN_CANONICAL:
                                __throwif(ERR_FAILED, !is_canonical_nan(ret));
                                break;
                            
                            // compare as bitpattern
                            case TYPE_NUM_F32:
                                __throwif(ERR_FAILED, ret->val.num.i32 != expect->val.num.i32);
                                break;
                            
                            case TYPE_NUM_F64:
                                __throwif(ERR_FAILED, ret->val.num.i64 != expect->val.num.i64);
                                break;
                            
                            case TYPE_EXTENREF:
                                __throwif(ERR_FAILED, ret->val.ref != expect->val.ref);
                                break;

                            case TYPE_FUNCREF:
                                 __throwif(ERR_FAILED, ret->val.ref != expect->val.ref);
                                break;
                            // todo: add here
                            default:
                                PANIC("unknown type: %x", expect->type);
                        }
                    }
                } else if(strcmp(type, "action") == 0) {
                    __throwiferr(invoke(current_ctx->instance, addr, &args));
                } else {
                    error_t ret = invoke(current_ctx->instance, addr, &args);
                    // check that invocation fails
                    __throwif(ERR_FAILED, !IS_ERROR(ret));

                    // check that error messagees match
                    __throwif(
                        ERR_FAILED, 
                        strstr(
                            json_object_get_string(command, "text"),
                            error_msg[-ret]
                        ) == NULL
                    );

                    // empty stack if assert_{exhaustion, trap}
                     current_ctx->instance->stack->idx = -1;
                }
            }
            else if(strcmp(action_type, "get") == 0) {
                const char *module = json_object_get_string(action, "module");
                if(module) {
                    current_ctx = find_test_ctx(module);
                }
                const char *field = json_object_get_string(action, "field");

                exportinst_t *exportinst = find_exportinst(field);

                args_t expects;
                convert_to_args(&expects, json_object_get_array(command, "expected"));
                
                val_t val = VECTOR_ELEM(&current_ctx->instance->store->globals, exportinst->value.global)->val;
                arg_t *expect = VECTOR_ELEM(&expects, 0);
                switch(expect->type) {
                    case TYPE_NUM_I32:
                        __throwif(ERR_FAILED, val.num.i32 != expect->val.num.i32);
                        break;

                    default:
                        PANIC("unknown type: %x", expect->type);
                }
            }
            else {
                PANIC("unknown action: %s", action_type);
            }
        }
        else if(strcmp(type, "assert_invalid") == 0) {
            module_t *module;

            const char *fpath = json_object_get_string(command, "filename");
            __throwiferr(decode_module_from_fpath(fpath, &module));

            // validate
            error_t ret = validate_module(module);
            __throwif(ERR_FAILED, !IS_ERROR(ret));

            // check that error messagees match
            __throwif(
                ERR_FAILED, 
                strstr(
                    json_object_get_string(command, "text"),
                    error_msg[-ret]
                ) == NULL
            );
        }
        else if(strcmp(type, "assert_malformed") == 0) {
            const char *module_type    = json_object_get_string(command, "module_type");
            if(strcmp(module_type, "binary") == 0) {
                module_t *module;
                const char *fpath = json_object_get_string(command, "filename");

                // check that decode fails
                error_t ret = decode_module_from_fpath(fpath, &module);
                __throwif(ERR_FAILED, !IS_ERROR(ret));

                // check that error messagees match
                __throwif(
                    ERR_FAILED, 
                    strstr(
                        json_object_get_string(command, "text"),
                        error_msg[-ret]
                    ) == NULL
                );
            }
        }
        else if(strcmp(type, "assert_unlinkable") == 0) {
            module_t *module;
            instance_t *instance;

            const char *fpath = json_object_get_string(command, "filename");
            __throwiferr(decode_module_from_fpath(fpath, &module));

            // validate
            __throwiferr(validate_module(module));

            // instantiate
            error_t ret = instantiate(&instance, module);
           
            __throwif(ERR_FAILED, !IS_ERROR(ret));

            // check that error messagees match
            __throwif(
                ERR_FAILED, 
                strstr(
                    json_object_get_string(command, "text"),
                    error_msg[-ret]
                ) == NULL
            );
        }
        // todo: add here
        else {
            WARN("Skip: type: %s, line: %0.f", type, line);
        }
    }
    __catch:
        if(IS_ERROR(err)) {
            ERROR("Faild: type: %s, line: %0.f", type, line);
        } else {
            INFO("Pass: type: %s, line: %0.f", type, line);
        }
        return err;
}

int main(int argc, char *argv[]) {
    test_ctx_t ctx = {};
    JSON_Value *root;

    __try {
        __throwiferr(register_imports());

        INFO("testsuite: %s", argv[1]);

        root = json_parse_file(argv[1]);
        __throwif(ERR_FAILED, !root);

        JSON_Object *obj = json_value_get_object(root);
        __throwif(ERR_FAILED, !obj);

        JSON_Array *commands = json_object_get_array(obj, "commands");
        __throwif(ERR_FAILED, !commands);

        for(int i = 0; i < json_array_get_count(commands); i++) {
            __throwiferr(run_command(json_array_get_object(commands, i)));
        }
    }
    __catch:
        // todo: cleanup
        return err;
}