#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <parson.h>

// ref: https://github.com/WebAssembly/wabt/blob/main/docs/wast2json.md

static void parse_const_vec(JSON_Array *vec) {
    JSON_Object *c;
    for(int i = 0; i < json_array_get_count(vec); i++) {
        c = json_array_get_object(vec, i);
        printf(
            " type: %s value: %s",
            json_object_get_string(c, "type"),
            json_object_get_string(c, "value")
        );
    }
}

static void parse_action(JSON_Object *action) {
    const char *type = json_object_get_string(action, "type");
    printf(
        " type: %s field: %s", 
        type,
        json_object_get_string(action, "field")
    );

    if(strcmp(type, "invoke") == 0) {
        printf(" args: [");
        parse_const_vec(json_object_get_array(action, "args"));
        printf("]");
    }
}

static void parse_command(JSON_Object *command) {
    const char *type = json_object_get_string(command, "type");
    
    printf(
        "type: %s line: %.0f", 
        type, 
        json_object_get_number(command, "line")
    );

    if(strcmp(type, "module") == 0) {
        printf(
            " filename: %s",
            json_object_get_string(command, "filename")
        );
    } 
    else if(strcmp(type, "assert_return") == 0) {
        parse_action(json_object_get_object(command, "action"));
        printf(" expected: [");
        parse_const_vec(json_object_get_array(command, "expected"));
        printf("]");
    }
    else if(strcmp(type, "assert_trap") == 0) {
        parse_action(json_object_get_object(command, "action"));
        printf(
            " text: %s",
            json_object_get_string(command, "text")
        );
    }
    else if(strcmp(type, "assert_invalid") == 0 || strcmp(type, "assert_malformed") == 0) {
        printf(
            " filename: %s text: %s module_type: %s",
            json_object_get_string(command, "filename"),
            json_object_get_string(command, "text"),
            json_object_get_string(command, "module_type")
        );
    }
    // todo: add here

    putchar('\n');
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        puts("Usage: ./runtest <JSON>");
        exit(0);
    }

    JSON_Value *root = json_parse_file(argv[1]);
    JSON_Object *obj = json_value_get_object(root);

    printf("source_filename: %s\n", json_object_get_string(obj, "source_filename"));

    JSON_Array *commands = json_object_get_array(obj, "commands");
    JSON_Object *command;

    for(int i = 0; i < json_array_get_count(commands); i++) {
        parse_command(json_array_get_object(commands, i));
    }

    // cleanup
    json_value_free(root);
}