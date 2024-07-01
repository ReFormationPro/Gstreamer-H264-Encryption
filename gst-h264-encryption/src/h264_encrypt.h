/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2020 Niels De Graef <niels.degraef@gmail.com>
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

#ifndef __GST_H264ENCRYPT_H__
#define __GST_H264ENCRYPT_H__

#include <gst/base/gstbasetransform.h>
#include <gst/codecparsers/gsth264parser.h>
#include <gst/gst.h>
#include <stdlib.h>

#include "ciphers/aes.h"
#include "h264_encryption_base.h"
#include "h264_encryption_mode.h"
#include "h264_encryption_types.h"

G_BEGIN_DECLS

#define RANDOM_IV_SEED_DEFAULT 1869052520

/*
 * Declares type structure as with derivable/final declares but does not define
 * class or instance structures.
 * See G_DECLARE_FINAL_TYPE and/or G_DECLARE_FINAL_TYPE for details.
 */
#define G_DECLARE_TYPE_STRUCTURES(ModuleObjName, module_obj_name, MODULE,     \
                                  OBJ_NAME, ParentName)                       \
  GType module_obj_name##_get_type(void);                                     \
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                            \
  typedef struct _##ModuleObjName ModuleObjName;                              \
  typedef struct _##ModuleObjName##Class ModuleObjName##Class;                \
                                                                              \
  _GLIB_DEFINE_AUTOPTR_CHAINUP(ModuleObjName, ParentName)                     \
  G_DEFINE_AUTOPTR_CLEANUP_FUNC(ModuleObjName##Class, g_type_class_unref)     \
                                                                              \
  G_GNUC_UNUSED static inline ModuleObjName *MODULE##_##OBJ_NAME(             \
      gpointer ptr) {                                                         \
    return G_TYPE_CHECK_INSTANCE_CAST(ptr, module_obj_name##_get_type(),      \
                                      ModuleObjName);                         \
  }                                                                           \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME(gpointer ptr) { \
    return G_TYPE_CHECK_INSTANCE_TYPE(ptr, module_obj_name##_get_type());     \
  }                                                                           \
  G_GNUC_END_IGNORE_DEPRECATIONS

GST_ELEMENT_REGISTER_DECLARE(h264encrypt);
#define GST_TYPE_H264_ENCRYPT (gst_h264_encrypt_get_type())
G_DECLARE_TYPE_STRUCTURES(GstH264Encrypt, gst_h264_encrypt, GST, H264_ENCRYPT,
                          GstH264EncryptionBase)

struct _GstH264EncryptClass {
  GstH264EncryptionBaseClass parent_class;

  /* Signals */
  /* NOTE For performance, these can be replaced with callbacks, ie.
   * gst_app_sink_set_callbacks.
   */
  gboolean (*iv)(GstH264Encrypt *encrypt, uint8_t *iv, guint block_length);

  gpointer padding[12];
};

// GstH264Encrypt *gst_h264_encrypt_new(void);

struct _GstH264Encrypt {
  GstH264EncryptionBase encryption_base;

  gboolean inserted_sei;
  // randomness data
  char iv_random_state_buf[128];
  struct random_data iv_random_data;
  guint iv_random_seed;
};

G_END_DECLS

#endif /* __GST_H264ENCRYPT_H__ */
