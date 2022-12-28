#include "smush.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "codec48.h"

int _smush_debug_prints = 0;

smush_ctx* smush_from_fpath(const char* fpath)
{
    smush_ctx* ctx = (smush_ctx*)malloc(sizeof(smush_ctx));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(*ctx));

    ctx->framebuffer = malloc(640*480*sizeof(uint8_t));
    memset(ctx->framebuffer, 0, 640*480*sizeof(uint8_t));
    ctx->c48_ctx = NULL;

    ctx->f = fopen(fpath, "rb");
    if (!ctx->f) {
        goto cleanup_1;
    }

    strncpy(ctx->fpath, fpath, sizeof(ctx->fpath));

    fread(&ctx->header, 1, sizeof(ctx->header), ctx->f);
    fread(&ctx->ahdr, 1, sizeof(ctx->ahdr), ctx->f);

    smush_header* hdr = &ctx->header;
    if(getbe32(hdr->magic) != SMUSH_MAGIC_ANIM) {
        smush_error("libsmush: Invalid magic! %c%c%c%c\n", hdr->magic[0], hdr->magic[1], hdr->magic[2], hdr->magic[3]);
        goto cleanup_2;
    }

    smush_ahdr* ahdr = &ctx->ahdr;
    if(getbe32(ahdr->magic) != SMUSH_MAGIC_AHDR) {
        smush_error("libsmush: Invalid magic! %c%c%c%c\n", ahdr->magic[0], ahdr->magic[1], ahdr->magic[2], ahdr->magic[3]);
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

    memcpy(ctx->palette, ctx->ahdr.palette, sizeof(ctx->palette));

    ctx->start_fpos = sizeof(ctx->header) + sizeof(smush_header) + getbe32(ahdr->size);
    ctx->max_fpos = sizeof(ctx->header) + getbe32(hdr->size);
    ctx->frame_fpos = ctx->start_fpos;

    return ctx;

cleanup_1:
    free(ctx);
    return NULL;

cleanup_2:
    fclose(ctx->f);
    free(ctx);
    return NULL;
}

void smush_destroy(smush_ctx* ctx) {
    codec48_destroy(ctx);

    fclose(ctx->f);

    free(ctx->framebuffer);
    free(ctx);
}

void smush_set_debug(smush_ctx* ctx, int val) {
    _smush_debug_prints = val;
}

void smush_set_audio_callback(smush_ctx* ctx, smush_audio_callback_t callback)
{
    ctx->audio_callback = callback;
}

int smush_done(smush_ctx* ctx) {
    return ctx->cur_frame >= getle16(ctx->ahdr.num_frames);
}

int smush_cur_frame(smush_ctx* ctx) {
    return ctx->cur_frame;
}

int smush_num_frames(smush_ctx* ctx) {
    return getle16(ctx->ahdr.num_frames);
}

void smush_restart(smush_ctx* ctx) {
    ctx->frame_fpos = ctx->start_fpos;
    ctx->cur_frame = 0;
    memcpy(ctx->palette, ctx->ahdr.palette, sizeof(ctx->palette));
}

void smush_frame(smush_ctx* ctx)
{
    if (ctx->frame_fpos >= ctx->max_fpos) {
        smush_restart(ctx);
    }

    uint32_t seek_pos = ctx->frame_fpos;
    
    while (1)
    {
        smush_header tmp;
        
        fseek(ctx->f, seek_pos, SEEK_SET);
        if(fread(&tmp, 1, sizeof(tmp), ctx->f) <= 0) break;

        if(getbe32(tmp.magic) == SMUSH_MAGIC_FRME) {
            smush_proc_frme(ctx, seek_pos, getbe32(tmp.size));

            ctx->frame_fpos = seek_pos;
            ctx->frame_fpos += 8;
            ctx->frame_fpos += getbe32(tmp.size);
            ctx->cur_frame++;
            break;
        }
        else {
            smush_debug("  Tag @ 0x%x:\n", seek_pos);
            smush_debug("    Magic: %c%c%c%c\n", tmp.magic[0], tmp.magic[1], tmp.magic[2], tmp.magic[3]);
            smush_debug("    Size: 0x%x\n", getbe32(tmp.size));
        }

        seek_pos += 8;
        seek_pos += getbe32(tmp.size);
    }    
}

void smush_proc_fobj(smush_ctx* ctx, uint32_t seek_pos, uint32_t total_size)
{
    smush_fobj fobj;

    fseek(ctx->f, seek_pos, SEEK_SET);
    if(fread(&fobj, 1, sizeof(fobj), ctx->f) <= 0) return;

    smush_debug("    Codec: %u\n", fobj.codec);
    smush_debug("    Codec Param: 0x%x\n", fobj.codec_param);
    smush_debug("    Xpos: %d\n", getles16(fobj.x));
    smush_debug("    Ypos: %d\n", getles16(fobj.y));
    smush_debug("    Width: %u\n", getle16(fobj.width));
    smush_debug("    Height: %u\n", getle16(fobj.height));
    smush_debug("    Unk3: 0x%x\n", getle16(fobj.unk3));
    smush_debug("    Unk4: 0x%x\n", getle16(fobj.unk4));

    ctx->codec_x = getles16(fobj.x);
    ctx->codec_y = getles16(fobj.y);
    ctx->codec_w = getle16(fobj.width);
    ctx->codec_h = getle16(fobj.height);

    uint8_t* data = malloc(total_size);
    memset(data, 0, total_size);
    if (!data) return;

    fread(data, total_size - 0xE, 1, ctx->f);

    if (fobj.codec == 48) {
        codec48_proc(ctx, data, total_size - 0xE);
    }
    else {
        smush_error("Cannot handle codec: %u\n", fobj.codec);
    }
    

    free(data);
}

uint8_t smush_color_delta(uint8_t palval, int16_t delta)
{
    int t = (palval * 129 + delta) / 128;

    if (t < 0) {
        t = 0;
    }
    else if (t > 255) {
        t = 255;
    }

    return t;
}

void smush_proc_xpal(smush_ctx* ctx, uint32_t seek_pos, uint32_t total_size)
{
    uint8_t* data = (uint8_t*)malloc(total_size);
    if (!data) return;

    uint8_t* data_orig = data;

    memset(data, 0, total_size);
    fseek(ctx->f, seek_pos, SEEK_SET);
    fread(data, total_size, 1, ctx->f);

    uint32_t which = getbe32(data);
    data += sizeof(uint32_t);

    smush_debug("    XPAL which: %x\n", which);
    if (which == 1) {
        uint32_t extra = getle16(data);
        data += sizeof(uint16_t);
        smush_debug("    XPAL extra: %x\n", extra);
        for (int i = 0; i < 256 * 3; i++) {
            ctx->palette[i] = smush_color_delta(ctx->palette[i], ctx->delta_palette[i]);
        }
    }
    else if (which == 2) {
        for (int i = 0; i < 256 * 3; i++) {
            ctx->delta_palette[i] = getle16(&data[i*2]);
        }

        memcpy(ctx->palette, &data[256*3*2], sizeof(ctx->palette));
    }
    

    free(data_orig);
}

void smush_proc_npal(smush_ctx* ctx, uint32_t seek_pos, uint32_t total_size)
{
    assert(total_size == 0x300);
    fseek(ctx->f, seek_pos, SEEK_SET);
    fread(ctx->palette, sizeof(ctx->palette), 1, ctx->f);
}

void smush_proc_iact_payload(smush_ctx* ctx, const uint8_t* data, int64_t total_size)
{
    uint8_t* iact_tmp = ctx->iact_tmp;
    
    // !! The state in iact_tmp carries between IACT chunks
    while (total_size > 0) {
        if (ctx->iact_idx >= 2) {
            uint16_t len = getbe16(iact_tmp) + 2;
            len -= ctx->iact_idx;

            if (len > total_size) {
                len = total_size;
                memcpy(iact_tmp + ctx->iact_idx, data, len);
                data += len;
                ctx->iact_idx += len;
                total_size = 0;
                continue;
            }

            memcpy(iact_tmp + ctx->iact_idx, data, len);
            data += len;
            total_size -= len;

            //total_size -= sizeof(uint16_t);
            
            smush_debug("    Len?: 0x%04x\n", len);

            uint8_t* disposable_buf = (uint8_t*)malloc(0x1000);
            uint8_t* decode_in = iact_tmp + 2;
            uint8_t *out = disposable_buf;

            int decode_len = 1024;
            

            uint8_t hinib = *decode_in++;
            uint8_t lownib = hinib >> 4;
            hinib &= 0xF;

            // RLEish I guess
            while (decode_len--) 
            {
                uint8_t val = *decode_in++;
                //printf("%02x %04x %x\n", val, out - ctx->audio_buffer, total_size);
                if (val == 0x80) {
                    *out++ = *decode_in++;
                    *out++ = *decode_in++;
                }
                else {
                    int16_t val16 = (int8_t)val << lownib;
                    *out++ = val16 >> 8;
                    *out++ = (uint8_t)val16;
                }

                val = *decode_in++;
                if (val == 0x80) {
                    *out++ = *decode_in++;
                    *out++ = *decode_in++;
                }
                else {
                    int16_t val16 = (int8_t)val << hinib;
                    *out++ = val16 >> 8;
                    *out++ = (uint8_t)val16;
                }
            }

            if (ctx->audio_callback) {
                ctx->audio_callback(disposable_buf, 0x1000);
            }
            else {
                free(disposable_buf);
            }
            
            ctx->iact_idx = 0;
        }
        else {
            if (total_size > 1 && ctx->iact_idx == 0) {
                iact_tmp[ctx->iact_idx] = *data++;
                ctx->iact_idx++;
                total_size--;
            }

            iact_tmp[ctx->iact_idx] = *data++;
            ctx->iact_idx++;
            total_size--;
        }
    }
}

void smush_proc_iact(smush_ctx* ctx, uint32_t seek_pos, uint32_t total_size)
{
    smush_iact iact;
    fseek(ctx->f, seek_pos, SEEK_SET);
    fread(&iact, sizeof(iact), 1, ctx->f);

    uint16_t code, flags, unk, track_flags;

    code = getle16(iact.code);
    flags = getle16(iact.flags);
    unk = getle16(iact.unk);
    track_flags = getle16(iact.track_flags);

    smush_debug("    Code: 0x%04x\n", code);
    smush_debug("    Flags: 0x%04x\n", flags);
    smush_debug("    Unk: 0x%04x\n", unk);
    smush_debug("    Track Flags: 0x%04x\n", track_flags);

    total_size -= sizeof(iact);

    if (code == 8 && flags == 0x2E && !track_flags) {
        smush_imuse_iact payload;
        fread(&payload, sizeof(payload), 1, ctx->f);

        uint16_t track_id, index, frame_count;
        uint32_t bytes_left;

        track_id = getle16(payload.track_id);
        index = getle16(payload.index);
        frame_count = getle16(payload.frame_count);
        bytes_left = getle32(payload.bytes_left);

        total_size -= sizeof(payload);

        smush_debug("    Track ID: 0x%04x\n", track_id);
        smush_debug("    Index: 0x%04x\n", index);
        smush_debug("    Frame Count: 0x%04x\n", frame_count);
        smush_debug("    Bytes Left: 0x%04x\n", bytes_left);

        uint8_t* data = (uint8_t*)malloc(total_size);
        if (!data) return;

        memset(data, 0, total_size);
        fread(data, total_size, 1, ctx->f);

        smush_proc_iact_payload(ctx, data, total_size);

        free(data);
    }
    else {
        smush_error("Can't handle this IACT!\n", track_flags);
    }
}

void smush_proc_frme(smush_ctx* ctx, uint32_t seek_pos, uint32_t total_size)
{
    smush_header tmp;
    smush_fobj fobj;
        
    fseek(ctx->f, seek_pos, SEEK_SET);
    if(fread(&tmp, 1, sizeof(tmp), ctx->f) <= 0) return;

    smush_debug("  Frame Header @ 0x%x:\n", seek_pos);
    smush_debug("    Magic: %c%c%c%c\n", tmp.magic[0], tmp.magic[1], tmp.magic[2], tmp.magic[3]);
    smush_debug("    Size: 0x%x\n\n", getbe32(tmp.size));
    seek_pos += sizeof(tmp);

    uint32_t max_seek_pos = seek_pos + getbe32(tmp.size);

    while (1)
    {
        fseek(ctx->f, seek_pos, SEEK_SET);
        if(fread(&tmp, 1, sizeof(tmp), ctx->f) <= 0) return;

        smush_debug("  Frame Data @ 0x%x:\n", seek_pos);
        smush_debug("    Magic: %c%c%c%c\n", tmp.magic[0], tmp.magic[1], tmp.magic[2], tmp.magic[3]);
        smush_debug("    Size: 0x%x\n", getbe32(tmp.size));
        seek_pos += sizeof(tmp);

        if(getbe32(tmp.magic) == SMUSH_MAGIC_FOBJ) {
            smush_proc_fobj(ctx, seek_pos-8, getbe32(tmp.size));
            //break;
        }
        else if (getbe32(tmp.magic) == SMUSH_MAGIC_XPAL) {
            smush_proc_xpal(ctx, seek_pos, getbe32(tmp.size));
        }
        else if (getbe32(tmp.magic) == SMUSH_MAGIC_NPAL) {
            smush_proc_npal(ctx, seek_pos, getbe32(tmp.size));
        }
        else if (getbe32(tmp.magic) == SMUSH_MAGIC_IACT) {
            smush_proc_iact(ctx, seek_pos, getbe32(tmp.size));
        }
        else if (getbe32(tmp.magic) == SMUSH_MAGIC_FTCH) {
            // TODO?
        }
        else if (getbe32(tmp.magic) == SMUSH_MAGIC_STOR) {
            // TODO?
        }
        else {
            smush_warn("    Unhandled tag: %c%c%c%c\n", tmp.magic[0], tmp.magic[1], tmp.magic[2], tmp.magic[3]);
        }
        
        seek_pos += getbe32(tmp.size);
        if (seek_pos & 1) {
            seek_pos++;
        }

        if (seek_pos >= ctx->max_fpos) {
            //smush_restart(ctx);
            break;
        }

        if (seek_pos >= max_seek_pos) {
            break;
        }
    }

    
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
    
#if 0
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
#endif
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

    printf("    Frame Data:\n");
    printf("      Magic: %c%c%c%c\n", fobj.magic[0], fobj.magic[1], fobj.magic[2], fobj.magic[3]);
    printf("      Size: 0x%x\n", getbe32(fobj.size));
    printf("      Codec: %u\n", fobj.codec);
    printf("      Codec Param: 0x%x\n", fobj.codec_param);
    printf("      Xpos: %d\n", getles16(fobj.x));
    printf("      Ypos: %d\n", getles16(fobj.y));
    printf("      Width: %u\n", getle16(fobj.width));
    printf("      Height: %u\n", getle16(fobj.height));
    printf("      Unk3: 0x%x\n", getle16(fobj.unk3));
    printf("      Unk4: 0x%x\n", getle16(fobj.unk4));

    uint8_t* data = malloc(total_size - 0xE);
    if (!data) return;

    fread(data, total_size - 0xE, 1, ctx->f);

    if (fobj.codec == 48) {
        //codec48_proc(ctx, data);
    }
    else {
        printf("Cannot handle codec: %u\n", fobj.codec);
    }
    

    free(data);
}