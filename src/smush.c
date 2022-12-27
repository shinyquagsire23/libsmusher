#include "smush.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

smush_ctx* smush_from_fpath(const char* fpath)
{
    smush_ctx* ctx = (smush_ctx*)malloc(sizeof(smush_ctx));
    if (!ctx) return NULL;

    ctx->f = fopen(fpath, "rb");
    if (!ctx->f) {
        goto cleanup_1;
    }

    strncpy(ctx->fpath, fpath, sizeof(ctx->fpath));

    fread(&ctx->header, 1, sizeof(ctx->header), ctx->f);
    fread(&ctx->ahdr, 1, sizeof(ctx->ahdr), ctx->f);

    smush_header* hdr = &ctx->header;
    if(getbe32(hdr->magic) != SMUSH_MAGIC_ANIM) {
        printf("libsmush: Invalid magic! %c%c%c%c\n", hdr->magic[0], hdr->magic[1], hdr->magic[2], hdr->magic[3]);
        goto cleanup_2;
    }

    smush_ahdr* ahdr = &ctx->ahdr;
    if(getbe32(ahdr->magic) != SMUSH_MAGIC_AHDR) {
        printf("libsmush: Invalid magic! %c%c%c%c\n", ahdr->magic[0], ahdr->magic[1], ahdr->magic[2], ahdr->magic[3]);
        goto cleanup_2;
    }

    if (getle16(ahdr->version) == 2) {
        fread(&ctx->ahdr_ext, 1, sizeof(ctx->ahdr_ext), ctx->f);
        ctx->num_channels = 1;
    }
    else {
        putle16(ctx->ahdr_ext.frame_rate, 15);
        putle16(ctx->ahdr_ext.audio_rate, 11025);
        ctx->num_channels = 1;
    }

    ctx->start_fpos = sizeof(ctx->header) + sizeof(smush_header) + getbe32(ahdr->size);

    return ctx;

cleanup_1:
    free(ctx);
    return NULL;

cleanup_2:
    fclose(ctx->f);
    free(ctx);
    return NULL;
}

void smush_print(smush_ctx* ctx)
{
    printf("%s:\n", ctx->fpath);

    smush_header* hdr = &ctx->header;
    printf("  Header:\n");
    printf("    Magic: %c%c%c%c\n", hdr->magic[0], hdr->magic[1], hdr->magic[2], hdr->magic[3]);
    printf("    Size: 0x%x\n", getbe32(hdr->size));

    smush_ahdr* ahdr = &ctx->ahdr;
    printf("  AHDR:\n");
    printf("    Magic: %c%c%c%c\n", ahdr->magic[0], ahdr->magic[1], ahdr->magic[2], ahdr->magic[3]);
    printf("    Size: 0x%x\n", getbe32(ahdr->size));
    printf("    Version: 0x%x\n", getle16(ahdr->version));
    printf("    Num Frames: %u\n", getle16(ahdr->num_frames));
    printf("    Unk1: %u\n", getle16(ahdr->unk1));

    if (getle16(ahdr->version) == 2) {
        smush_ahdr_ext* ahdr_ext = &ctx->ahdr_ext;

        printf("  AHDR Ext:\n");
        printf("    Frame Rate: %u fps\n", getle32(ahdr_ext->frame_rate));
        printf("    Unk2: 0x%x\n", getle32(ahdr_ext->unk2));
        printf("    Audio Rate: %u\n", getle32(ahdr_ext->audio_rate));
    }

    uint32_t seek_pos = ctx->start_fpos;
    
    while (1)
    {
        smush_header tmp;
        
        fseek(ctx->f, seek_pos, SEEK_SET);
        if(fread(&tmp, 1, sizeof(tmp), ctx->f) <= 0) break;

        if(getbe32(tmp.magic) == SMUSH_MAGIC_FRME) {
            smush_print_frme(ctx, seek_pos, getbe32(tmp.size));
        }
        else {
            printf("  Tag @ 0x%x:\n", seek_pos);
            printf("    Magic: %c%c%c%c\n", tmp.magic[0], tmp.magic[1], tmp.magic[2], tmp.magic[3]);
            printf("    Size: 0x%x\n", getbe32(tmp.size));
        }

        seek_pos += 8;
        seek_pos += getbe32(tmp.size);
    }
}

void smush_print_frme(smush_ctx* ctx, uint32_t seek_pos, uint32_t total_size)
{
    smush_header tmp;
    smush_fobj fobj;
        
    fseek(ctx->f, seek_pos, SEEK_SET);
    if(fread(&tmp, 1, sizeof(tmp), ctx->f) <= 0) return;

    printf("  Frame Header @ 0x%x:\n", seek_pos);
    printf("    Magic: %c%c%c%c\n", tmp.magic[0], tmp.magic[1], tmp.magic[2], tmp.magic[3]);
    printf("    Size: 0x%x\n\n", getbe32(tmp.size));

    if(fread(&fobj, 1, sizeof(fobj), ctx->f) <= 0) return;

    printf("    Magic: %c%c%c%c\n", fobj.magic[0], fobj.magic[1], fobj.magic[2], fobj.magic[3]);
    printf("    Size: 0x%x\n", getbe32(fobj.size));
    printf("    Codec: %u\n", fobj.codec);
    printf("    Codec Param: 0x%x\n", fobj.codec_param);
    printf("    Xpos: %d\n", getles16(fobj.x));
    printf("    Ypos: %d\n", getles16(fobj.y));
    printf("    Width: %u\n", getle16(fobj.width));
    printf("    Height: %u\n", getle16(fobj.height));
    printf("    Unk3: 0x%x\n", getle16(fobj.unk3));
    printf("    Unk4: 0x%x\n", getle16(fobj.unk4));
}