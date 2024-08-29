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

#ifndef __GST_H264_DECRYPT_H__
#define __GST_H264_DECRYPT_H__

#include <gst/gst.h>

#include "h264_encryption_base.h"

G_BEGIN_DECLS

GST_ELEMENT_REGISTER_DECLARE(h264decrypt)
#define GST_TYPE_H264_DECRYPT (gst_h264_decrypt_get_type())
G_DECLARE_FINAL_TYPE(GstH264Decrypt, gst_h264_decrypt, GST, H264_DECRYPT,
                     GstH264EncryptionBase)
// #define GST_H264_SRC(obj)
// (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_SRC,GstBaseSrc)) #define
// GST_BASE_SRC_CLASS(klass)
// (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASE_SRC,GstBaseSrcClass)) #define
// GST_BASE_SRC_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj),
// GST_TYPE_BASE_SRC, GstBaseSrcClass)) #define GST_IS_BASE_SRC(obj)
// (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASE_SRC)) #define
// GST_IS_BASE_SRC_CLASS(klass)
// (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASE_SRC)) #define
// GST_BASE_SRC_CAST(obj)          ((GstBaseSrc *)(obj))

struct _GstH264Decrypt {
  GstH264EncryptionBase encryption_base;

  gboolean found_iv_sei;
};

G_END_DECLS

#endif /* __GST_H264_DECRYPT_H__ */
