/* ============================================================================
 * stone_build.c — Stone Multi-Target Build Wrapper
 *
 * Usage: stone build input.st --target python|javascript|lua|c
 *
 * Dispatches to the correct backend.
 * All targets share the same Stone grammar.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *input = NULL;
    const char *target = "c";
    const char *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            target = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (!input) {
            input = argv[i];
        }
    }

    if (!input) {
        fprintf(stderr, "Usage: %s <input.st> --target <target> [-o output]\n", argv[0]);
        fprintf(stderr, "Targets: c, python, javascript, lua\n");
        return 1;
    }

    const char *backend = NULL;
    if (strcmp(target, "c") == 0)        backend = "./stone";
    else if (strcmp(target, "python") == 0)    backend = "./stone2py";
    else if (strcmp(target, "javascript") == 0) backend = "./stone2js";
    else if (strcmp(target, "lua") == 0)       backend = "./stone2lua";
    else {
        fprintf(stderr, "Unknown target: %s\n", target);
        fprintf(stderr, "Targets: c, python, javascript, lua\n");
        return 1;
    }

    char cmd[4096];
    if (output) {
        snprintf(cmd, sizeof(cmd), "%s %s > %s", backend, input, output);
    } else {
        snprintf(cmd, sizeof(cmd), "%s %s", backend, input);
    }

    return system(cmd);
}
