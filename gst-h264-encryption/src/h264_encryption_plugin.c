/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) YEAR AUTHOR_NAME AUTHOR_EMAIL
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/* gstreamer looks for this structure to register plugins
 *
 * FIXME:exchange the string 'Template h264encryption' with your plugin
 * description
 */
GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, h264encryption,
                  "Experimental H264 video encryption plugin",
                  h264encryption_init, PACKAGE_VERSION, GST_LICENSE,
                  GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
