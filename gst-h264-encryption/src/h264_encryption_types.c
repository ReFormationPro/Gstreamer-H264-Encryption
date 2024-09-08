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

#include "h264_encryption_types.h"

#include <stdio.h>  // for: sscanf()
#include <string.h> // for: memcpy()

GST_DEBUG_CATEGORY_EXTERN(GST_H264_ENCRYPTION);
#define GST_CAT_DEFAULT GST_H264_ENCRYPTION

/**
 * Adapted from
 * https://stackoverflow.com/questions/3408706/hexadecimal-string-to-byte-array-in-c
 */
static gboolean hex2bytes(const char *str, uint8_t *bytes, size_t byte_count) {
  if (G_UNLIKELY(str == NULL)) {
    GST_ERROR("Input string cannot be NULL");
    return FALSE;
  }
  if (G_UNLIKELY(strlen(str) != byte_count * 2)) {
    GST_ERROR("Expected %ld characters in the input string, found %ld",
              byte_count * 2, strlen(str));
    return FALSE;
  }
  for (size_t i = 0; i < byte_count; i++) {
    sscanf(str, "%2hhx", &bytes[i]);
    str += 2;
  }
  return TRUE;
}

#define DEFINE_GST_ENCRYPTION_STRUCT(struct_name, function_name_prefix)       \
  struct_name *function_name_prefix##_##new (void) {                          \
    return g_new(struct_name, 1);                                             \
  }                                                                           \
  void function_name_prefix##_##free(struct_name *self) { g_free(self); }     \
  struct_name *function_name_prefix##_##copy(struct_name *self) {             \
    struct_name *copy = function_name_prefix##_##new ();                      \
    memcpy(copy->bytes, self->bytes, sizeof(self->bytes));                    \
    return copy;                                                              \
  }                                                                           \
  static gboolean function_name_prefix##_##deserialize(GValue *dest,          \
                                                       const gchar *s) {      \
    struct_name *boxed_obj;                                                   \
    boxed_obj = function_name_prefix##_##new ();                              \
    if (!hex2bytes(s, boxed_obj->bytes, sizeof(boxed_obj->bytes))) {          \
      GST_ERROR("Failed to convert string '%s' to " G_STRINGIFY(struct_name), \
                s);                                                           \
      return FALSE;                                                           \
    }                                                                         \
    g_value_take_boxed(dest, (gconstpointer)boxed_obj);                       \
    return TRUE;                                                              \
  }                                                                           \
  static void register_##struct_name##_deserialization_func(GType type) {     \
    static GstValueTable table = {                                            \
        G_TYPE_NONE,                                                          \
        NULL,                                                                 \
        NULL,                                                                 \
        (GstValueDeserializeFunc)(function_name_prefix##_##deserialize),      \
        (GstValueDeserializeWithPSpecFunc)NULL,                               \
        {NULL}};                                                              \
    table.type = type;                                                        \
    gst_value_register(&table);                                               \
  }                                                                           \
  G_DEFINE_BOXED_TYPE_WITH_CODE(                                              \
      struct_name, function_name_prefix,                                      \
      (GBoxedCopyFunc)function_name_prefix##_##copy,                          \
      function_name_prefix##_##free,                                          \
      register_##struct_name##_deserialization_func(g_define_type_id))

DEFINE_GST_ENCRYPTION_STRUCT(GstEncryptionKey, gst_encryption_key)
DEFINE_GST_ENCRYPTION_STRUCT(GstEncryptionIV, gst_encryption_iv)
