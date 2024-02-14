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

#ifndef __GST_H264ENCRYPT_H__
#define __GST_H264ENCRYPT_H__

#include <gst/base/gstbasetransform.h>
#include <gst/codecparsers/gsth264parser.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_H264_ENCRYPT (gst_h264_encrypt_get_type())
G_DECLARE_FINAL_TYPE(GstH264Encrypt, gst_h264_encrypt, GST, H264_ENCRYPT,
                     GstBaseTransform)
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

struct _GstH264Encrypt {
  GstBaseTransform element;

  gboolean silent;
  GstH264NalParser *nalparser;
};

G_END_DECLS

#endif /* __GST_H264ENCRYPT_H__ */
