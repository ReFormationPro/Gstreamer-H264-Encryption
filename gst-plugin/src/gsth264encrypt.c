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
 * SECTION:element-h264encrypt
 *
 * FIXME:Describe h264encrypt here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! h264encrypt ! fakesink silent=TRUE
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
#include "gsth264encrypt.h"
#include "gsth264encryptionmode.h"
#include "gsth264encryptionplugin.h"
#include "gsth264encryptiontypes.h"

GST_DEBUG_CATEGORY_STATIC(gst_h264_encrypt_debug);
#define GST_CAT_DEFAULT gst_h264_encrypt_debug
#define DEFAULT_ENCRYPTION_MODE GST_H264_ENCRYPTION_MODE_AES_CTR

/* h264encrypt signals and args */
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

#define gst_h264_encrypt_parent_class parent_class
G_DEFINE_TYPE(GstH264Encrypt, gst_h264_encrypt, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE(h264encrypt, "h264encrypt", GST_RANK_NONE,
                            GST_TYPE_H264_ENCRYPT);

static void gst_h264_encrypt_set_property(GObject *object, guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec);
static void gst_h264_encrypt_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec);
static void gst_h264_encrypt_dispose(GObject *object);
static void gst_h264_encrypt_finalize(GObject *object);

static GstFlowReturn gst_h264_encrypt_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *input, GstBuffer **outbuf);
static GstFlowReturn gst_h264_encrypt_transform(GstBaseTransform *base,
                                                GstBuffer *inbuf,
                                                GstBuffer *outbuf);

/* GObject vmethod implementations */

/* initialize the h264encrypt's class */
static void gst_h264_encrypt_class_init(GstH264EncryptClass *klass) {
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;

  gobject_class->set_property = gst_h264_encrypt_set_property;
  gobject_class->get_property = gst_h264_encrypt_get_property;
  gobject_class->dispose = gst_h264_encrypt_dispose;
  gobject_class->finalize = gst_h264_encrypt_finalize;

  gst_element_class_set_details_simple(
      gstelement_class, "h264encrypt", "Codec/Encryptor/Video",
      "H264 Video Encryptor", "Oguzhan Oztaskin <oguzhanoztaskin@gmail.com>");
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
      GST_DEBUG_FUNCPTR(gst_h264_encrypt_transform);
  GST_BASE_TRANSFORM_CLASS(klass)->prepare_output_buffer =
      GST_DEBUG_FUNCPTR(gst_h264_encrypt_prepare_output_buffer);

  GST_DEBUG_CATEGORY_INIT(gst_h264_encrypt_debug, "h264encrypt", 0,
                          "h264encrypt general logs");

  gst_type_mark_as_plugin_api(GST_TYPE_H264_ENCRYPTION_MODE, 0);
}

/* initialize the new element
 * initialize instance structure
 */
static void gst_h264_encrypt_init(GstH264Encrypt *h264encrypt) {
  h264encrypt->nalparser = gst_h264_nal_parser_new();
  h264encrypt->encryption_mode = DEFAULT_ENCRYPTION_MODE;
  h264encrypt->key = NULL;
  h264encrypt->iv = NULL;
}

static void gst_h264_encrypt_dispose(GObject *object) {
  G_OBJECT_CLASS(gst_h264_encrypt_parent_class)->dispose(object);
}

static void gst_h264_encrypt_finalize(GObject *object) {
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(object);
  gst_h264_nal_parser_free(h264encrypt->nalparser);
  h264encrypt->nalparser = NULL;
  // FIXME need to use
  if (h264encrypt->key) g_boxed_free(GST_TYPE_ENCRYPTION_KEY, h264encrypt->key);
  h264encrypt->key = NULL;
  if (h264encrypt->iv) g_boxed_free(GST_TYPE_ENCRYPTION_IV, h264encrypt->iv);
  h264encrypt->iv = NULL;
  G_OBJECT_CLASS(gst_h264_encrypt_parent_class)->finalize(object);
}

static void gst_h264_encrypt_set_property(GObject *object, guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec) {
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(object);

  switch (prop_id) {
    case PROP_ENCRYPTION_MODE:
      h264encrypt->encryption_mode = g_value_get_enum(value);
      break;
    case PROP_KEY:
      if (h264encrypt->key) {
        g_boxed_free(GST_TYPE_ENCRYPTION_KEY, h264encrypt->key);
      }
      h264encrypt->key = g_value_dup_boxed(value);
      break;
    case PROP_IV:
      if (h264encrypt->iv) {
        g_boxed_free(GST_TYPE_ENCRYPTION_IV, h264encrypt->iv);
      }
      h264encrypt->iv = g_value_dup_boxed(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_h264_encrypt_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec) {
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(object);

  switch (prop_id) {
    case PROP_ENCRYPTION_MODE:
      g_value_set_enum(value, h264encrypt->encryption_mode);
      break;
    case PROP_KEY:
      g_value_set_boxed(value, h264encrypt->key);
      break;
    case PROP_IV:
      g_value_set_boxed(value, h264encrypt->iv);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn gst_h264_encrypt_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *input, GstBuffer **outbuf) {
  // TODO Calculate buffer size better
  gsize input_size = gst_buffer_get_size(input);
  // Also account for SEI, changable AES_BLOCKLEN and emulation bytes
  *outbuf = gst_buffer_new_and_alloc(input_size + 40 + AES_BLOCKLEN + 30);
  return GST_FLOW_OK;
}

static GstMemory *gst_h264_encrypt_create_iv_sei_memory(
    guint start_code_prefix_length, const guint8 *iv, guint iv_size) {
  GArray *messages =
      g_array_sized_new(FALSE, FALSE, sizeof(GstH264SEIMessage), 1);
  messages->len = 1;
  GstH264SEIMessage *sei_message =
      &g_array_index(messages, GstH264SEIMessage, 0);
  sei_message->payloadType = GST_H264_SEI_USER_DATA_UNREGISTERED;
  GstH264UserDataUnregistered *udu =
      &sei_message->payload.user_data_unregistered;
  memcpy(udu->uuid, GST_H264_ENCRYPT_IV_SEI_UUID, sizeof(udu->uuid));
  udu->data = iv;
  udu->size = iv_size;
  // TODO avc/byte-stream
  GstMemory *sei_memory =
      gst_h264_create_sei_memory(start_code_prefix_length, messages);
  g_array_free(messages, TRUE);
  return sei_memory;
}

static gboolean gst_h264_encrypt_encrypt_slice_nalu(GstH264Encrypt *h264encrypt,
                                                    struct AES_ctx *ctx,
                                                    GstH264NalUnit *nalu) {
  GstH264SliceHdr slice;
  GstH264ParserResult parse_slice_hdr_result;
  if ((parse_slice_hdr_result = gst_h264_parser_parse_slice_hdr(
           h264encrypt->nalparser, nalu, &slice, TRUE, TRUE)) !=
      GST_H264_PARSER_OK) {
    GST_ERROR_OBJECT(h264encrypt, "Unable to parse slice header! Err: %d",
                     (uint32_t)parse_slice_hdr_result);
    return FALSE;
  }
  const gsize slice_header_size =
      ((slice.header_size - 1) / 8 + 1) + slice.n_emulation_prevention_bytes;
  gsize payload_offset = nalu->offset + nalu->header_bytes + slice_header_size;
  const gsize payload_size =
      nalu->size - nalu->header_bytes - slice_header_size;
  GST_DEBUG_OBJECT(h264encrypt,
                   "Encrypting nal unit of type %d offset %ld size %ld",
                   nalu->type, payload_offset, payload_size);
  switch (h264encrypt->encryption_mode) {
    case GST_H264_ENCRYPTION_MODE_AES_CTR:
      AES_CTR_xcrypt_buffer(ctx, &nalu->data[payload_offset], payload_size);
      break;
    case GST_H264_ENCRYPTION_MODE_AES_CBC:
      // FIXME This causes chrashes because CBC encrypts blockwise and
      // causes buffer overflows
      AES_CBC_encrypt_buffer(ctx, &nalu->data[payload_offset], payload_size);
      break;
    case GST_H264_ENCRYPTION_MODE_AES_ECB:
      // FIXME Last block has to be of size AES_BLOCKLEN too
      // Here, we instead just do not encrypt the last block.
      // We should be using padding here instead
      for (size_t i = 0; i < payload_size - AES_BLOCKLEN; i += AES_BLOCKLEN) {
        AES_ECB_encrypt(ctx, &nalu->data[payload_offset + i]);
      }
      break;
    case GST_H264_ENCRYPTION_MODE_AES_ECB_DECRYPT: {
      // FIXME Same as ecb encryption
      for (size_t i = 0; i < payload_size - AES_BLOCKLEN; i += AES_BLOCKLEN) {
        AES_ECB_decrypt(ctx, &nalu->data[payload_offset + i]);
      }
      break;
    }
    case GST_H264_ENCRYPTION_MODE_AES_CBC_DECRYPT: {
      // FIXME Same as cbc encryption
      AES_CBC_decrypt_buffer(ctx, &nalu->data[payload_offset], payload_size);
      break;
    }
  }
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

static GstFlowReturn gst_h264_encrypt_transform(GstBaseTransform *base,
                                                GstBuffer *inbuf,
                                                GstBuffer *outbuf) {
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(base);
  GstH264NalUnit nalu;
  GstH264ParserResult result;
  struct AES_ctx ctx;
  GstMapInfo map_info, dest_map_info;

  /**
   * TODO
   * 1- Copy all NAL units that precede SEI
   * 2- Create SEI Memory for unregistered user data (FIXME Are multiple SEI
   * NALUs allowed?)
   * 3- Put SEI Memory into outbuf
   *
   * For each SLICE NAL unit:
   * 4- Calculate encrypted NALu size:
   *    - Padding will increase byte size
   *    - Account for a fixed number of emulation prevention bytes
   * 5- Encrypt NAL payload
   * 6- Insert emulation bytes (TODO Maybe make 5-6 in one pass?)
   * 7- Copy the padded, encrytped, emulation prevention byte inserted memory
   */

  /**
   * TODO Easier
   * 1- Find the first SLICE
   * 2- Insert memory before and SEI
   * 3- Calculate memory for the rest of the bytes (so complete nal parsing)
   * 4- Copy SLICE into output, pad and encrypt and insert emulation prevention
   * bytes there 5- Finish
   */
  GST_DEBUG_OBJECT(h264encrypt, "A buffer is received");
  if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(inbuf)))
    gst_object_sync_values(GST_OBJECT(h264encrypt),
                           GST_BUFFER_TIMESTAMP(inbuf));
  if (G_UNLIKELY(!gst_buffer_map(inbuf, &map_info, GST_MAP_READ))) {
    GST_ERROR_OBJECT(base, "Unable to map input buffer for read!");
    return GST_FLOW_ERROR;
  }
  if (G_UNLIKELY(!gst_buffer_map(outbuf, &dest_map_info, GST_MAP_READWRITE))) {
    GST_ERROR_OBJECT(base, "Unable to map output buffer for rw!");
    return GST_FLOW_ERROR;
  }
  if (G_LIKELY(h264encrypt->encryption_mode !=
               GST_H264_ENCRYPTION_MODE_AES_ECB)) {
    if (G_UNLIKELY(!h264encrypt->key || !h264encrypt->iv)) {
      GST_ERROR_OBJECT(base, "Key or IV is not set!");
      goto error;
    }
    AES_init_ctx_iv(&ctx, h264encrypt->key->bytes, h264encrypt->iv->bytes);
  } else {
    if (G_UNLIKELY(!h264encrypt->key)) {
      GST_ERROR_OBJECT(base, "Key is not set!");
      goto error;
    }
    AES_init_ctx(&ctx, h264encrypt->key->bytes);
  }
  size_t dest_offset = 0;
  gboolean inserted_sei = FALSE;
  result = gst_h264_parser_identify_nalu(h264encrypt->nalparser, map_info.data,
                                         0, map_info.size, &nalu);
  while (result == GST_H264_PARSER_OK || result == GST_H264_PARSER_NO_NAL_END) {
    // Encrypts the following NALU types:
    // GST_H264_NAL_SLICE        = 1,
    // GST_H264_NAL_SLICE_DPA    = 2,
    // GST_H264_NAL_SLICE_DPB    = 3,
    // GST_H264_NAL_SLICE_DPC    = 4,
    // GST_H264_NAL_SLICE_IDR    = 5,
    // Need to populate SPS/PPS of nalparser for parsing slice header later
    gst_h264_parser_parse_nal(h264encrypt->nalparser, &nalu);
    if (nalu.type >= GST_H264_NAL_SLICE &&
        nalu.type <= GST_H264_NAL_SLICE_IDR) {
      if (inserted_sei == FALSE) {
        // FIXME This somehow causes artifacts
        // Insert SEI right before the first slice
        // TODO Check if we need emulation three byte insertion
        GstMapInfo memory_map_info;
        GstMemory *sei_memory = gst_h264_encrypt_create_iv_sei_memory(
            nalu.offset - nalu.sc_offset, h264encrypt->iv->bytes, AES_BLOCKLEN);
        if (!gst_memory_map(sei_memory, &memory_map_info, GST_MAP_READ)) {
          GST_ERROR("Unable to map sei memory for read!");
          gst_mini_object_unref(GST_MINI_OBJECT(sei_memory));
          goto error;
        }
        if (_copy_memory_bytes(&dest_map_info, &memory_map_info, &dest_offset,
                               0, memory_map_info.size) == 0) {
          gst_memory_unmap(sei_memory, &memory_map_info);
          gst_mini_object_unref(GST_MINI_OBJECT(sei_memory));
          goto error;
        }
        gst_memory_unmap(sei_memory, &memory_map_info);
        gst_mini_object_unref(GST_MINI_OBJECT(sei_memory));
        inserted_sei = TRUE;
      }
      // Copy the slice into dest
      size_t nalu_total_size;
      if ((nalu_total_size =
               _copy_nalu_bytes(&dest_map_info, &nalu, &dest_offset)) == 0) {
        goto error;
      }
      // TODO Apply padding
      // Encrypt dest nalu
      GstH264NalUnit dest_nalu;  // NOTE Maybe we can modify nalu instead of
                                 // parsing another one
      GstH264ParserResult parse_result = gst_h264_parser_identify_nalu(
          h264encrypt->nalparser, dest_map_info.data,
          dest_offset - nalu_total_size, dest_offset, &dest_nalu);
      GST_DEBUG_OBJECT(
          h264encrypt,
          "Source nal unit is copied. Type %d sc_offset %d total_size "
          "%d",
          nalu.type, nalu.sc_offset,
          nalu.size + (nalu.offset - nalu.sc_offset));
      GST_DEBUG_OBJECT(
          h264encrypt,
          "Copied nal unit is parsed. Type %d sc_offset %d "
          "total_size %d expected size %ld",
          dest_nalu.type, dest_nalu.sc_offset,
          dest_nalu.size + (dest_nalu.offset - dest_nalu.sc_offset),
          nalu_total_size);
      if (parse_result != GST_H264_PARSER_NO_NAL_END &&
          parse_result != GST_H264_PARSER_OK) {
        GST_ERROR_OBJECT(h264encrypt, "Unable to parse destination nal unit");
        goto error;
      }
      if (!gst_h264_encrypt_encrypt_slice_nalu(h264encrypt, &ctx, &dest_nalu)) {
        GST_ERROR_OBJECT(h264encrypt, "Failed to encrypt slice nal unit");
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
    //           h264encrypt,
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
        gst_h264_parser_identify_nalu(h264encrypt->nalparser, map_info.data,
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
