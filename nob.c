#define NOB_IMPLEMENTATION

#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"

int main(int argc, char *argv[]) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  Nob_Cmd cmd = {0};
#if !defined(_MSC_VER)
  nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o",
                 BUILD_FOLDER "blog-builder", SRC_FOLDER "main.c");
#else
  nob_cmd_append(&cmd, "cl", "-I.", "/Fe:" BUILD_FOLDER "blog-builder",
                 SRC_FOLDER "main.c");
#endif

  if (!nob_cmd_run(&cmd))
    return 1;

  return 0;
}
