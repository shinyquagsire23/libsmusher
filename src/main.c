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

    smush_frame(ctx);
    //smush_print(ctx);

    FILE* f = fopen("test.bin", "wb");
    for (int i = 0; i < 640*480; i++)
    {
        uint8_t val = ctx->framebuffer[i];
        fwrite(&ctx->ahdr.palette[val * 3], 3, 1, f);
    }
    fclose(f);

    printf("Hello there\n");
    return 0;
}