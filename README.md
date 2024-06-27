# GStreamer H264 Encryption Plugin
This is an experimental plugin designed to offer GStreamer elements for H.264 encryption/decryption while preserving the H.264 structure with nalu and slice headers intact.
The goal is to enable the playback of the encrypted stream even if properties such as stream-format or alignment are altered.
This capability allows secure video streaming over potentially insecure channels or storage in the MP4 format, with the added advantage of recoverability in the face of packet loss, common in UDP streams.

Note that this solution requires both encrypting and decrypting sides to be using this plugin. Thus, it is not compatible with existing tools without advanced alterations. You might want to look at DRM and Common Encryption for that.

The current implementation supports 128-bit AES encryption in ECB, CBC, and CTR modes. Although the IV (Initialization Vector) and key are currently static, there are plans to enhance security by introducing dynamic IVs in future iterations. The AES implementation used can be found [here.](https://github.com/kokke/tiny-AES-c/tree/master "Tiny AES C")

#### Note: Use it at your own risk!

## TODO
- Replace the static IV property with a callback that returns a unique IV for each initialization of the AES context.
    - Implemented IV signal and seed, need to remove previous IV code.
- Decryptor does not use IV, remove it from its properties.
- Refactor file names and clean the project structure
- Add CMake for ease

## Issues
- As stated in the TODO list, same IV is used for all frames and each NAL units. This is not cryptographically secure.

## Example Pipelines:
Note that decryptor iv has to be present but not used.

- Encrypt and decrypt in counter mode:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    h264decrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    nvh264dec ! glimagesink
```
- Encrypt and decrypt in cyber block chaining mode:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    h264decrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    nvh264dec ! glimagesink
```
- Encrypt, change stream format, change back to byte-stream, decrypt:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    h264parse ! video/x-h264,stream-format=avc3 ! h264parse ! video/x-h264,stream-format=byte-stream ! \
    h264decrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    nvh264dec ! glimagesink
```
- You can also stack encryptors. However, then you need to decrypt in the reverse order:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv=01234567012345670123456701234567 key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA encryption-mode=aes-cbc ! \
    h264encrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    h264parse ! video/x-h264,stream-format=avc3 ! h264parse ! video/x-h264,stream-format=byte-stream ! \
    h264decrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    h264decrypt iv=01234567012345670123456701234567 key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA encryption-mode=aes-cbc ! \
    nvh264dec ! glimagesink
```
- If you are using CTR mode then the order does not matter but you get error logs from padding as you should:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv=01234567012345670123456701234567 key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA encryption-mode=aes-ctr ! \
    h264encrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    h264encrypt iv=01234567012345670123456701234567 key=BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB encryption-mode=aes-ctr ! \
    h264parse ! video/x-h264,stream-format=avc3 ! h264parse ! video/x-h264,stream-format=byte-stream ! \
    h264decrypt iv=01234567012345670123456701234567 key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA encryption-mode=aes-ctr ! \
    h264decrypt iv=01234567012345670123456701234567 key=BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB encryption-mode=aes-ctr ! \
    h264decrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    nvh264dec ! glimagesink


...
0:00:01.527514691 257912 0x7d269c002060 ERROR            h264decrypt gsth264decrypt.c:222:_remove_padding: Padding is not removed! Invalid byte found: 155
```
## Development Pipelines:
You may use the following to ensure raw video and decrypted video match each other:


```
gst-launch-1.0 videotestsrc pattern=ball num-buffers=1000 ! nvh264enc ! filesink location=source.h264

gst-launch-1.0 filesrc location=source.h264 ! h264parse ! tee name=t1 \
    t1. ! queue ! multifilesink location=raw/%03d \
    t1. ! queue ! h264encrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! tee name=t2 \
    t2. ! queue ! multifilesink location=enc/%03d \
    t2. ! queue ! h264decrypt iv=01234567012345670123456701234567 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! multifilesink location=dec/%03d
```
Then compare outputs with:
```
diff <(hd raw/002) <(hd dec/002)
```
Compare all frames:
```
for i in {000..999}; do
    echo File $i;
    diff <(hd raw/$i) <(hd dec/$i);
done;
```
#### *Original README.md is below:*

# GStreamer template repository

This git module contains template code for possible GStreamer projects.

* gst-app :
  basic meson-based layout for writing a GStreamer-based application.

* gst-plugin :
  basic meson-based layout and basic filter code for writing a GStreamer plug-in.

## License

This code is provided under a MIT license [MIT], which basically means "do
with it as you wish, but don't blame us if it doesn't work". You can use
this code for any project as you wish, under any license as you wish. We
recommend the use of the LGPL [LGPL] license for applications and plugins,
given the minefield of patents the multimedia is nowadays. See our website
for details [Licensing].

## Usage

Configure and build all examples (application and plugins) as such:

    meson builddir
    ninja -C builddir

See <https://mesonbuild.com/Quick-guide.html> on how to install the Meson
build system and ninja.

Modify `gst-plugin/meson.build` to add or remove source files to build or
add additional dependencies or compiler flags or change the name of the
plugin file to be installed.

Modify `meson.build` to check for additional library dependencies
or other features needed by your plugin.

Once the plugin is built you can either install system-wide it with `sudo ninja
-C builddir install` (however, this will by default go into the `/usr/local`
prefix where it won't be picked up by a `GStreamer` installed from packages, so
you would need to set the `GST_PLUGIN_PATH` environment variable to include or
point to `/usr/local/lib/gstreamer-1.0/` for your plugin to be found by a
from-package `GStreamer`).

Alternatively, you will find your plugin binary in `builddir/gst-plugins/src/`
as `libgstplugin.so` or similar (the extension may vary), so you can also set
the `GST_PLUGIN_PATH` environment variable to the `builddir/gst-plugins/src/`
directory (best to specify an absolute path though).

You can also check if it has been built correctly with:

    gst-inspect-1.0 builddir/gst-plugins/src/libgstplugin.so

## Auto-generating your own plugin

You will find a helper script in `gst-plugins/tools/make_element` to generate
the source/header files for a new plugin.

To create sources for `myfilter` based on the `gsttransform` template run:

``` shell
cd src;
../tools/make_element myfilter gsttransform
```

This will create `gstmyfilter.c` and `gstmyfilter.h`. Open them in an editor and
start editing. There are several occurances of the string `template`, update
those with real values. The plugin will be called `myfilter` and it will have
one element called `myfilter` too. Also look for `FIXME:` markers that point you
to places where you need to edit the code.

You can then add your sources files to `gst-plugins/meson.build` and re-run
ninja to have your plugin built.


[MIT]: http://www.opensource.org/licenses/mit-license.php or COPYING.MIT
[LGPL]: http://www.opensource.org/licenses/lgpl-license.php or COPYING.LIB
[Licensing]: https://gstreamer.freedesktop.org/documentation/application-development/appendix/licensing.html
