# GStreamer H264 Encryption Plugin
This is an experimental plugin designed to offer GStreamer elements for H.264 encryption/decryption while preserving the H.264 structure with nalu and slice headers intact.
The goal is to enable the playback of the encrypted stream even if properties such as stream-format or alignment are altered.
This capability allows secure video streaming over potentially insecure channels or storage in the MP4 format while not losing recoverability in the face of packet loss, which is common in UDP based streams (ie. RTP).

Note that this solution requires both encrypting and decrypting sides to be using this plugin. Thus, it is not compatible with existing tools without advanced alterations. You might want to look at DRM and Common Encryption for that.

The current implementation supports 128-bit AES encryption in ECB, CBC, and CTR modes. Although the IV ([Initialization Vector](https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Initialization_vector_(IV))) and key are currently static, there are plans to enhance security by introducing dynamic IVs in future iterations. 

This project utilizes [Tiny AES in C](https://github.com/kokke/tiny-AES-c/tree/master) library for its AES encryption implementation.

Minimum GStreamer version requirement is 1.23.1.

Developed on ubuntu:22.04 docker image and GStreamer 1.23.1.

> [!WARNING]
> Use it at your own risk!

## TODO
- Add CMake for ease
- ECB mode does not use IV. However, IV is generated and inserted into the stream regardless. Remove it.

## Example Pipelines:
- Encrypt and decrypt in counter (CTR) mode:
```shell
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    nvh264dec ! glimagesink
```
- Encrypt and decrypt in cyber block chaining (CBC) mode:
```shell
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    nvh264dec ! glimagesink
```
- Encrypt, change stream format, change back to byte-stream, decrypt:
```shell
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    h264parse ! video/x-h264,stream-format=avc3 ! h264parse ! video/x-h264,stream-format=byte-stream ! \
    h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    nvh264dec ! glimagesink
```
- You can also stack encryptors. However, then you need to decrypt in the **reverse** order:
```shell
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv-seed=1869052520 key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA encryption-mode=aes-cbc ! \
    h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    h264parse ! video/x-h264,stream-format=avc3 ! h264parse ! video/x-h264,stream-format=byte-stream ! \
    h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    h264decrypt key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA encryption-mode=aes-cbc ! \
    nvh264dec ! glimagesink
```

## Development Pipelines:
You may use the following to ensure raw video and decrypted video match each other:
```shell
gst-launch-1.0 videotestsrc pattern=ball num-buffers=1000 ! nvh264enc ! filesink location=source.h264

gst-launch-1.0 filesrc location=source.h264 ! h264parse ! tee name=t1 \
    t1. ! queue ! multifilesink location=raw/%03d \
    t1. ! queue ! h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! tee name=t2 \
    t2. ! queue ! multifilesink location=enc/%03d \
    t2. ! queue ! h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-cbc ! multifilesink location=dec/%03d
```
Then compare outputs with:
```shell
diff <(hd raw/002) <(hd dec/002)
```
Compare all frames:
```shell
for i in {000..999}; do
    echo File $i;
    diff <(hd raw/$i) <(hd dec/$i);
done;
```
