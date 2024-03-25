/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
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

/**
 * SECTION:element-h264decrypt
 *
 * FIXME:Describe h264decrypt here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! h264decrypt ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/base/base.h>
#include <gst/codecparsers/gsth264parser.h>
#include <gst/controller/controller.h>
#include <gst/gst.h>

#include "ciphers/aes.h"
#include "gsth264decrypt.h"
#include "gsth264encryptionbaseprivate.h"
#include "gsth264encryptionmode.h"
#include "gsth264encryptionplugin.h"
#include "gsth264encryptiontypes.h"

GST_DEBUG_CATEGORY_STATIC(gst_h264_decrypt_debug);
#define GST_CAT_DEFAULT gst_h264_decrypt_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-h264,alignment=au,stream-format=byte-stream"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-h264,alignment=au,stream-format=byte-stream"));

#define gst_h264_decrypt_parent_class parent_class
G_DEFINE_TYPE(GstH264Decrypt, gst_h264_decrypt, GST_TYPE_H264_ENCRYPTION_BASE);
GST_ELEMENT_REGISTER_DEFINE(h264decrypt, "h264decrypt", GST_RANK_NONE,
                            GST_TYPE_H264_DECRYPT);

static GstFlowReturn gst_h264_decrypt_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *input, GstBuffer **outbuf);
static void gst_h264_decrypt_enter_base_transform(
    GstH264EncryptionBase *encryption_base);
static gboolean gst_h264dencrypt_before_nalu_copy(
    GstH264EncryptionBase *encryption_base, GstH264NalUnit *src_nalu,
    GstMapInfo *dest_map_info, size_t *dest_offset);
static gboolean gst_h264_decrypt_process_slice_nalu(
    GstH264EncryptionBase *encryption_base, struct AES_ctx *ctx,
    GstH264NalUnit *dest_nalu, GstMapInfo *dest_map_info, size_t *dest_offset);
static gboolean gst_h264_decrypt_decrypt_slice_nalu(GstH264Decrypt *h264decrypt,
                                                    struct AES_ctx *ctx,
                                                    GstH264NalUnit *nalu,
                                                    size_t *dest_offset);
/* GObject vmethod implementations */

/* initialize the h264decrypt's class */
static void gst_h264_decrypt_class_init(GstH264DecryptClass *klass) {
  // GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstH264EncryptionBaseClass *gsth264encryptionbase_class;

  // gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;
  gsth264encryptionbase_class = (GstH264EncryptionBaseClass *)klass;

  gsth264encryptionbase_class->enter_base_transform =
      gst_h264_decrypt_enter_base_transform;
  gsth264encryptionbase_class->before_nalu_copy =
      gst_h264dencrypt_before_nalu_copy;
  gsth264encryptionbase_class->process_slice_nalu =
      gst_h264_decrypt_process_slice_nalu;

  gst_element_class_set_details_simple(
      gstelement_class, "h264decrypt", "Codec/Encryptor/Video",
      "H264 Video Encryptor", "Oguzhan Oztaskin <oguzhanoztaskin@gmail.com>");

  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&src_template));
  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&sink_template));

  GST_BASE_TRANSFORM_CLASS(klass)->prepare_output_buffer =
      GST_DEBUG_FUNCPTR(gst_h264_decrypt_prepare_output_buffer);

  GST_DEBUG_CATEGORY_INIT(gst_h264_decrypt_debug, "h264decrypt", 0,
                          "h264decrypt general logs");
}

/* initialize the new element
 * initialize instance structure
 */
static void gst_h264_decrypt_init(GstH264Decrypt *h264decrypt) {
  // FIXME Do I need to call this or is it already called?
  // G_OBJECT_CLASS(parent_class)->init(h264decrypt);
}

/* GstBaseTransform vmethod implementations */

static GstFlowReturn gst_h264_decrypt_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *input, GstBuffer **outbuf) {
  gsize input_size = gst_buffer_get_size(input);
  *outbuf = gst_buffer_new_and_alloc(input_size);
  return GST_FLOW_OK;
}

static void gst_h264_decrypt_enter_base_transform(
    GstH264EncryptionBase *encryption_base) {}

static gboolean gst_h264dencrypt_before_nalu_copy(
    GstH264EncryptionBase *encryption_base, GstH264NalUnit *src_nalu,
    GstMapInfo *dest_map_info, size_t *dest_offset) {
  return TRUE;
}

static gboolean gst_h264_decrypt_process_slice_nalu(
    GstH264EncryptionBase *encryption_base, struct AES_ctx *ctx,
    GstH264NalUnit *dest_nalu, GstMapInfo *dest_map_info, size_t *dest_offset) {
  GstH264Decrypt *h264decrypt = GST_H264_DECRYPT(encryption_base);
  // TODO Remove emulation three bytes here
  if (!gst_h264_decrypt_decrypt_slice_nalu(h264decrypt, ctx, dest_nalu,
                                           dest_offset)) {
    GST_ERROR_OBJECT(encryption_base, "Failed to decrypt slice nal unit");
    return FALSE;
  }
  return TRUE;
}

/**
 * Removes the padding if exists and returns padding byte count.
 *
 * Assumes data is padded at byte level, so it does not check individual bits.
 */
inline static gint _remove_padding(uint8_t *data, size_t size) {
  for (int i = size - 1; i >= 0; i--) {
    switch (data[i]) {
      case 0:
        continue;
      case 0x80: {
        data[i] = 0;
        return size - i;
      }
      default: {
        GST_ERROR("Padding is not removed! Invalid byte found: %d", data[i]);
        break;
      }
    }
  }
  // All zeros, not found
  return 0;
}

/**
 * Decrypts padded nal unit and updates dest_offset.
 *
 * @nalu: Destination NAL unit
 */
static gboolean gst_h264_decrypt_decrypt_slice_nalu(GstH264Decrypt *h264decrypt,
                                                    struct AES_ctx *ctx,
                                                    GstH264NalUnit *nalu,
                                                    size_t *dest_offset) {
  GstH264EncryptionBase *encryption_base =
      GST_H264_ENCRYPTION_BASE(h264decrypt);
  GstH264EncryptionUtils *utils =
      gst_h264_encryption_base_get_encryption_utils(encryption_base);
  // Calculate payload offset and size
  gsize payload_offset, payload_size;
  if (!gst_h264_encryption_base_calculate_payload_offset_and_size(
          encryption_base, utils->nalparser, nalu, &payload_offset,
          &payload_size)) {
    return FALSE;
  }
  if (payload_size % AES_BLOCKLEN != 0) {
    GST_ERROR_OBJECT(encryption_base,
                     "Encrypted block size (%ld) is not a multiple of "
                     "AES_BLOCKLEN (%d). Not attempting to decrypt.",
                     payload_size, AES_BLOCKLEN);
    return FALSE;
  }
  GST_DEBUG_OBJECT(encryption_base,
                   "Decrypting nal unit of type %d offset %ld size %ld",
                   nalu->type, payload_offset, payload_size);
  // Decrypt
  switch (utils->encryption_mode) {
    case GST_H264_ENCRYPTION_MODE_AES_CTR:
      AES_CTR_xcrypt_buffer(ctx, &nalu->data[payload_offset], payload_size);
      break;
    case GST_H264_ENCRYPTION_MODE_AES_ECB: {
      for (size_t i = 0; i < payload_size; i += AES_BLOCKLEN) {
        AES_ECB_decrypt(ctx, &nalu->data[payload_offset + i]);
      }
      break;
    }
    case GST_H264_ENCRYPTION_MODE_AES_CBC: {
      AES_CBC_decrypt_buffer(ctx, &nalu->data[payload_offset], payload_size);
      break;
    }
  }
  // Remove padding
  // Only last AES_BLOCKLEN many bytes can be padding bytes
  int padding_byte_count =
      _remove_padding(&nalu->data[payload_size - AES_BLOCKLEN], AES_BLOCKLEN);
  if (G_UNLIKELY(padding_byte_count == 0)) {
    GST_WARNING_OBJECT(encryption_base,
                       "Padding is not found, data is invalid.");
  }
  // We should overwrite padding bytes
  *dest_offset -= padding_byte_count;
  return TRUE;
}
