/*
 * GStreamer H264 Encryption Plugin
 *
 * Copyright (C) 2024 Oguzhan Oztaskin <oguzhanoztaskin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __GST_H264_ENCRYPTION_BASE_PRIVATE_H__
#define __GST_H264_ENCRYPTION_BASE_PRIVATE_H__

#include <gst/gst.h>

#include "ciphers/aes.h"           // for: AES_ctx
#include "h264_encryption_base.h"
#include "h264_encryption_mode.h"  // for: GstH264EncryptionMode
#include "h264_encryption_types.h" // for: GstEncryptionKey

G_BEGIN_DECLS

enum {
  PROP_0,
  PROP_ENCRYPTION_MODE,
  PROP_KEY,
  PROP_LAST,
};

typedef struct GstH264EncryptionUtils {
  GstH264NalParser *nalparser;
  GstH264EncryptionMode encryption_mode;
  GstEncryptionKey *key;
  struct AES_ctx ctx;
} GstH264EncryptionUtils;

size_t _copy_memory_bytes(GstMapInfo *dest_map_info, GstMapInfo *src_map_info,
                          size_t *dest_offset, size_t src_offset, size_t size);

size_t _copy_nalu_bytes(GstMapInfo *dest_map_info, GstH264NalUnit *nalu,
                        size_t *dest_offset);

// NOTE: This is a bad work-around for making protected fields
GstH264EncryptionUtils *gst_h264_encryption_base_get_encryption_utils(
    GstH264EncryptionBase *);

G_END_DECLS

#endif /* __GST_H264_ENCRYPTION_BASE_PRIVATE_H__ */
