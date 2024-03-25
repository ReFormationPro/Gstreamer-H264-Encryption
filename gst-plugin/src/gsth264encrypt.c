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
#include "gsth264encryptionbase.h"
#include "gsth264encryptionbaseprivate.h"
#include "gsth264encryptionmode.h"
#include "gsth264encryptionplugin.h"
#include "gsth264encryptiontypes.h"

GST_DEBUG_CATEGORY_STATIC(gst_h264_encrypt_debug);
#define GST_CAT_DEFAULT gst_h264_encrypt_debug
#define DEFAULT_ENCRYPTION_MODE GST_H264_ENCRYPTION_MODE_AES_CTR

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-h264,alignment=au,stream-format=byte-stream"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-h264,alignment=au,stream-format=byte-stream"));

#define gst_h264_encrypt_parent_class parent_class
G_DEFINE_TYPE(GstH264Encrypt, gst_h264_encrypt, GST_TYPE_H264_ENCRYPTION_BASE);
GST_ELEMENT_REGISTER_DEFINE(h264encrypt, "h264encrypt", GST_RANK_NONE,
                            GST_TYPE_H264_ENCRYPT);

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

  gst_element_class_set_details_simple(
      gstelement_class, "h264encrypt", "Codec/Encryptor/Video",
      "H264 Video Encryptor", "Oguzhan Oztaskin <oguzhanoztaskin@gmail.com>");

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
static void gst_h264_encrypt_init(GstH264Encrypt *h264encrypt) {}

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

/**
 * Returns the number of bytes used for padding.
 *
 * If returns 0, max size was less than required bytes and nothing is written.
 */
inline static gint _apply_padding(uint8_t *data, size_t size, size_t max_size) {
  int i;
  gint padding_byte_count = AES_BLOCKLEN - (size % AES_BLOCKLEN);
  if (padding_byte_count + size >= max_size) {
    return 0;
  }
  data[size++] = 0x80;
  for (i = 1; i < padding_byte_count; i++) {
    data[size++] = 0;
  }
  return i;
}

static gboolean gst_h264_encrypt_encrypt_slice_nalu(GstH264Encrypt *h264encrypt,
                                                    struct AES_ctx *ctx,
                                                    GstH264NalUnit *nalu,
                                                    GstMapInfo *map_info,
                                                    size_t *dest_offset) {
  // TODO Extract this into a function
  GstH264SliceHdr slice;
  GstH264ParserResult parse_slice_hdr_result;
  GstH264EncryptionBase *encryption_base =
      GST_H264_ENCRYPTION_BASE(h264encrypt);
  if ((parse_slice_hdr_result = gst_h264_parser_parse_slice_hdr(
           encryption_base->nalparser, nalu, &slice, TRUE, TRUE)) !=
      GST_H264_PARSER_OK) {
    GST_ERROR_OBJECT(h264encrypt, "Unable to parse slice header! Err: %d",
                     (uint32_t)parse_slice_hdr_result);
    return FALSE;
  }
  const gsize slice_header_size =
      ((slice.header_size - 1) / 8 + 1) + slice.n_emulation_prevention_bytes;
  gsize payload_offset = nalu->offset + nalu->header_bytes + slice_header_size;
  gsize payload_size = nalu->size - nalu->header_bytes - slice_header_size;
  GST_DEBUG_OBJECT(h264encrypt,
                   "Encrypting nal unit of type %d offset %ld size %ld",
                   nalu->type, payload_offset, payload_size);
  // Apply padding
  int padding_byte_count =
      _apply_padding(nalu->data, payload_size, map_info->maxsize);
  if (G_UNLIKELY(padding_byte_count == 0)) {
    GST_ERROR_OBJECT(h264encrypt, "Not enough space for padding!");
    return FALSE;
  }
  *dest_offset += padding_byte_count;
  payload_size += padding_byte_count;
  // Encrypt
  switch (encryption_base->encryption_mode) {
    case GST_H264_ENCRYPTION_MODE_AES_CTR:
      AES_CTR_xcrypt_buffer(ctx, &nalu->data[payload_offset], payload_size);
      break;
    case GST_H264_ENCRYPTION_MODE_AES_CBC:
      AES_CBC_encrypt_buffer(ctx, &nalu->data[payload_offset], payload_size);
      break;
    case GST_H264_ENCRYPTION_MODE_AES_ECB:
      for (size_t i = 0; i < payload_size; i += AES_BLOCKLEN) {
        AES_ECB_encrypt(ctx, &nalu->data[payload_offset + i]);
      }
      break;
  }
  return TRUE;
}

static GstFlowReturn gst_h264_encrypt_transform(GstBaseTransform *base,
                                                GstBuffer *inbuf,
                                                GstBuffer *outbuf) {
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(base);
  GstH264EncryptionBase *encryption_base =
      GST_H264_ENCRYPTION_BASE(h264encrypt);
  GstH264NalUnit nalu;
  GstH264ParserResult result;
  struct AES_ctx ctx;
  GstMapInfo map_info, dest_map_info;

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
  if (G_LIKELY(encryption_base->encryption_mode !=
               GST_H264_ENCRYPTION_MODE_AES_ECB)) {
    if (G_UNLIKELY(!encryption_base->key || !encryption_base->iv)) {
      GST_ERROR_OBJECT(base, "Key or IV is not set!");
      goto error;
    }
    AES_init_ctx_iv(&ctx, encryption_base->key->bytes,
                    encryption_base->iv->bytes);
  } else {
    if (G_UNLIKELY(!encryption_base->key)) {
      GST_ERROR_OBJECT(base, "Key is not set!");
      goto error;
    }
    AES_init_ctx(&ctx, encryption_base->key->bytes);
  }
  size_t dest_offset = 0;
  gboolean inserted_sei = FALSE;
  result = gst_h264_parser_identify_nalu(
      encryption_base->nalparser, map_info.data, 0, map_info.size, &nalu);
  while (result == GST_H264_PARSER_OK || result == GST_H264_PARSER_NO_NAL_END) {
    // Encrypts the following NALU types:
    // GST_H264_NAL_SLICE        = 1,
    // GST_H264_NAL_SLICE_DPA    = 2,
    // GST_H264_NAL_SLICE_DPB    = 3,
    // GST_H264_NAL_SLICE_DPC    = 4,
    // GST_H264_NAL_SLICE_IDR    = 5,
    // Need to populate SPS/PPS of nalparser for parsing slice header later
    gst_h264_parser_parse_nal(encryption_base->nalparser, &nalu);
    if (nalu.type >= GST_H264_NAL_SLICE &&
        nalu.type <= GST_H264_NAL_SLICE_IDR) {
      if (inserted_sei == FALSE) {
        // FIXME This somehow causes artifacts
        // Insert SEI right before the first slice
        // TODO Check if we need emulation three byte insertion
        GstMapInfo memory_map_info;
        GstMemory *sei_memory = gst_h264_encrypt_create_iv_sei_memory(
            nalu.offset - nalu.sc_offset, encryption_base->iv->bytes,
            AES_BLOCKLEN);
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
      // Encrypt dest nalu
      GstH264NalUnit dest_nalu;  // NOTE Maybe we can modify nalu instead of
                                 // parsing another one
      GstH264ParserResult parse_result = gst_h264_parser_identify_nalu(
          encryption_base->nalparser, dest_map_info.data,
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
      if (!gst_h264_encrypt_encrypt_slice_nalu(h264encrypt, &ctx, &dest_nalu,
                                               &dest_map_info, &dest_offset)) {
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
    result =
        gst_h264_parser_identify_nalu(encryption_base->nalparser, map_info.data,
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
