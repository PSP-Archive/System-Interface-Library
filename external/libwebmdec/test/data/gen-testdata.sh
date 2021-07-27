#!/bin/sh

set -e

perl -e '
    srand(1);
    $a = (("\x20"x96 . "\x60"x96)x16 . ("\xA0"x96 . "\xE0"x96)x16);
    print $a x 5;
    print "\x60\0\xC0" x (2048*5);
' >test.raw

perl -e '
    use Math::Trig;
    use constant TWOPI => 3.1415926535897932 * 2;
    for ($i = 0; $i < 14700; $i++) {
        $left = int(10000 * sin(($i/100)*TWOPI));
        $right = int(6000 * sin(($i/50)*TWOPI));
        print pack("vv", $left, $right);
    }
' >test.pcm

perl -e '
    use Math::Trig;
    use constant TWOPI => 3.1415926535897932 * 2;
    for ($i = 0; $i < 14700; $i++) {
        $left = int(10000 * sin(($i/100)*TWOPI));
        print pack("v", $left);
    }
' >test-mono.pcm

# "-threads 1" to ensure deterministic behavior.
ffmpeg \
    -y -threads 1 \
    -vcodec rawvideo -f rawvideo -video_size 64x32 -pixel_format rgb24 \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 2 -i test.pcm \
    -force_key_frames 0:00:00.166 \
    -vcodec libvpx -acodec libvorbis stereo.webm

ffmpeg \
    -y -threads 1 \
    -vcodec rawvideo -f rawvideo -video_size 64x32 -pixel_format rgb24 \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 1 -i test-mono.pcm \
    -force_key_frames 0:00:00.166 \
    -vcodec libvpx -acodec libvorbis mono.webm

ffmpeg \
    -y -threads 1 \
    -vcodec rawvideo -f rawvideo -video_size 64x32 -pixel_format rgb24 \
        -framerate 30 -i test.raw \
    -force_key_frames 0:00:00.166 \
    -vcodec libvpx -acodec libvorbis no-audio.webm

rm -f test.raw test.pcm test.264 test-mono.pcm
