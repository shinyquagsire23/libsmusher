#include "codec48.h"

#include <string.h>
#include <stdlib.h>

void codec48_proc(smush_ctx* parent_ctx, const uint8_t* data)
{
    codec48_hdr* hdr = (codec48_hdr*)data;
    codec48_ctx* ctx = parent_ctx->c48_ctx;

    if (!ctx) {
        ctx = (codec48_ctx*)malloc(sizeof(codec48_ctx));
        memset(ctx, 0, sizeof(*ctx));
        parent_ctx->c48_ctx = ctx;

        ctx->width = parent_ctx->codec_w;
        ctx->height = parent_ctx->codec_h;
        ctx->block_x = (parent_ctx->codec_w + 7) / 8;
        ctx->block_y = (parent_ctx->codec_h + 7) / 8;
        ctx->pitch = ctx->block_x * 8;

        ctx->frame_size = 640*480;
        ctx->delta_buf[0] = malloc(ctx->frame_size*2);
        ctx->delta_buf[1] = ctx->delta_buf[0] + ctx->frame_size;
    }
    
    printf("  Codec 48:\n");
    printf("    Type: 0x%02x\n", hdr->type);
    printf("    Table Idx: 0x%02x\n", hdr->table_index);
    printf("    Seq num: 0x%04x\n", getle16(hdr->seq_num));
    printf("    Unk2: 0x%04x\n", getle32(hdr->unk2));
    printf("    Unk3: 0x%04x\n", getle32(hdr->unk3));
    printf("    Flags: 0x%04x\n", getle32(hdr->flags));

    data += sizeof(codec48_hdr);

    if (getle32(hdr->flags) & C48_INTERPOLATION_FLAG) {
        for (int i = 0; i < 256; i++)
        {
            uint8_t* pA = ctx->interpolation_table + i;
            uint8_t* pB = ctx->interpolation_table + i;
            for (int j = 256 - i; j > 0; j--) {
                uint8_t val = *data++;
                *pB = val;
                *pA++ = val;
                pB += 256;
            }
            pA += 256;
        }
    }

    if (hdr->type == 0) {
        memcpy(ctx->delta_buf[ctx->cur_buf], data, getle32(hdr->unk2));
    }
    else if (hdr->type == 2) {
        codec48_proc_block2(ctx, data, ctx->width * ctx->height, ctx->delta_buf[ctx->cur_buf]);
    }
    else if (hdr->type == 3) {
        printf("TODO block 3\n");
    }
    else if (hdr->type == 5) {
        printf("TODO block 5\n");
    }
    else {
        printf("Unknown block type %x\n", hdr->type);
    }

    memcpy(parent_ctx->framebuffer, ctx->delta_buf[ctx->cur_buf], ctx->pitch * ctx->height);
}

void codec48_proc_block2(codec48_ctx* ctx, const uint8_t* data, uint32_t len, uint8_t* out)
{
    while (len > 0)
    {
        uint8_t packed = *data++;
        uint8_t num = (packed >> 1) + 1;

        if (num > len) {
            num = len;
        }

        if (packed & 1) {
            memset(out, *data++, num);
        }
        else {
            memcpy(out, data, num);
            data += num;
        }

        out += num;
        len -= num;
    }
}