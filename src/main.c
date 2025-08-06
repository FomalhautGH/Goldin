#include "token.h"
#include "lexer.h"
#include "compiler.h"
#include "codegen.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define NOB_STRIP_PREFIX
#include "nob.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

#define WRONG_USAGE 2
#define FILE_NOT_FOUND 3
#define GEN_ERROR 4
#define FAILED_CMD 5
#define COMPILATION_ERROR 6

static void print_usage(FILE* stream, const char* exe_name) {
    fprintf(stderr, "Usage: %s [OPTIONS] <inputs...> \n", exe_name);
    fprintf(stderr, "OPTIONS: \n");
    flag_print_options(stream);
}

int main(int argc, char** argv) {
    const char* exe = argv[0];

    char **output_file = flag_str("o", "a.out", "output file");
    bool *help = flag_bool("help", false, "Print this help to stdout and exit with 0");
    char **library = flag_str("l", NULL, "library to link to");

    if (!flag_parse(argc, argv)) {
        print_usage(stderr, exe);
        flag_print_error(stderr);
        exit(WRONG_USAGE);
    }

    if (*help) {
        print_usage(stdout, exe);
        exit(EXIT_SUCCESS);
    }

    int rest_argc = flag_rest_argc();
    char **rest_argv = flag_rest_argv();

    if (rest_argc <= 0) {
        print_usage(stderr, exe);
        fprintf(stderr, "ERROR: no input file was provided\n");
        exit(WRONG_USAGE);
    }

    const char* file_name = rest_argv[0];
    if (!init_lexer(file_name)) exit(FILE_NOT_FOUND);

    init_compiler();
    if (!generate_ops()) exit(COMPILATION_ERROR);

    Op* ops = get_ops();
    Arg* data = get_data();
    String_Builder result = {0};
    if (!generate_GAS_x86_64(&result, ops, data)) exit(GEN_ERROR);

    String_Builder asm_file = {0};
    sb_appendf(&asm_file, "%s.asm", file_name);
    sb_append_null(&asm_file);
    write_entire_file(asm_file.items, result.items, result.count);

    Cmd assemble = {0};
    nob_cc(&assemble);
    if (*library != NULL) nob_cmd_append(&assemble, "-l", *library);
    nob_cc_output(&assemble, *output_file);
    nob_cc_inputs(&assemble, asm_file.items);
    if (!cmd_run_sync_and_reset(&assemble)) exit(FAILED_CMD);

    free_lexer();
    free_compiler();
    sb_free(result);
    sb_free(asm_file);
    cmd_free(assemble);

    exit(EXIT_SUCCESS);
}
