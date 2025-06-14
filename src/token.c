#include "token.h"

#define INITIAL_CAPACITY 64

TokenVec* new_tokenvec() {
    TokenVec* vec = malloc(sizeof(TokenVec));

    vec->size = 0;
    vec->capacity = INITIAL_CAPACITY;
    vec->vector = malloc(sizeof(Token) * INITIAL_CAPACITY);
    assert(vec->vector != NULL);

    return vec;
}

void append_token(TokenVec* vec, Token token) {
    if (vec->size + 1 >= vec->capacity) {
        vec->capacity *= 2;
        vec->vector = realloc(vec->vector, sizeof(Token) * vec->capacity);
        assert(vec->vector != NULL);
    }

    vec->vector[vec->size] = token;
    vec->size += 1;
}

void print_tokenvec(TokenVec* vec) {
    for (size_t i = 0; i < vec->size; ++i) {
        TokenType type = vec->vector[i].type;
        char* string = vec->vector[i].content->items;

        switch (type) {
            case LeftParen: printf("("); break;
            case RightParen: printf(")"); break;
            case LeftBracket: printf("{"); break;
            case RightBracket: printf("}"); break;
            case SemiColon: printf(";"); break;
            case Greater: printf(">"); break;
            case Less: printf("<"); break;
            default: { printf("type: %d, %s", type, string); }
        }

        printf("\n");
    }
}

void free_tokenvec(TokenVec* vec) {
    for (size_t i = 0; i < vec->size; ++i) {
        if (vec->vector[i].content != NULL) sb_free(*vec->vector[i].content);
        free(vec->vector[i].content);
    }

    free(vec->vector);
    free(vec);
}
