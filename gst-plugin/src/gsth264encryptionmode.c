#include "gsth264encryptionmode.h"

#include <gst/gst.h>

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