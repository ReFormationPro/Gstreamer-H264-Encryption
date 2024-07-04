# GStreamer H264 Encryption Plugin
This is an experimental plugin designed to offer GStreamer elements for H.264 encryption/decryption while preserving the H.264 structure with nalu and slice headers intact.
The goal is to enable the playback of the encrypted stream even if properties such as stream-format or alignment are altered.
This capability allows secure video streaming over potentially insecure channels or storage in the MP4 format, with the added advantage of recoverability in the face of packet loss, common in UDP streams.

Note that this solution requires both encrypting and decrypting sides to be using this plugin. Thus, it is not compatible with existing tools without advanced alterations. You might want to look at DRM and Common Encryption for that.

The current implementation supports 128-bit AES encryption in ECB, CBC, and CTR modes. Although the IV (Initialization Vector) and key are currently static, there are plans to enhance security by introducing dynamic IVs in future iterations. The AES implementation used can be found [here.](https://github.com/kokke/tiny-AES-c/tree/master "Tiny AES C")

#### Note: Use it at your own risk!

## TODO
- Add CMake for ease
- ECB mode does not use IV. However, IV is generated and inserted into the stream regardless. Remove it.

## Issues
No known issues at the moment.

## Example Pipelines:
Note that decryptor iv has to be present but not used.

- Encrypt and decrypt in counter mode:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    nvh264dec ! glimagesink
```
- Encrypt and decrypt in cyber block chaining mode:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    nvh264dec ! glimagesink
```
- Encrypt, change stream format, change back to byte-stream, decrypt:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    h264parse ! video/x-h264,stream-format=avc3 ! h264parse ! video/x-h264,stream-format=byte-stream ! \
    h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-ctr ! \
    nvh264dec ! glimagesink
```
- You can also stack encryptors. However, then you need to decrypt in the reverse order:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv-seed=1869052520 key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA encryption-mode=aes-cbc ! \
    h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    h264parse ! video/x-h264,stream-format=avc3 ! h264parse ! video/x-h264,stream-format=byte-stream ! \
    h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-cbc ! \
    h264decrypt key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA encryption-mode=aes-cbc ! \
    nvh264dec ! glimagesink
```
- If you are using CTR mode and encrypting the stream multiple times and if you shuffle the decryption order, you can still get a playback for a few seconds. You will get error logs for padding as it will be incorrect, and finally your stream will error and end:
```
gst-launch-1.0 videotestsrc pattern=ball ! nvh264enc ! \
    h264encrypt iv-seed=1869052520 key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB encryption-mode=aes-ctr ! \
    h264encrypt iv-seed=1869052520 key=01234567012345670123456701234568 encryption-mode=aes-ctr ! \
    h264encrypt iv-seed=1869052520 key=BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBC encryption-mode=aes-ctr ! \
    h264parse ! video/x-h264,stream-format=avc3 ! h264parse ! video/x-h264,stream-format=byte-stream ! \
    h264decrypt key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB encryption-mode=aes-ctr ! \
    h264decrypt key=BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBC encryption-mode=aes-ctr ! \
    h264decrypt key=01234567012345670123456701234568 encryption-mode=aes-ctr ! \
    nvh264dec ! glimagesink
...
0:00:02.843930270 76389 0x7b2f1c002060 ERROR            h264decrypt gsth264decrypt.c:223:_remove_padding: Padding is not removed! Invalid byte found: 197
0:00:02.843933446 76389 0x7b2f1c002060 WARN             h264decrypt gsth264decrypt.c:310:gst_h264_decrypt_decrypt_slice_nalu:<h264decrypt2> Padding is not found, data is invalid.
0:00:02.843938626 76389 0x7b2f1c002060 WARN             h264decoder gsth264decoder.c:980:gst_h264_decoder_handle_frame_num_gap:<nvh264dec0> Invalid frame num 3, maybe frame drop
0:00:02.844209010 76389 0x7b2f1c002060 ERROR            h264decrypt gsth264decrypt.c:278:gst_h264_decrypt_decrypt_slice_nalu:<h264decrypt1> Encrypted block size (703) is not a multiple of AES_BLOCKLEN (16). Not attempting to decrypt.
0:00:02.844213989 76389 0x7b2f1c002060 ERROR            h264decrypt gsth264decrypt.c:202:gst_h264_decrypt_process_slice_nalu:<h264decrypt1> Failed to decrypt slice nal unit
0:00:02.844217636 76389 0x7b2f1c002060 ERROR     h264encryptionbase gsth264encryptionbase.c:313:gst_h264_encryption_base_transform:<h264decrypt1> Subclass failed to parse slice nalu
...
```
## Development Pipelines:
You may use the following to ensure raw video and decrypted video match each other:


```
gst-launch-1.0 videotestsrc pattern=ball num-buffers=1000 ! nvh264enc ! filesink location=source.h264

gst-launch-1.0 filesrc location=source.h264 ! h264parse ! tee name=t1 \
    t1. ! queue ! multifilesink location=raw/%03d \
    t1. ! queue ! h264encrypt iv-seed=1869052520 key=01234567012345670123456701234567 encryption-mode=aes-cbc ! tee name=t2 \
    t2. ! queue ! multifilesink location=enc/%03d \
    t2. ! queue ! h264decrypt key=01234567012345670123456701234567 encryption-mode=aes-cbc ! multifilesink location=dec/%03d
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
