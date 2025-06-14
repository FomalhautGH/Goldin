#include "token.h"
#include "lexer.h"

#define FILE_NOT_FOUND 3
#define WRONG_USAGE 2

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: au INPUT\n");
        exit(WRONG_USAGE);
    }

    init_hm_keywords();

    String_Builder content = {0};
    if (!read_entire_file(argv[1], &content)) exit(FILE_NOT_FOUND);

    init_lexer(&content);
    TokenVec* vec = parse();

    print_tokenvec(vec);

    free_tokenvec(vec);
    sb_free(content);
    free_hm_keywords();
    exit(0);
}
