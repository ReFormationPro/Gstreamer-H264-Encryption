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

#ifndef __GST_H264_ENCRYPTION_BASE_H__
#define __GST_H264_ENCRYPTION_BASE_H__

#include <gst/base/gstbasetransform.h>      // for: GstBaseTransform, GstBaseTransformClass
#include <gst/codecparsers/gsth264parser.h> // for: GstH264NalParser, GstH264NalUnit
#include <gst/gst.h>

G_BEGIN_DECLS

/**
 * Ciphertext may end with a null-byte and this null-byte is discarded
 * according to ITU-T Rec. H.264 (05/2003) Section B.2 point 5.
 * To avoid this, we always put a marker at the end of the ciphertext,
 * whose value is defined below. Any byte greater than 0x03 should be ok.
 */
#define CIPHERTEXT_END_MARKER 0x80
// Fix unused variable warnings
#define UNUSED(x) (void)(x)

#define IS_SLICE_NALU(nalu_type) \
  (nalu_type >= GST_H264_NAL_SLICE && nalu_type <= GST_H264_NAL_SLICE_IDR)

#define GST_TYPE_H264_ENCRYPTION_BASE (gst_h264_encryption_base_get_type())
G_DECLARE_DERIVABLE_TYPE(GstH264EncryptionBase, gst_h264_encryption_base, GST,
                         H264_ENCRYPTION_BASE, GstBaseTransform)

typedef void (*enter_base_transform_func)(
    GstH264EncryptionBase *encryption_base);
typedef gboolean (*before_nalu_copy_func)(
    GstH264EncryptionBase *encryption_base, GstH264NalUnit *src_nalu,
    GstMapInfo *dest_map_info, size_t *dest_offset, gboolean *copy);
typedef gboolean (*process_slice_nalu_func)(
    GstH264EncryptionBase *encryption_base, GstH264NalUnit *dest_nalu,
    GstMapInfo *dest_map_info, size_t *dest_offset);

struct _GstH264EncryptionBaseClass {
  GstBaseTransformClass parent_class;

  enter_base_transform_func enter_base_transform;
  before_nalu_copy_func before_nalu_copy;
  process_slice_nalu_func process_slice_nalu;
  gpointer padding[6];
};
// GstH264EncryptionBase *    gst_h264_encryption_base_new(void);

gboolean gst_h264_encryption_base_calculate_payload_offset_and_size(
    GstH264EncryptionBase *encryption_base, GstH264NalParser *nalparser,
    GstH264NalUnit *nalu, gsize *payload_offset, gsize *payload_size);

G_END_DECLS

#endif /* __GST_H264_ENCRYPTION_BASE_H__ */
