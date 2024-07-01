project('gst-template', 'c', version : '1.19.0.1', license : 'GPL')

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')

gst_version = meson.project_version()

api_version = '1.0'

gst_dep = dependency('gstreamer-1.0', version : '>=1.19',
    required : true, fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : '>=1.19',
  fallback : ['gstreamer', 'gst_base_dep'])
# TODO What is fallback?
gstbad_dep = dependency('gstreamer-plugins-bad-1.0', version : '>=1.19',
  fallback : ['gstreamer', 'gst_bad_dep'])
gstcodecparsers_dep = dependency('gstreamer-codecparsers-1.0', version : '>=1.19',
  fallback : ['gstreamer', 'gst_codec_parsers_dep'])

subdir('gst-h264-encryption')
