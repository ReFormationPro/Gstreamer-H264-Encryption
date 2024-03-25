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
#include "gsth264encryptionmode.h"
#include "gsth264encryptionplugin.h"
#include "gsth264encryptiontypes.h"

GST_DEBUG_CATEGORY_STATIC(gst_h264_decrypt_debug);
#define GST_CAT_DEFAULT gst_h264_decrypt_debug
#define DEFAULT_DECRYPTION_MODE GST_H264_ENCRYPTION_MODE_AES_CTR

/* h264decrypt signals and args */
enum {
  /* FILL ME */
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ENCRYPTION_MODE,
  PROP_IV,
  PROP_KEY,
};

// static uint8_t key[16] = {10, 10, 10, 20, 20, 20, 30, 30,
//                           30, 04, 04, 04, 04, 05, 05, 05};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-h264,alignment=au,stream-format=byte-stream"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-h264,alignment=au,stream-format=byte-stream"));

#define gst_h264_decrypt_parent_class parent_class
G_DEFINE_TYPE(GstH264Decrypt, gst_h264_decrypt, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE(h264decrypt, "h264decrypt", GST_RANK_NONE,
                            GST_TYPE_H264_DECRYPT);

static void gst_h264_decrypt_set_property(GObject *object, guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec);
static void gst_h264_decrypt_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec);
static void gst_h264_decrypt_dispose(GObject *object);
static void gst_h264_decrypt_finalize(GObject *object);

static GstFlowReturn gst_h264_decrypt_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *input, GstBuffer **outbuf);
static GstFlowReturn gst_h264_decrypt_transform(GstBaseTransform *base,
                                                GstBuffer *inbuf,
                                                GstBuffer *outbuf);

/* GObject vmethod implementations */

/* initialize the h264decrypt's class */
static void gst_h264_decrypt_class_init(GstH264DecryptClass *klass) {
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;

  gobject_class->set_property = gst_h264_decrypt_set_property;
  gobject_class->get_property = gst_h264_decrypt_get_property;
  gobject_class->dispose = gst_h264_decrypt_dispose;
  gobject_class->finalize = gst_h264_decrypt_finalize;

  gst_element_class_set_details_simple(
      gstelement_class, "h264decrypt", "Codec/Encryptor/Video",
      "H264 Video Encryptor", "Oguzhan Oztaskin <oguzhanoztaskin@gmail.com>");
  g_object_class_install_property(
      gobject_class, PROP_ENCRYPTION_MODE,
      g_param_spec_enum("encryption-mode", "Decryption Mode",
                        "Mode of decryption to perform",
                        GST_TYPE_H264_ENCRYPTION_MODE, DEFAULT_DECRYPTION_MODE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                            GST_PARAM_MUTABLE_PAUSED));
  g_object_class_install_property(
      gobject_class, PROP_KEY,
      g_param_spec_boxed("key", "Encryption Key",
                         // TODO Just enter bitsize
                         G_STRINGIFY(AES_KEYLEN * 8) " bit encryption key",
                         GST_TYPE_ENCRYPTION_KEY,
                         G_PARAM_WRITABLE | GST_PARAM_MUTABLE_PAUSED |
                             G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      gobject_class, PROP_IV,
      g_param_spec_boxed(
          "iv", "Encryption IV",
          // TODO Just enter bitsize
          G_STRINGIFY(AES_BLOCKLEN *
                      8) " bit encryption iv. Required for CTR/CBC modes",
          GST_TYPE_ENCRYPTION_IV, G_PARAM_WRITABLE | GST_PARAM_MUTABLE_PAUSED));

  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&src_template));
  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&sink_template));

  GST_BASE_TRANSFORM_CLASS(klass)->transform =
      GST_DEBUG_FUNCPTR(gst_h264_decrypt_transform);
  GST_BASE_TRANSFORM_CLASS(klass)->prepare_output_buffer =
      GST_DEBUG_FUNCPTR(gst_h264_decrypt_prepare_output_buffer);

  GST_DEBUG_CATEGORY_INIT(gst_h264_decrypt_debug, "h264decrypt", 0,
                          "h264decrypt general logs");

  gst_type_mark_as_plugin_api(GST_TYPE_H264_ENCRYPTION_MODE, 0);
}

/* initialize the new element
 * initialize instance structure
 */
static void gst_h264_decrypt_init(GstH264Decrypt *h264decrypt) {
  h264decrypt->nalparser = gst_h264_nal_parser_new();
  h264decrypt->encryption_mode = DEFAULT_DECRYPTION_MODE;
  h264decrypt->key = NULL;
  h264decrypt->iv = NULL;
}

static void gst_h264_decrypt_dispose(GObject *object) {
  G_OBJECT_CLASS(gst_h264_decrypt_parent_class)->dispose(object);
}

static void gst_h264_decrypt_finalize(GObject *object) {
  GstH264Decrypt *h264decrypt = GST_H264_DECRYPT(object);
  gst_h264_nal_parser_free(h264decrypt->nalparser);
  h264decrypt->nalparser = NULL;
  // FIXME need to use
  if (h264decrypt->key) g_boxed_free(GST_TYPE_ENCRYPTION_KEY, h264decrypt->key);
  h264decrypt->key = NULL;
  if (h264decrypt->iv) g_boxed_free(GST_TYPE_ENCRYPTION_IV, h264decrypt->iv);
  h264decrypt->iv = NULL;
  G_OBJECT_CLASS(gst_h264_decrypt_parent_class)->finalize(object);
}

static void gst_h264_decrypt_set_property(GObject *object, guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec) {
  GstH264Decrypt *h264decrypt = GST_H264_DECRYPT(object);

  switch (prop_id) {
    case PROP_ENCRYPTION_MODE:
      h264decrypt->encryption_mode = g_value_get_enum(value);
      break;
    case PROP_KEY:
      if (h264decrypt->key) {
        g_boxed_free(GST_TYPE_ENCRYPTION_KEY, h264decrypt->key);
      }
      h264decrypt->key = g_value_dup_boxed(value);
      break;
    case PROP_IV:
      if (h264decrypt->iv) {
        g_boxed_free(GST_TYPE_ENCRYPTION_IV, h264decrypt->iv);
      }
      h264decrypt->iv = g_value_dup_boxed(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_h264_decrypt_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec) {
  GstH264Decrypt *h264decrypt = GST_H264_DECRYPT(object);

  switch (prop_id) {
    case PROP_ENCRYPTION_MODE:
      g_value_set_enum(value, h264decrypt->encryption_mode);
      break;
    case PROP_KEY:
      g_value_set_boxed(value, h264decrypt->key);
      break;
    case PROP_IV:
      g_value_set_boxed(value, h264decrypt->iv);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn gst_h264_decrypt_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *input, GstBuffer **outbuf) {
  // TODO Calculate buffer size better
  gsize input_size = gst_buffer_get_size(input);
  *outbuf = gst_buffer_new_and_alloc(input_size);
  return GST_FLOW_OK;
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
  GstH264SliceHdr slice;
  GstH264ParserResult parse_slice_hdr_result;
  if ((parse_slice_hdr_result = gst_h264_parser_parse_slice_hdr(
           h264decrypt->nalparser, nalu, &slice, TRUE, TRUE)) !=
      GST_H264_PARSER_OK) {
    GST_ERROR_OBJECT(h264decrypt, "Unable to parse slice header! Err: %d",
                     (uint32_t)parse_slice_hdr_result);
    return FALSE;
  }
  const gsize slice_header_size =
      ((slice.header_size - 1) / 8 + 1) + slice.n_emulation_prevention_bytes;
  gsize payload_offset = nalu->offset + nalu->header_bytes + slice_header_size;
  gsize payload_size = nalu->size - nalu->header_bytes - slice_header_size;
  GST_DEBUG_OBJECT(h264decrypt,
                   "Decrypting nal unit of type %d offset %ld size %ld",
                   nalu->type, payload_offset, payload_size);
  if (payload_size % AES_BLOCKLEN != 0) {
    GST_ERROR_OBJECT(h264decrypt,
                     "Encrypted block size is not a multiple of "
                     "AES_BLOCKLEN=%d. Not attempting to decrypt.",
                     AES_BLOCKLEN);
    return FALSE;
  }
  // Decrypt
  switch (h264decrypt->encryption_mode) {
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
    GST_WARNING_OBJECT(h264decrypt, "Padding is not found, data is invalid.");
  }
  // We should overwrite padding bytes
  *dest_offset -= padding_byte_count;
  return TRUE;
}

inline static size_t _copy_memory_bytes(GstMapInfo *dest_map_info,
                                        GstMapInfo *src_map_info,
                                        size_t *dest_offset, size_t src_offset,
                                        size_t size) {
  if (dest_map_info->maxsize < *dest_offset + size) {
    GST_ERROR("Unable to copy as destination is too small");
    return 0;
  }
  memcpy(&dest_map_info->data[*dest_offset], &src_map_info->data[src_offset],
         size);
  *dest_offset += size;
  return size;
}

inline static size_t _copy_nalu_bytes(GstMapInfo *dest_map_info,
                                      GstH264NalUnit *nalu,
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

static GstFlowReturn gst_h264_decrypt_transform(GstBaseTransform *base,
                                                GstBuffer *inbuf,
                                                GstBuffer *outbuf) {
  GstH264Decrypt *h264decrypt = GST_H264_DECRYPT(base);
  GstH264NalUnit nalu;
  GstH264ParserResult result;
  struct AES_ctx ctx;
  GstMapInfo map_info, dest_map_info;

  GST_DEBUG_OBJECT(h264decrypt, "A buffer is received");
  if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(inbuf)))
    gst_object_sync_values(GST_OBJECT(h264decrypt),
                           GST_BUFFER_TIMESTAMP(inbuf));
  if (G_UNLIKELY(!gst_buffer_map(inbuf, &map_info, GST_MAP_READ))) {
    GST_ERROR_OBJECT(base, "Unable to map input buffer for read!");
    return GST_FLOW_ERROR;
  }
  if (G_UNLIKELY(!gst_buffer_map(outbuf, &dest_map_info, GST_MAP_READWRITE))) {
    GST_ERROR_OBJECT(base, "Unable to map output buffer for rw!");
    return GST_FLOW_ERROR;
  }
  if (G_LIKELY(h264decrypt->encryption_mode !=
               GST_H264_ENCRYPTION_MODE_AES_ECB)) {
    if (G_UNLIKELY(!h264decrypt->key || !h264decrypt->iv)) {
      GST_ERROR_OBJECT(base, "Key or IV is not set!");
      goto error;
    }
    AES_init_ctx_iv(&ctx, h264decrypt->key->bytes, h264decrypt->iv->bytes);
  } else {
    if (G_UNLIKELY(!h264decrypt->key)) {
      GST_ERROR_OBJECT(base, "Key is not set!");
      goto error;
    }
    AES_init_ctx(&ctx, h264decrypt->key->bytes);
  }
  size_t dest_offset = 0;
  result = gst_h264_parser_identify_nalu(h264decrypt->nalparser, map_info.data,
                                         0, map_info.size, &nalu);
  while (result == GST_H264_PARSER_OK || result == GST_H264_PARSER_NO_NAL_END) {
    // Encrypts the following NALU types:
    // GST_H264_NAL_SLICE        = 1,
    // GST_H264_NAL_SLICE_DPA    = 2,
    // GST_H264_NAL_SLICE_DPB    = 3,
    // GST_H264_NAL_SLICE_DPC    = 4,
    // GST_H264_NAL_SLICE_IDR    = 5,
    // Need to populate SPS/PPS of nalparser for parsing slice header later
    gst_h264_parser_parse_nal(h264decrypt->nalparser, &nalu);
    if (nalu.type >= GST_H264_NAL_SLICE &&
        nalu.type <= GST_H264_NAL_SLICE_IDR) {
      // Copy the slice into dest
      size_t nalu_total_size;
      if ((nalu_total_size =
               _copy_nalu_bytes(&dest_map_info, &nalu, &dest_offset)) == 0) {
        goto error;
      }
      // Encrypt dest nalu
      GstH264NalUnit dest_nalu;  // NOTE Maybe we can modify nalu instead of
                                 // parsing another one
      GstH264ParserResult parse_result = gst_h264_parser_identify_nalu(
          h264decrypt->nalparser, dest_map_info.data,
          dest_offset - nalu_total_size, dest_offset, &dest_nalu);
      GST_DEBUG_OBJECT(
          h264decrypt,
          "Source nal unit is copied. Type %d sc_offset %d total_size "
          "%d",
          nalu.type, nalu.sc_offset,
          nalu.size + (nalu.offset - nalu.sc_offset));
      GST_DEBUG_OBJECT(
          h264decrypt,
          "Copied nal unit is parsed. Type %d sc_offset %d "
          "total_size %d expected size %ld",
          dest_nalu.type, dest_nalu.sc_offset,
          dest_nalu.size + (dest_nalu.offset - dest_nalu.sc_offset),
          nalu_total_size);
      if (parse_result != GST_H264_PARSER_NO_NAL_END &&
          parse_result != GST_H264_PARSER_OK) {
        GST_ERROR_OBJECT(h264decrypt, "Unable to parse destination nal unit");
        goto error;
      }
      if (!gst_h264_decrypt_decrypt_slice_nalu(h264decrypt, &ctx, &dest_nalu,
                                               &dest_offset)) {
        GST_ERROR_OBJECT(h264decrypt, "Failed to decrypt slice nal unit");
        goto error;
      }
      // TODO Insert emulation three bytes here
    } else {
      // Copy non-slice nal unit
      size_t nalu_total_size = nalu.size + (nalu.offset - nalu.sc_offset);
      if (_copy_memory_bytes(&dest_map_info, &map_info, &dest_offset,
                             nalu.sc_offset, nalu_total_size) == 0) {
        goto error;
      }
    }
    // TODO Fix emulation 3 bytes
    // uint32_t state = 0xffffffff;
    // for (size_t i = 0; i < nalu.size - nalu.header_bytes; i++) {
    //   state = (state << 8) |
    //           (nalu.data[nalu.offset + nalu.header_bytes + i] & 0xff);
    //   switch (state & 0x00ffffff) {
    //     case 0x00000000:
    //     case 0x00000001:
    //     case 0x00000002:
    //     case 0x00000003: {
    //       GST_ERROR_OBJECT(
    //           h264decrypt,
    //           "Emulation prevention byte is found in the encrypted result!");
    //       // goto error;
    //       // Just try to make it work even if we are deliberately corrupting
    //       the
    //       // video. transform_ip does not allow us to change buffer size
    //       // nalu.data[nalu.offset + nalu.header_bytes + i - 1] = 1;
    //       state = 0xffffff00;
    //     }
    //   }
    // }
    result =
        gst_h264_parser_identify_nalu(h264decrypt->nalparser, map_info.data,
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
