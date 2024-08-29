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

#ifndef __GST_H264_ENCRYPTION_PLUGIN_H__
#define __GST_H264_ENCRYPTION_PLUGIN_H__

#include <gst/gst.h>

G_BEGIN_DECLS

// Has to be 16 bytes, excluding the null byte
#define GST_H264_ENCRYPT_IV_SEI_UUID "GSTH264ENCRYPTIV"
// NOTE: If IV SEI payload size changes, you need to change the third byte below
#define GST_H264_ENCRYPT_IV_SEI_SIGNATURE \
  "\x06\x05\x20" GST_H264_ENCRYPT_IV_SEI_UUID

G_END_DECLS

#endif /* __GST_H264_ENCRYPTION_PLUGIN_H__ */
