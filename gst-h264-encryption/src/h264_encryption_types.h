#ifndef __GST_H264ENCRYPTION_TYPES_H__
#define __GST_H264ENCRYPTION_TYPES_H__

#include <gst/gst.h>

#include "ciphers/aes.h"

G_BEGIN_DECLS

#define DECLARE_GST_ENCRYPTION_STRUCT(struct_name, function_name_prefix, \
                                      key_size)                          \
  typedef struct struct_name {                                           \
    uint8_t bytes[key_size];                                             \
  } struct_name;                                                         \
                                                                         \
  GST_EXPORT GType function_name_prefix##_##get_type(void) G_GNUC_CONST; \
                                                                         \
  GST_EXPORT struct_name *function_name_prefix##_##new (void);           \
  GST_EXPORT void function_name_prefix##_##free(struct_name *self);      \
  GST_EXPORT struct_name *function_name_prefix##_##copy(struct_name *self);

DECLARE_GST_ENCRYPTION_STRUCT(GstEncryptionKey, gst_encryption_key, AES_KEYLEN)
#define GST_TYPE_ENCRYPTION_KEY (gst_encryption_key_get_type())

DECLARE_GST_ENCRYPTION_STRUCT(GstEncryptionIV, gst_encryption_iv, AES_BLOCKLEN)
#define GST_TYPE_ENCRYPTION_IV (gst_encryption_iv_get_type())

G_END_DECLS

#endif /* __GST_H264ENCRYPTION_TYPES_H__ */