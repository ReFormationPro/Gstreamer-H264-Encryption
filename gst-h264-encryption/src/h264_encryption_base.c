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

/**
 * SECTION:element-h264encryptionbase
 *
 * FIXME:Describe h264encryptionbase here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! h264encryptionbase ! fakesink silent=TRUE
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
#include "h264_encryption_base.h"
#include "h264_encryption_base_private.h"
#include "h264_encryption_mode.h"
#include "h264_encryption_types.h"

GST_DEBUG_CATEGORY_STATIC(gst_h264_encryption_base_debug);
#define GST_CAT_DEFAULT gst_h264_encryption_base_debug
#define DEFAULT_ENCRYPTION_MODE GST_H264_ENCRYPTION_MODE_AES_CTR

#define gst_h264_encryption_base_parent_class parent_class

typedef struct _GstH264EncryptionBasePrivate GstH264EncryptionBasePrivate;
struct _GstH264EncryptionBasePrivate {
  GstH264EncryptionUtils utils;
};
G_DEFINE_TYPE_WITH_PRIVATE(GstH264EncryptionBase, gst_h264_encryption_base,
                           GST_TYPE_BASE_TRANSFORM)
GST_ELEMENT_REGISTER_DEFINE(h264encryptionbase, "h264encryptionbase",
                            GST_RANK_NONE, GST_TYPE_H264_ENCRYPTION_BASE)

static void gst_h264_encryption_base_set_property(GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void gst_h264_encryption_base_get_property(GObject *object,
                                                  guint prop_id, GValue *value,
                                                  GParamSpec *pspec);
static void gst_h264_encryption_base_dispose(GObject *object);
static void gst_h264_encryption_base_finalize(GObject *object);

static GstFlowReturn gst_h264_encryption_base_transform(GstBaseTransform *base,
                                                        GstBuffer *inbuf,
                                                        GstBuffer *outbuf);

/* GObject vmethod implementations */

/* initialize the h264encryptionbase's class */
static void gst_h264_encryption_base_class_init(
    GstH264EncryptionBaseClass *klass) {
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;

  gobject_class->set_property = gst_h264_encryption_base_set_property;
  gobject_class->get_property = gst_h264_encryption_base_get_property;
  gobject_class->dispose = gst_h264_encryption_base_dispose;
  gobject_class->finalize = gst_h264_encryption_base_finalize;

  klass->enter_base_transform = NULL;
  klass->before_nalu_copy = NULL;
  klass->process_slice_nalu = NULL;

  gst_element_class_set_details_simple(
      gstelement_class, "h264encryptionbase", "Codec/Encryption/Video",
      "H264 video encryption base element",
      "Oguzhan Oztaskin <oguzhanoztaskin@gmail.com>");
  g_object_class_install_property(
      gobject_class, PROP_ENCRYPTION_MODE,
      g_param_spec_enum("encryption-mode", "Encryption Mode",
                        "Mode of encryption to perform",
                        GST_TYPE_H264_ENCRYPTION_MODE, DEFAULT_ENCRYPTION_MODE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                            GST_PARAM_MUTABLE_PAUSED));
  g_object_class_install_property(
      gobject_class, PROP_KEY,
      g_param_spec_boxed("key", "Encryption Key",
                         G_STRINGIFY(AES_KEYLEN * 8) " bit encryption key",
                         GST_TYPE_ENCRYPTION_KEY,
                         G_PARAM_WRITABLE | GST_PARAM_MUTABLE_PAUSED |
                             G_PARAM_STATIC_STRINGS));

  GST_BASE_TRANSFORM_CLASS(klass)->transform =
      GST_DEBUG_FUNCPTR(gst_h264_encryption_base_transform);

  GST_DEBUG_CATEGORY_INIT(gst_h264_encryption_base_debug, "h264encryptionbase",
                          0, "h264encryptionbase general logs");

  gst_type_mark_as_plugin_api(GST_TYPE_H264_ENCRYPTION_MODE, 0);
}

/* initialize the new element
 * initialize instance structure
 */
static void gst_h264_encryption_base_init(
    GstH264EncryptionBase *h264encryptionbase) {
  GstH264EncryptionBasePrivate *priv =
      gst_h264_encryption_base_get_instance_private(h264encryptionbase);
  priv->utils.nalparser = gst_h264_nal_parser_new();
  priv->utils.encryption_mode = DEFAULT_ENCRYPTION_MODE;
  priv->utils.key = NULL;
}

static void gst_h264_encryption_base_dispose(GObject *object) {
  G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void gst_h264_encryption_base_finalize(GObject *object) {
  GstH264EncryptionBase *h264encryptionbase = GST_H264_ENCRYPTION_BASE(object);
  GstH264EncryptionBasePrivate *priv =
      gst_h264_encryption_base_get_instance_private(h264encryptionbase);
  gst_h264_nal_parser_free(priv->utils.nalparser);
  priv->utils.nalparser = NULL;
  if (priv->utils.key) g_boxed_free(GST_TYPE_ENCRYPTION_KEY, priv->utils.key);
  priv->utils.key = NULL;
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_h264_encryption_base_set_property(GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec) {
  GstH264EncryptionBase *h264encryptionbase = GST_H264_ENCRYPTION_BASE(object);
  GstH264EncryptionBasePrivate *priv =
      gst_h264_encryption_base_get_instance_private(h264encryptionbase);

  switch (prop_id) {
    case PROP_ENCRYPTION_MODE:
      priv->utils.encryption_mode = g_value_get_enum(value);
      break;
    case PROP_KEY:
      if (priv->utils.key) {
        g_boxed_free(GST_TYPE_ENCRYPTION_KEY, priv->utils.key);
      }
      priv->utils.key = g_value_dup_boxed(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_h264_encryption_base_get_property(GObject *object,
                                                  guint prop_id, GValue *value,
                                                  GParamSpec *pspec) {
  GstH264EncryptionBase *h264encryptionbase = GST_H264_ENCRYPTION_BASE(object);
  GstH264EncryptionBasePrivate *priv =
      gst_h264_encryption_base_get_instance_private(h264encryptionbase);

  switch (prop_id) {
    case PROP_ENCRYPTION_MODE:
      g_value_set_enum(value, priv->utils.encryption_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

size_t _copy_memory_bytes(GstMapInfo *dest_map_info, GstMapInfo *src_map_info,
                          size_t *dest_offset, size_t src_offset, size_t size) {
  if (dest_map_info->maxsize < *dest_offset + size) {
    GST_ERROR("Unable to copy as destination is too small");
    return 0;
  }
  memcpy(&dest_map_info->data[*dest_offset], &src_map_info->data[src_offset],
         size);
  *dest_offset += size;
  return size;
}

size_t _copy_nalu_bytes(GstMapInfo *dest_map_info, GstH264NalUnit *nalu,
                        size_t *dest_offset) {
  size_t nalu_total_size = nalu->size + (nalu->offset - nalu->sc_offset);
  if (dest_map_info->maxsize < *dest_offset + nalu_total_size) {
    GST_ERROR("Unable to copy as destination is too small");
    return 0;
  }
  memcpy(&dest_map_info->data[*dest_offset], &nalu->data[nalu->sc_offset],
         nalu_total_size);
  *dest_offset += nalu_total_size;
  return nalu_total_size;
}

static GstFlowReturn gst_h264_encryption_base_transform(GstBaseTransform *base,
                                                        GstBuffer *inbuf,
                                                        GstBuffer *outbuf) {
  GstH264EncryptionBase *h264encryptionbase = GST_H264_ENCRYPTION_BASE(base);
  GstH264NalUnit nalu;
  GstH264ParserResult result;
  struct AES_ctx *ctx;
  GstMapInfo map_info, dest_map_info;
  GstH264EncryptionBasePrivate *priv =
      gst_h264_encryption_base_get_instance_private(h264encryptionbase);
  ctx = &priv->utils.ctx;

  GST_DEBUG_OBJECT(h264encryptionbase, "A buffer is received");
  if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(inbuf)))
    gst_object_sync_values(GST_OBJECT(h264encryptionbase),
                           GST_BUFFER_TIMESTAMP(inbuf));
  if (G_UNLIKELY(!gst_buffer_map(inbuf, &map_info, GST_MAP_READ))) {
    GST_ERROR_OBJECT(base, "Unable to map input buffer for read!");
    return GST_FLOW_ERROR;
  }
  if (G_UNLIKELY(!gst_buffer_map(outbuf, &dest_map_info, GST_MAP_READWRITE))) {
    GST_ERROR_OBJECT(base, "Unable to map output buffer for rw!");
    return GST_FLOW_ERROR;
  }
  // Init context with key (iv is later if needed)
  if (G_UNLIKELY(!priv->utils.key)) {
    GST_ERROR_OBJECT(base, "Key is not set!");
    goto error;
  }
  AES_init_ctx(ctx, priv->utils.key->bytes);
  size_t dest_offset = 0;
  result = gst_h264_parser_identify_nalu(priv->utils.nalparser, map_info.data,
                                         0, map_info.size, &nalu);
  GST_H264_ENCRYPTION_BASE_GET_CLASS(h264encryptionbase)
      ->enter_base_transform(h264encryptionbase);
  while (result == GST_H264_PARSER_OK || result == GST_H264_PARSER_NO_NAL_END) {
    // Processes the following NALU types:
    // GST_H264_NAL_SLICE        = 1,
    // GST_H264_NAL_SLICE_DPA    = 2,
    // GST_H264_NAL_SLICE_DPB    = 3,
    // GST_H264_NAL_SLICE_DPC    = 4,
    // GST_H264_NAL_SLICE_IDR    = 5,
    // Need to populate SPS/PPS of nalparser for parsing slice header later
    gst_h264_parser_parse_nal(priv->utils.nalparser, &nalu);
    gboolean copy;
    if (!GST_H264_ENCRYPTION_BASE_GET_CLASS(h264encryptionbase)
             ->before_nalu_copy(h264encryptionbase, &nalu, &dest_map_info,
                                &dest_offset, &copy)) {
      goto error;
    }
    if (G_LIKELY(copy)) {
      if (IS_SLICE_NALU(nalu.type)) {
        // Copy the slice into dest
        size_t nalu_total_size;
        if ((nalu_total_size =
                 _copy_nalu_bytes(&dest_map_info, &nalu, &dest_offset)) == 0) {
          goto error;
        }
        // Process dest nalu
        GstH264NalUnit dest_nalu;  // NOTE Maybe we can modify nalu instead of
                                   // parsing another one
        GstH264ParserResult parse_result = gst_h264_parser_identify_nalu(
            priv->utils.nalparser, dest_map_info.data,
            dest_offset - nalu_total_size, dest_offset, &dest_nalu);
        GST_DEBUG_OBJECT(
            h264encryptionbase,
            "Source nal unit is copied. Type %d sc_offset %d total_size "
            "%d",
            nalu.type, nalu.sc_offset,
            nalu.size + (nalu.offset - nalu.sc_offset));
        GST_DEBUG_OBJECT(
            h264encryptionbase,
            "Copied nal unit is parsed. Type %d sc_offset %d "
            "total_size %d expected size %ld",
            dest_nalu.type, dest_nalu.sc_offset,
            dest_nalu.size + (dest_nalu.offset - dest_nalu.sc_offset),
            nalu_total_size);
        if (parse_result != GST_H264_PARSER_NO_NAL_END &&
            parse_result != GST_H264_PARSER_OK) {
          GST_ERROR_OBJECT(h264encryptionbase,
                           "Unable to parse destination nal unit");
          goto error;
        }
        if (!GST_H264_ENCRYPTION_BASE_GET_CLASS(h264encryptionbase)
                 ->process_slice_nalu(h264encryptionbase, &dest_nalu,
                                      &dest_map_info, &dest_offset)) {
          GST_ERROR_OBJECT(h264encryptionbase,
                           "Subclass failed to parse slice nalu");
          goto error;
        }
      } else {
        // Copy non-slice nal unit
        size_t nalu_total_size = nalu.size + (nalu.offset - nalu.sc_offset);
        if (_copy_memory_bytes(&dest_map_info, &map_info, &dest_offset,
                               nalu.sc_offset, nalu_total_size) == 0) {
          goto error;
        }
      }
    }
    // Parse the next nalu
    result = gst_h264_parser_identify_nalu(priv->utils.nalparser, map_info.data,
                                           nalu.offset + nalu.size,  //
                                           map_info.size, &nalu);
  }
  gst_buffer_unmap(inbuf, &map_info);
  gst_buffer_unmap(outbuf, &dest_map_info);
  gst_buffer_set_size(outbuf, dest_offset);
  return GST_FLOW_OK;
error: {
  gst_buffer_unmap(inbuf, &map_info);
  gst_buffer_unmap(outbuf, &dest_map_info);
  return GST_FLOW_ERROR;
}
}

gboolean gst_h264_encryption_base_calculate_payload_offset_and_size(
    GstH264EncryptionBase *encryption_base, GstH264NalParser *nalparser,
    GstH264NalUnit *nalu, gsize *payload_offset, gsize *payload_size) {
  // Calculate payload offset and size
  GstH264SliceHdr slice;
  GstH264ParserResult parse_slice_hdr_result;
  if ((parse_slice_hdr_result = gst_h264_parser_parse_slice_hdr(
           nalparser, nalu, &slice, TRUE, TRUE)) != GST_H264_PARSER_OK) {
    GST_ERROR_OBJECT(encryption_base, "Unable to parse slice header! Err: %d",
                     (uint32_t)parse_slice_hdr_result);
    return FALSE;
  }
  const gsize slice_header_size =
      ((slice.header_size - 1) / 8 + 1) + slice.n_emulation_prevention_bytes;
  *payload_offset = nalu->offset + nalu->header_bytes + slice_header_size;
  *payload_size = nalu->size - nalu->header_bytes - slice_header_size;
  return TRUE;
}

GstH264EncryptionUtils *gst_h264_encryption_base_get_encryption_utils(
    GstH264EncryptionBase *encryption_base) {
  GstH264EncryptionBasePrivate *priv =
      gst_h264_encryption_base_get_instance_private(encryption_base);
  return &priv->utils;
}
