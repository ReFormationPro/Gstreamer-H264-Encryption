plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-h264-encryption')
cdata.set_quoted('GST_LICENSE', 'GPL')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'Gstreamer H264 Encryption')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://github.com/ReFormationPro/Gstreamer-H264-Encryption')
configure_file(output : 'config.h', configuration : cdata)

# The h264encryption Plugin
gsth264encryption_sources = [
  'src/h264_encrypt.c',
  'src/h264_decrypt.c',
  'src/h264_encryption_base.c',
  'src/ciphers/aes.c',
  'src/h264_encryption_plugin.c',
  'src/h264_encryption_mode.c',
  'src/h264_encryption_types.c',
]

gsth264encryption = library('gsth264encryption',
  gsth264encryption_sources,
  c_args: plugin_c_args + ['-DGST_USE_UNSTABLE_API'],
  dependencies : [gst_dep, gstbase_dep, gstcodecparsers_dep],
  install : true,
  install_dir : plugins_install_dir,
)
