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

#ifndef __GST_H264_ENCRYPTION_MODE_H__
#define __GST_H264_ENCRYPTION_MODE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef enum {
  GST_H264_ENCRYPTION_MODE_AES_ECB,
  GST_H264_ENCRYPTION_MODE_AES_CTR,
  GST_H264_ENCRYPTION_MODE_AES_CBC,
} GstH264EncryptionMode;

GType gst_h264_encryption_mode_get_type(void);
#define GST_TYPE_H264_ENCRYPTION_MODE (gst_h264_encryption_mode_get_type())

G_END_DECLS

#endif /* __GST_H264_ENCRYPTION_MODE_H__ */
