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

#include "h264_encryption_mode.h"

GType gst_h264_encryption_mode_get_type(void) {
  static GType h264_encryption_mode_type = 0;
  static const GEnumValue pattern_types[] = {
      {GST_H264_ENCRYPTION_MODE_AES_ECB,
       "AES Electronic Code Book mode encryption. Unsafe, do not use",
       "aes-ecb"},
      {GST_H264_ENCRYPTION_MODE_AES_CTR, "AES Counter mode encryption",
       "aes-ctr"},
      {GST_H264_ENCRYPTION_MODE_AES_CBC,
       "AES Cipher Block Chaining mode encryption", "aes-cbc"},
      {0, NULL, NULL}};
  if (g_once_init_enter(&h264_encryption_mode_type)) {
    GType setup_value =
        g_enum_register_static("GstH264EncryptionMode", pattern_types);
    g_once_init_leave(&h264_encryption_mode_type, setup_value);
  }

  return h264_encryption_mode_type;
}
