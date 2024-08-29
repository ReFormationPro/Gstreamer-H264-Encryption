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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>

#include "h264_decrypt.h"
#include "h264_encrypt.h"
#include "h264_encryption_plugin.h"

// GST_DEBUG_CATEGORY_STATIC(gst_plugin_template_debug);
// #define GST_CAT_DEFAULT gst_plugin_template_debug

/* PACKAGE: this is usually set by meson depending on some _INIT macro
 * in meson.build and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use meson to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "h264encryption"
#endif

GST_DEBUG_CATEGORY(GST_H264_ENCRYPTION);

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean h264encryption_init(GstPlugin *h264encryption) {
  GST_DEBUG_CATEGORY_INIT(GST_H264_ENCRYPTION, "GST_H264_ENCRYPTION", 0,
                          "GstH264Encryption general logs");
  gboolean result = GST_ELEMENT_REGISTER(h264decrypt, h264encryption);
  return result & GST_ELEMENT_REGISTER(h264encrypt, h264encryption);
}

/*
 * gstreamer looks for this structure to register plugins
 */
GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, h264encryption,
                  "Experimental H264 video encryption plugin",
                  h264encryption_init, PACKAGE_VERSION, GST_LICENSE,
                  GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
