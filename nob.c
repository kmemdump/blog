#define NOB_IMPLEMENTATION

#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"

int main(int argc, char* argv[]) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    const char* output = BUILD_FOLDER "blog-builder";
    const char* inputs[] = {
        SRC_FOLDER "main.c",   SRC_FOLDER "tinytemplate.c", SRC_FOLDER "tinytemplate.h", SRC_FOLDER "md4c.c",
        SRC_FOLDER "md4c.h",   SRC_FOLDER "md4c-html.c",    SRC_FOLDER "md4c-html.h",    SRC_FOLDER "entity.c",
        SRC_FOLDER "entity.h", SRC_FOLDER "xml.h",
    };

    int rebuild_is_needed = nob_needs_rebuild(output, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild_is_needed < 0) return 1;
    if (!rebuild_is_needed) return 0;

    Nob_Cmd cmd = {0};
#if !defined(_MSC_VER)
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", output, SRC_FOLDER "main.c", SRC_FOLDER "tinytemplate.c",
                   SRC_FOLDER "md4c.c", SRC_FOLDER "md4c-html.c", SRC_FOLDER "entity.c");
#else
    nob_cmd_append(&cmd, "cl", "-I.", "/Fe:" BUILD_FOLDER "blog-builder", SRC_FOLDER "main.c",
                   SRC_FOLDER "tinytemplate.c", SRC_FOLDER "md4c.c", SRC_FOLDER "md4c-html.c", SRC_FOLDER "entity.c");
#endif

    if (!nob_cmd_run(&cmd)) return 1;

    return 0;
}
