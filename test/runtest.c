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
    int         fd;
    size_t      size;
    uint8_t     *map;
    module_t    *mod;
    store_t     *store;
} test_ctx_t;

static const char *error_msg[] = {
    [-ERR_SUCCESS]                      = "",
    [-ERR_FAILED]                       = "",
    [-ERR_TYPE_MISMATCH]                = "type mismatch",
    [-ERR_UNKNOWN_LOCAL]                = "unknown local",
    [-ERR_UNKNOWN_LABEL]                = "unknown label",
    [-ERR_UNKNOWN_FUNC]                 = "unknown function",
    [-ERR_TRAP_INTERGER_DIVIDE_BY_ZERO] = "integer divide by zero",
    [-ERR_TRAP_INTERGET_OVERFLOW]       = "integer overflow"
};

// helpers
// todo: consider the case where funcidx != funcaddr
error_t lookup_func_by_name(funcaddr_t *addr, const char *name, module_t *mod) {
    VECTOR_FOR_EACH(export, &mod->exports, export_t) {
        if(strcmp(export->name, name) == 0 && export->exportdesc.kind == 0) {
            *addr = export->exportdesc.funcidx;
            return ERR_SUCCESS;
        }
    }
    return ERR_FAILED;
}

error_t map_file(test_ctx_t *ctx, const char *fpath) {
    __try {
        // open and map file
        ctx->fd = open(fpath, O_RDONLY);
        __throwif(ERR_FAILED, ctx->fd == -1);
        struct stat s;
        __throwif(ERR_FAILED, fstat(ctx->fd, &s) == -1);
        ctx->size = s.st_size;
        ctx->map = mmap(
            NULL,
            ctx->size,
            PROT_READ,
            MAP_PRIVATE,
            ctx->fd, 
            0
        );
        __throwif(ERR_FAILED, ctx->map == MAP_FAILED);
    }
    __catch:
        return err;
}

void destroy_test_ctx(test_ctx_t *ctx) {
    free(ctx->mod);
    free(ctx->store);
    munmap(ctx->map, ctx->size);
    close(ctx->fd);
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
        arg->val.num.i32 = strtoimax(val, NULL, 0);
    }
    else {
        PANIC("unknown arg type: %s", ty);
    }
    // todo: add here
}

static void convert_to_args(args_t *args, JSON_Array *array) {
    VECTOR_INIT(args, json_array_get_count(array), arg_t);

    size_t idx = 0;
    VECTOR_FOR_EACH(arg, args, arg_t) {
        convert_to_arg(arg, json_array_get_object(array, idx++));
    }
}

static error_t run_command(test_ctx_t *ctx, JSON_Object *command) {
    const char *type    = json_object_get_string(command, "type");
    const double line   = json_object_get_number(command, "line");
    __try {
        if(strcmp(type, "module") == 0) {
            // *.wasm files expected in the same directory as the json.
            const char *fpath = json_object_get_string(command, "filename");
            __throwif(ERR_FAILED, map_file(ctx, fpath));
            
            // decode
            __throwif(
                ERR_FAILED, IS_ERROR(
                    decode_module(
                        &ctx->mod, ctx->map, ctx->size
                    )
                )
            );

            // validate
            __throwif(ERR_FAILED, IS_ERROR(validate_module(ctx->mod)));

            // instantiate
            __throwif(ERR_FAILED, IS_ERROR(instantiate(&ctx->store, ctx->mod)));
        }
        else if(strcmp(type, "assert_return") == 0 || strcmp(type, "assert_trap") == 0) {
            JSON_Object *action = json_object_get_object(command, "action");
            const char *action_type = json_object_get_string(action, "type");

            if(strcmp(action_type, "invoke") == 0) {
                const char *func = json_object_get_string(action, "field");

                funcaddr_t addr;
                __throwif(
                    ERR_FAILED, 
                    IS_ERROR(
                        lookup_func_by_name(&addr, func, ctx->mod)
                    )
                );
                
                args_t args;
                convert_to_args(&args, json_object_get_array(action, "args"));

                if(strcmp(type, "assert_return") == 0) {
                    __throwif(ERR_FAILED, IS_ERROR(invoke(ctx->store, addr, &args)));

                    args_t expects;
                    convert_to_args(&expects, json_object_get_array(command, "expected"));
                    
                    size_t idx = 0;
                    VECTOR_FOR_EACH(expect, &expects, arg_t) {
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
                            // todo: add here
                        }
                    }
                } else {
                    error_t err = invoke(ctx->store, addr, &args);
                    // check that invocation fails
                    __throwif(ERR_FAILED, !IS_ERROR(err));

                    // check that error messagees match
                    __throwif(
                        ERR_FAILED, 
                        strcmp(
                            error_msg[-err], 
                            json_object_get_string(command, "text")
                        ) != 0
                    );
                }
            }
        }
        else if(strcmp(type, "assert_invalid") == 0) {
            const char *fpath = json_object_get_string(command, "filename");
            __throwif(ERR_FAILED, map_file(ctx, fpath));
            
            // decode
            __throwif(
                ERR_FAILED, IS_ERROR(
                    decode_module(
                        &ctx->mod, ctx->map, ctx->size
                    )
                )
            );

            // check that validation fails
            error_t ret = validate_module(ctx->mod);
            __throwif(ERR_FAILED, !IS_ERROR(ret));
            // check that error messagees match
            __throwif(
                ERR_FAILED, 
                strcmp(
                    error_msg[-ret], 
                    json_object_get_string(command, "text")
                ) != 0
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
        INFO("testsuite: %s", argv[1]);

        root = json_parse_file(argv[1]);
        __throwif(ERR_FAILED, !root);

        JSON_Object *obj = json_value_get_object(root);
        __throwif(ERR_FAILED, !obj);

        JSON_Array *commands = json_object_get_array(obj, "commands");
        __throwif(ERR_FAILED, !commands);

        for(int i = 0; i < json_array_get_count(commands); i++) {
            __throwif(ERR_FAILED, IS_ERROR(
                run_command(&ctx, json_array_get_object(commands, i))
            ));
        }
    }
    __catch:
        // cleanup
        json_value_free(root);
        destroy_test_ctx(&ctx);

        return err;
}