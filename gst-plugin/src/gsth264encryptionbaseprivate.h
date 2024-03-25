/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2020 Niels De Graef <niels.degraef@gmail.com>
 * Copyright (C) 2024 root <<user@hostname.org>>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_H264ENCRYPTIONBASEPRIVATE_H__
#define __GST_H264ENCRYPTIONBASEPRIVATE_H__

#include <gst/gst.h>

#include "gsth264encryptionbase.h"

G_BEGIN_DECLS

enum {
  PROP_0,
  PROP_ENCRYPTION_MODE,
  PROP_IV,
  PROP_KEY,
};

typedef struct GstH264EncryptionUtils {
  GstH264NalParser *nalparser;
  GstH264EncryptionMode encryption_mode;
  GstEncryptionKey *key;
  GstEncryptionIV *iv;
} GstH264EncryptionUtils;

size_t _copy_memory_bytes(GstMapInfo *dest_map_info, GstMapInfo *src_map_info,
                          size_t *dest_offset, size_t src_offset, size_t size);

size_t _copy_nalu_bytes(GstMapInfo *dest_map_info, GstH264NalUnit *nalu,
                        size_t *dest_offset);

// NOTE This is a bad work-around for making protected fields
GstH264EncryptionUtils *gst_h264_encryption_base_get_encryption_utils(
    GstH264EncryptionBase *);

G_END_DECLS

#endif /* __GST_H264ENCRYPTIONBASEPRIVATE_H__ */
