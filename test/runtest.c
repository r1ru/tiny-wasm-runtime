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
    list_elem_t     link;
    const char      *name;
    const char      *export_name;
    moduleinst_t    *moduleinst;
} test_module_t;

static test_module_t *current_test_module = NULL;

static store_t *S = NULL;

list_t test_modules = {.prev = &test_modules, .next = &test_modules};
list_t exported_modules = {.prev = &exported_modules, .next = &exported_modules};

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
static test_module_t *new_test_module(const char *name, const char *export_name, moduleinst_t *moduleinst) {
    test_module_t *test_module = malloc(sizeof(test_module_t));
    list_elem_init(&test_module->link);
    test_module->name = name;
    test_module->export_name = export_name;
    test_module->moduleinst = moduleinst;
    return test_module;
}

static test_module_t *find_test_module(const char *name) {
    LIST_FOR_EACH(module, &test_modules, test_module_t, link) {
        if(strcmp(module->name, name) == 0) {
            return module;
        }
    }
    return NULL;
}

static test_module_t *find_exported_module(const char *name) {
    LIST_FOR_EACH(module, &test_modules, test_module_t, link) {
        if(strcmp(module->export_name, name) == 0) {
            return module;
        }
    }
    return NULL;
}

static externval_t find_export(test_module_t *from, const char *name) {
    VECTOR_FOR_EACH(exportinst, &from->moduleinst->exports) {
        if(strcmp(exportinst->name, name) == 0) {
            return exportinst->value;
        }
    }
    printf("%s not found\n", name);
    exit(1);
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
        default:
            return false;
    }
}

static inline bool is_arithmetic_nan(arg_t *arg) {
    switch(arg->type) {
        case TYPE_NUM_F32:
            return (arg->val.num.i32 & 0x00400000) == 0x00400000;
        case TYPE_NUM_F64:
            return (arg->val.num.i64 & 0x0008000000000000) == 0x0008000000000000;
        default:
            return false;
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
    VECTOR_NEW(args, json_array_get_count(array),  json_array_get_count(array));

    size_t idx = 0;
    VECTOR_FOR_EACH(arg, args) {
        convert_to_arg(arg, json_array_get_object(array, idx++));
    }
}

// todo: return err?
static void resolve_imports(module_t *module, externvals_t *externvals) {
    VECTOR_NEW(externvals, 0, module->imports.len);

    VECTOR_FOR_EACH(import, &module->imports) {
        test_module_t *from = find_exported_module(import->module);
        externval_t externval = find_export(from, import->name);
       
        VECTOR_APPEND(externvals, externval);
    }
}

static error_t run_command(JSON_Object *command) {
    const char *type    = json_object_get_string(command, "type");
    const double line   = json_object_get_number(command, "line");
    __try {
        if(strcmp(type, "module") == 0) {
            module_t *module;
            moduleinst_t *moduleinst;

            // decode
            const char *fpath = json_object_get_string(command, "filename");
            __throwiferr(decode_module_from_fpath(fpath, &module));

            // validate
            __throwiferr(validate_module(module));
            
            // create store if not exist
            if(!S) {
                S = new_store();
            }
            
            // resolve imports
            externvals_t externvals;
            resolve_imports(module, &externvals);

            // instantiate
            __throwiferr(instantiate(S, module, &externvals, &moduleinst));

            // create test module
            const char *name = json_object_get_string(command, "name");
            if(name) {
               current_test_module = new_test_module(name, "", moduleinst);
               list_push_back(&test_modules, &current_test_module->link);
            }
            else {
                current_test_module = new_test_module("", "", moduleinst);
            }
        }
        else if(strcmp(type, "assert_return") == 0 || strcmp(type, "assert_trap") == 0 || \
                strcmp(type, "action") == 0  || strcmp(type, "assert_exhaustion") == 0) {
            
            JSON_Object *action = json_object_get_object(command, "action");
            const char *action_type = json_object_get_string(action, "type");

            if(strcmp(action_type, "invoke") == 0) {
                const char *module = json_object_get_string(action, "module");
                if(module) {
                    current_test_module = find_test_module(module);
                }

                const char *func = json_object_get_string(action, "field");

                externval_t externval = find_export(current_test_module, func);
                __throwif(ERR_FAILED, externval.kind != FUNC_EXPORTDESC);

                args_t args;
                convert_to_args(&args, json_object_get_array(action, "args"));

                if(strcmp(type, "assert_return") == 0) {
                    __throwiferr(invoke(S, externval.func, &args));

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
                    __throwiferr(invoke(S, externval.func, &args));
                } else {
                    error_t ret = invoke(S, externval.func, &args);
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
                    S->stack->idx = -1;
                }
            }
            else if(strcmp(action_type, "get") == 0) {
                const char *module = json_object_get_string(action, "module");
                if(module) {
                    current_test_module = find_test_module(module);
                }
                const char *field = json_object_get_string(action, "field");

                externval_t externval = find_export(current_test_module, field);
                __throwif(ERR_FAILED, externval.kind != EXTERN_GLOBAL);

                args_t expects;
                convert_to_args(&expects, json_object_get_array(command, "expected"));
                
                val_t val = VECTOR_ELEM(&S->globals, externval.global)->val;
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
            moduleinst_t *moduleinst;

            const char *fpath = json_object_get_string(command, "filename");
            __throwiferr(decode_module_from_fpath(fpath, &module));

            // validate
            __throwiferr(validate_module(module));

            // resolve imports
            externvals_t externvals;
            resolve_imports(module, &externvals);

            // check that instantiation fails
            error_t ret = instantiate(S, module, &externvals, &moduleinst);
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
        else if(strcmp(type, "register") == 0) {
            const char *as = json_object_get_string(command, "as");
            current_test_module->export_name = as;
            if(!list_is_linked(&current_test_module->link)) {
                list_push_back(&test_modules, &current_test_module->link);
            }
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
    JSON_Value *root;

    __try {
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