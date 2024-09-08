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

#include <errno.h> // for: errno, strerror()
#include <gst/base/base.h>
#include <gst/codecparsers/gsth264parser.h>
#include <gst/controller/controller.h>
#include <gst/gst.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include "ciphers/aes.h"
#include "h264_encrypt.h"
#include "h264_encryption_base.h"
#include "h264_encryption_base_private.h"
#include "h264_encryption_mode.h"
#include "h264_encryption_plugin.h"
#include "h264_encryption_types.h"

enum { SIGNAL_IV, SIGNAL_LAST };
enum {
  PROP_IV_SEED = PROP_LAST,  // Extend encryption base props
  ENCRYPT_PROP_LAST
};

GST_DEBUG_CATEGORY_STATIC(gst_h264_encrypt_debug);
#define GST_CAT_DEFAULT gst_h264_encrypt_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-h264,alignment=au,stream-format=byte-stream"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-h264,alignment=au,stream-format=byte-stream"));

#define gst_h264_encrypt_parent_class parent_class
G_DEFINE_TYPE(GstH264Encrypt, gst_h264_encrypt, GST_TYPE_H264_ENCRYPTION_BASE)
GST_ELEMENT_REGISTER_DEFINE(h264encrypt, "h264encrypt", GST_RANK_NONE,
                            GST_TYPE_H264_ENCRYPT)

static GstMemory *gst_h264_encrypt_create_iv_sei_memory(
    guint start_code_prefix_length, const guint8 *iv, guint iv_size);
static GstFlowReturn gst_h264_encrypt_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *input, GstBuffer **outbuf);
static gboolean gst_h264_encrypt_encrypt_slice_nalu(GstH264Encrypt *h264encrypt,
                                                    GstH264NalUnit *nalu,
                                                    GstMapInfo *map_info,
                                                    size_t *dest_offset);
static guint gst_h264_encrypt_signals[SIGNAL_LAST] = {0};
void gst_h264_encrypt_enter_base_transform(
    GstH264EncryptionBase *encryption_base);
gboolean gst_h264_encrypt_before_nalu_copy(
    GstH264EncryptionBase *encryption_base, GstH264NalUnit *src_nalu,
    GstMapInfo *dest_map_info, size_t *dest_offset, gboolean *copy);
gboolean gst_h264_encrypt_process_slice_nalu(
    GstH264EncryptionBase *encryption_base, GstH264NalUnit *dest_nalu,
    GstMapInfo *dest_map_info, size_t *dest_offset);
static void gst_h264_encrypt_set_property(GObject *object, guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec);
static void gst_h264_encrypt_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec);
gboolean gst_h264_encrypt_get_random_iv(GstH264Encrypt *h264encrypt,
                                        uint8_t *iv, guint block_len);
void gst_h264_encrypt_set_random_iv_seed(GstH264Encrypt *h264encrypt,
                                         guint seed);
/* GObject vmethod implementations */

/* initialize the h264encrypt's class */
static void gst_h264_encrypt_class_init(GstH264EncryptClass *klass) {
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstH264EncryptionBaseClass *gsth264encryptionbase_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;
  gsth264encryptionbase_class = (GstH264EncryptionBaseClass *)klass;

  gsth264encryptionbase_class->enter_base_transform =
      gst_h264_encrypt_enter_base_transform;
  gsth264encryptionbase_class->before_nalu_copy =
      gst_h264_encrypt_before_nalu_copy;
  gsth264encryptionbase_class->process_slice_nalu =
      gst_h264_encrypt_process_slice_nalu;
  gobject_class->set_property = gst_h264_encrypt_set_property;
  gobject_class->get_property = gst_h264_encrypt_get_property;

  gst_element_class_set_details_simple(
      gstelement_class, "h264encrypt", "Codec/Encryption/Video",
      "Encrypts H264 streams. You must use h264decrypt to decrypt.",
      "Oguzhan Oztaskin <oguzhanoztaskin@gmail.com>");

  g_object_class_install_property(
      gobject_class, PROP_IV_SEED,
      g_param_spec_uint(
          "iv-seed", "Encryption IV seed",
          "32 bit seed value. Required for CTR/CBC modes. Setting "
          "this takes effect immediately. Make sure to set it to random for "
          "security.",
          0, (guint)-1, RANDOM_IV_SEED_DEFAULT,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PAUSED));

  gst_h264_encrypt_signals[SIGNAL_IV] =
      g_signal_new("iv", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(GstH264EncryptClass, iv), NULL, NULL, NULL,
                   G_TYPE_BOOLEAN, 2, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_NONE);

  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&src_template));
  gst_element_class_add_pad_template(
      gstelement_class, gst_static_pad_template_get(&sink_template));

  GST_BASE_TRANSFORM_CLASS(klass)->prepare_output_buffer =
      GST_DEBUG_FUNCPTR(gst_h264_encrypt_prepare_output_buffer);

  GST_DEBUG_CATEGORY_INIT(gst_h264_encrypt_debug, "h264encrypt", 0,
                          "h264encrypt general logs");
}

static void gst_h264_encrypt_set_property(GObject *object, guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec) {
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(object);
  switch (prop_id) {
    case PROP_IV_SEED: {
      guint seed = g_value_get_uint(value);
      gst_h264_encrypt_set_random_iv_seed(h264encrypt, seed);
    } break;
    default:
      G_OBJECT_CLASS(gst_h264_encrypt_parent_class)
          ->set_property(object, prop_id, value, pspec);
      break;
  }
}

static void gst_h264_encrypt_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec) {
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(object);
  switch (prop_id) {
    case PROP_IV_SEED:
      g_value_set_uint(value, h264encrypt->iv_random_seed);
      break;
    default:
      G_OBJECT_CLASS(gst_h264_encrypt_parent_class)
          ->get_property(object, prop_id, value, pspec);
      break;
  }
}

void gst_h264_encrypt_enter_base_transform(
    GstH264EncryptionBase *encryption_base) {
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(encryption_base);
  h264encrypt->inserted_sei = FALSE;
}

/**
 * Quickly check if this is our SEI
 *
 * NOTE: If IV SEI content size changes, third byte of the signature needs to be
 * changed too
 */
static inline gboolean is_iv_sei(void *sei_payload, size_t payload_size) {
  size_t signature_size = sizeof(GST_H264_ENCRYPT_IV_SEI_SIGNATURE) - 1;
  return payload_size >= signature_size &&
         memcmp(sei_payload, GST_H264_ENCRYPT_IV_SEI_SIGNATURE,
                signature_size) == 0;
}

gboolean gst_h264_encrypt_before_nalu_copy(
    GstH264EncryptionBase *encryption_base, GstH264NalUnit *src_nalu,
    GstMapInfo *dest_map_info, size_t *dest_offset, gboolean *copy) {
  *copy = TRUE;
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(encryption_base);
  if (h264encrypt->inserted_sei == FALSE &&
      (IS_SLICE_NALU(src_nalu->type) ||
       is_iv_sei(&src_nalu->data[src_nalu->offset], src_nalu->size))) {
    // Insert SEI right before the first slice
    // TODO Check if we need emulation three byte insertion
    GstMapInfo memory_map_info;
    GstMemory *sei_memory;
    // Update IV and put it in the SEI
    GstH264EncryptionUtils *utils =
        gst_h264_encryption_base_get_encryption_utils(encryption_base);
    if (!gst_h264_encrypt_get_random_iv(h264encrypt, utils->ctx.Iv,
                                        AES_BLOCKLEN)) {
      return FALSE;
    }
    sei_memory = gst_h264_encrypt_create_iv_sei_memory(
        src_nalu->offset - src_nalu->sc_offset, utils->ctx.Iv, AES_BLOCKLEN);
    if (!gst_memory_map(sei_memory, &memory_map_info, GST_MAP_READ)) {
      GST_ERROR("Unable to map sei memory for read!");
      gst_mini_object_unref(GST_MINI_OBJECT(sei_memory));
      return FALSE;
    }
    if (_copy_memory_bytes(dest_map_info, &memory_map_info, dest_offset, 0,
                           memory_map_info.size) == 0) {
      gst_memory_unmap(sei_memory, &memory_map_info);
      gst_mini_object_unref(GST_MINI_OBJECT(sei_memory));
      return FALSE;
    }
    gst_memory_unmap(sei_memory, &memory_map_info);
    gst_mini_object_unref(GST_MINI_OBJECT(sei_memory));
    h264encrypt->inserted_sei = TRUE;
  }
  return TRUE;
}

gboolean gst_h264_encrypt_process_slice_nalu(
    GstH264EncryptionBase *encryption_base, GstH264NalUnit *dest_nalu,
    GstMapInfo *dest_map_info, size_t *dest_offset) {
  GstH264Encrypt *h264encrypt = GST_H264_ENCRYPT(encryption_base);
  if (!gst_h264_encrypt_encrypt_slice_nalu(h264encrypt, dest_nalu,
                                           dest_map_info, dest_offset)) {
    GST_ERROR_OBJECT(h264encrypt, "Failed to encrypt slice nal unit");
    return FALSE;
  }
  return TRUE;
}

/* initialize the new element
 * initialize instance structure
 */
static void gst_h264_encrypt_init(GstH264Encrypt *h264encrypt) {
  h264encrypt->inserted_sei = FALSE;
  gst_h264_encrypt_set_random_iv_seed(h264encrypt, RANDOM_IV_SEED_DEFAULT);
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn gst_h264_encrypt_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *input, GstBuffer **outbuf) {
  UNUSED(trans);
  // TODO Calculate buffer size better
  gsize input_size = gst_buffer_get_size(input);
  // Also account for SEI, changable AES_BLOCKLEN and emulation bytes
  *outbuf = gst_buffer_new_and_alloc(input_size + 40 + AES_BLOCKLEN + 210);
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
inline static size_t _apply_padding(uint8_t *data, size_t size,
                                    size_t max_size) {
  size_t i;
  size_t padding_byte_count = AES_BLOCKLEN - (size % AES_BLOCKLEN);
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
                                                    GstH264NalUnit *nalu,
                                                    GstMapInfo *map_info,
                                                    size_t *dest_offset) {
  GstH264EncryptionBase *encryption_base =
      GST_H264_ENCRYPTION_BASE(h264encrypt);
  GstH264EncryptionUtils *utils =
      gst_h264_encryption_base_get_encryption_utils(encryption_base);
  // Calculate payload offset and size
  gsize payload_offset, payload_size;
  if (!gst_h264_encryption_base_calculate_payload_offset_and_size(
          encryption_base, utils->nalparser, nalu, &payload_offset,
          &payload_size)) {
    return FALSE;
  }
  GST_DEBUG_OBJECT(encryption_base,
                   "Encrypting nal unit of type %d offset %ld size %ld",
                   nalu->type, payload_offset, payload_size);
  // Apply padding
  size_t padding_byte_count =
      _apply_padding(&nalu->data[payload_offset], payload_size,
                     map_info->maxsize - payload_offset);
  if (G_UNLIKELY(padding_byte_count == 0)) {
    GST_ERROR_OBJECT(h264encrypt, "Not enough space for padding!");
    return FALSE;
  }
  *dest_offset += padding_byte_count;
  payload_size += padding_byte_count;
  // Encrypt
  switch (utils->encryption_mode) {
    case GST_H264_ENCRYPTION_MODE_AES_CTR:
      AES_CTR_xcrypt_buffer(&utils->ctx, &nalu->data[payload_offset],
                            payload_size);
      break;
    case GST_H264_ENCRYPTION_MODE_AES_CBC:
      AES_CBC_encrypt_buffer(&utils->ctx, &nalu->data[payload_offset],
                             payload_size);
      break;
    case GST_H264_ENCRYPTION_MODE_AES_ECB:
      for (size_t i = 0; i < payload_size; i += AES_BLOCKLEN) {
        AES_ECB_encrypt(&utils->ctx, &nalu->data[payload_offset + i]);
      }
      break;
  }
  // Insert emulation prevention bytes
  uint8_t *target = &nalu->data[payload_offset];
  uint8_t *read_copy = (uint8_t *)g_slice_copy(payload_size, target);
  uint32_t state = 0xffffffff;
  size_t i = 0, j = 0;
  for (; i < payload_size && j < map_info->maxsize; i++, j++) {
    state = (state << 8) | (read_copy[i] & 0xff);
    switch (state & 0x00ffffff) {
      // FIXME Do I need to escape these as well?
      case 0x00000000:
      case 0x00000001:
      case 0x00000002:
      case 0x00000003: {
        // Insert emulation prevention byte
        target[j] = 0x03;
        // Let the next round do the copy
        i--;
        state = 0xffffff03;
        break;
      }
      default: {
        // Just copy
        target[j] = read_copy[i];
        break;
      }
    }
  }
  g_slice_free1(payload_size, read_copy);
  if (G_UNLIKELY(i != payload_size)) {
    GST_ERROR_OBJECT(h264encrypt,
                     "Unable to encrypt as there is not enough space for "
                     "emulation prevention bytes");
    return FALSE;
  }
  // Increase offset/size by the amount of added emulation prevention bytes
  *dest_offset += j - i;
  // payload_size += j - i;
  // Add end marker
  if (G_UNLIKELY(j + 1 > map_info->maxsize)) {
    GST_ERROR_OBJECT(h264encrypt,
                     "Unable to encrypt as there is not enough space for "
                     "ciphertext end marker");
    return FALSE;
  }
  target[j] = CIPHERTEXT_END_MARKER;
  (*dest_offset)++;
  return TRUE;
}

gboolean gst_h264_encrypt_get_random_iv(GstH264Encrypt *h264encrypt,
                                        uint8_t *iv, guint block_len) {
  gboolean ret;
  // Try to get application provided IV
  g_signal_emit(h264encrypt, gst_h264_encrypt_signals[SIGNAL_IV], 0, iv,
                AES_BLOCKLEN, &ret);
  if (!ret) {
    GST_LOG_OBJECT(h264encrypt, "No IV is provided, using random IV.");

    // Generate random IV using random_r
    for (guint i = 0; i < block_len;) {
      int32_t rand_value;
      if (random_r(&h264encrypt->iv_random_data, &rand_value) != 0) {
        GST_ERROR_OBJECT(h264encrypt, "Failed generate random IV: %s",
                         strerror(errno));
        return FALSE;
      }
      int size = sizeof(rand_value) <= (block_len - i) ? sizeof(rand_value)
                                                       : block_len - i;
      memcpy(&iv[i], &rand_value, size);
      i += size;
    }
  } else {
    GST_LOG_OBJECT(h264encrypt, "Using IV provided by IV signal.");
  }
  return TRUE;
}

void gst_h264_encrypt_set_random_iv_seed(GstH264Encrypt *h264encrypt,
                                         guint seed) {
  GST_INFO_OBJECT(h264encrypt, "Setting random seed.");
  if (initstate_r(seed, h264encrypt->iv_random_state_buf,
                  sizeof(h264encrypt->iv_random_state_buf),
                  &h264encrypt->iv_random_data) != 0) {
    GST_ERROR_OBJECT(h264encrypt, "Unable to set random seed: %s",
                     strerror(errno));
  } else {
    h264encrypt->iv_random_seed = seed;
  }
}
