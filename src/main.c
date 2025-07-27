#include "token.h"
#include "lexer.h"
#include "compiler.h"
#include "codegen.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define NOB_STRIP_PREFIX
#include "nob.h"

#define WRONG_USAGE 2
#define FILE_NOT_FOUND 3
#define GEN_ERROR 4
#define FAILED_CMD 5
#define COMPILATION_ERROR 6

static void print_usage(char** argv) {
    fprintf(stderr, "Usage: %s [OPTIONS] <inputs...> \n", argv[0]);
    fprintf(stderr, "OPTIONS: \n");
    fprintf(stderr, "    -o\n");
    fprintf(stderr, "        Output path\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv);
        exit(WRONG_USAGE);
    }

    int file_name = 1;

    String_Builder output_path = {0};
    if (strcmp(argv[1], "-o") == 0) {
        sb_appendf(&output_path, "%s", argv[2]);
        file_name = 3;
    }

    if (!init_lexer(argv[file_name])) {
        exit(FILE_NOT_FOUND);
    }

    init_compiler();
    if (!generate_ops()) exit(COMPILATION_ERROR);

    String_Builder result = {0};

    Op* ops = get_ops();
    Arg* data = get_data();
    if (!generate_GAS_x86_64(&result, ops, data)) exit(GEN_ERROR);

    String_Builder asm_file = {0};
    sb_appendf(&asm_file, "%s.asm", argv[file_name]);
    sb_append_null(&asm_file);
    FILE* assembly = fopen(asm_file.items, "w+");
    fprintf(assembly, "%s", result.items);
    fclose(assembly);

    if (output_path.count > 0) {
        Cmd compile = {0};
        nob_cc(&compile);
        nob_cc_output(&compile, output_path.items);
        nob_cc_inputs(&compile, asm_file.items);
        if (!cmd_run_sync_and_reset(&compile)) exit(FAILED_CMD);
    }

    sb_free(asm_file);
    sb_free(result);
    free_lexer();
    free_compiler();
    exit(0);
}
