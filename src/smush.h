#ifndef _LIBSMUSHER_SMUSH_H
#define _LIBSMUSHER_SMUSH_H

#include "endian.h"

#include <stdio.h>

typedef FILE* filehandle_t;

typedef struct smush_header
{
    uint8_t magic[4];
    uint8_t size[4];
} smush_header;

typedef struct smush_ahdr
{
    uint8_t magic[4];
    uint8_t size[4];
    uint8_t version[2];
    uint8_t num_frames[2];
    uint8_t unk1[2];
    uint8_t palette[256 * 3];
} smush_ahdr;

typedef struct smush_ahdr_ext
{
    uint8_t frame_rate[4];
    uint8_t unk2[4];
    uint8_t audio_rate[4];
} smush_ahdr_ext;

typedef struct smush_fobj
{
    uint8_t magic[4];
    uint8_t size[4];
    uint8_t codec;
    uint8_t codec_param;
    uint8_t x[2];
    uint8_t y[2];
    uint8_t width[2];
    uint8_t height[2];
    uint8_t unk3[2];
    uint8_t unk4[2];
} smush_fobj;


typedef struct codec48_ctx codec48_ctx; 
typedef struct smush_ctx
{
    char fpath[512];
    filehandle_t f;
    smush_header header;
    smush_ahdr ahdr;
    smush_ahdr_ext ahdr_ext;

    uint32_t start_fpos;
    uint32_t frame_fpos;
    uint8_t num_channels;

    codec48_ctx* c48_ctx;
    uint8_t* framebuffer;

    int16_t codec_x;
    int16_t codec_y;
    uint16_t codec_w;
    uint16_t codec_h;
} smush_ctx;

// BE32
#define SMUSH_MAGIC_ANIM (0x414e494d)
#define SMUSH_MAGIC_AHDR (0x41484452)
#define SMUSH_MAGIC_FRME (0x46524D45)

smush_ctx* smush_from_fpath(const char* fpath);

void smush_frame(smush_ctx* ctx);
void smush_proc_frme(smush_ctx* ctx, uint32_t seek_pos, uint32_t total_size);

void smush_print(smush_ctx* ctx);
void smush_print_frme(smush_ctx* ctx, uint32_t seek_pos, uint32_t total_size);

#endif // _LIBSMUSHER_SMUSH_H