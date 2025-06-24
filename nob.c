#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define SRC_DIR "./src/"
#define BUILD_DIR "./build/"
#define EXAMPLES_DIR "./examples/"

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Cmd cmd = {0};
    mkdir_if_not_exists(BUILD_DIR);

    nob_cc(&cmd);
    cmd_append(&cmd, "-ggdb");
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, BUILD_DIR"au");
    nob_cc_inputs(&cmd, SRC_DIR"token.c");
    nob_cc_inputs(&cmd, SRC_DIR"lexer.c");
    nob_cc_inputs(&cmd, SRC_DIR"main.c");
    if (!cmd_run_sync_and_reset(&cmd)) return 1;

    cmd_append(&cmd, BUILD_DIR"au");
    nob_cc_output(&cmd, BUILD_DIR"hello");
    nob_cc_inputs(&cmd, EXAMPLES_DIR"hello.gdn");
    if (!cmd_run_sync_and_reset(&cmd)) return 1;

    cmd_append(&cmd, BUILD_DIR"hello");
    if (!cmd_run_sync_and_reset(&cmd)) return 1;

    return 0;
}
