#ifndef _LIBSMUSHER_CODEC48_H
#define _LIBSMUSHER_CODEC48_H

#include "smush.h"

typedef struct codec48_hdr
{
    uint8_t type;
    uint8_t table_index;
    uint8_t seq_num[2];
    uint8_t unk2[4];

    uint8_t unk3[4];
    uint8_t flags[4];

} codec48_hdr;

typedef struct codec48_ctx
{
    uint32_t width;
    uint32_t height;
    
    uint32_t pitch;
    uint32_t block_x;
    uint32_t block_y;
    uint32_t frame_size;

    uint8_t cur_buf;

    uint8_t* delta_buf[2];

    uint8_t offset_table[256];
    uint8_t interpolation_table[65536];
} codec48_ctx;

#define C48_INTERPOLATION_FLAG (0x0008)

void codec48_proc(smush_ctx* ctx, const uint8_t* data);
void codec48_proc_block2(codec48_ctx* ctx, const uint8_t* data, uint32_t len, uint8_t* out);

#endif // _LIBSMUSHER_CODEC48_H