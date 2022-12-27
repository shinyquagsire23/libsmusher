#include <stdlib.h>
#include <stdio.h>

#include "smush.h"
#include "endian.h"

int main(int argc, char** argv)
{
    if (argc <= 1) {
        printf("Usage: %s <movie.san>\n", argv[0]);
        return -1;
    }

    smush_ctx* ctx = smush_from_fpath(argv[1]);
    if (!ctx) {
        printf("Failed to open file `%s`!\n", argv[1]);
        return -1;
    }

    smush_print(ctx);

    printf("Hello there\n");
    return 0;
}