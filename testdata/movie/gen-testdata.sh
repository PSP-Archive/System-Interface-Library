#!/bin/sh

set -e

TOPDIR=`dirname "$0"`/../..

make -C "${TOPDIR}/tools" streamux

perl -e '
    srand(1);
    binmode STDOUT;
    # RGB 1F1F1F/5F5F5F/9F9F9F/DFDFDF -> YUV 2B8080/628080/998080/D08080
    $a = ((("\x2B"x32 . "\x62"x32) x 16) . (("\x99"x32 . "\xD0"x32) x 16))
       . ("\x80"x(32*16))
       . ("\x80"x(32*16));
    print $a x 5;
    # RGB 5F00BF -> YUV 3BC69C
    $b = "\x3B"x(64*32) . "\xC6"x(32*16) . "\x9C"x(32*16);
    print $b x 5;
    # YUV 806060/80A060/806080/80A080
    $c = "\x80"x(64*32)
       . (("\x60"x16 . "\xA0"x16) x 16)
       . ("\x60"x(32*8) . "\x80"x(32*8));
    print $c x 5;
    for ($i = 0; $i < (64*32+32*16*2)*5; $i++) {
        print pack("C*",int(rand(256)));
    }' \
    >test.raw

perl -e '
    use Math::Trig;
    use constant TWOPI => 3.1415926535897932 * 2;
    binmode STDOUT;
    for ($i = 0; $i < 29400; $i++) {
        $left = int(10000 * sin(($i/100)*TWOPI) + 0.5);
        $right = int(6000 * sin(($i/50)*TWOPI) + 0.5);
        print pack("vv", $left, $right);
    }' \
    >test.pcm

perl -e '
    use Math::Trig;
    use constant TWOPI => 3.1415926535897932 * 2;
    binmode STDOUT;
    for ($i = 0; $i < 29400; $i++) {
        $left = int(10000 * sin(($i/100)*TWOPI) + 0.5);
        print pack("v", $left);
    }' \
    >test-mono.pcm

perl -e '
    use Math::Trig;
    use constant TWOPI => 3.1415926535897932 * 2;
    binmode STDOUT;
    for ($i = 0; $i < 29400; $i++) {
        $left = int(10000 * sin(($i/100)*TWOPI) + 0.5);
        $right = int(6000 * sin(($i/50)*TWOPI) + 0.5);
	$center = int(8000 * sin(($i/75)*TWOPI) + 0.5);
	$lfe = int(2000 * sin(($i/800)*TWOPI) + 0.5);
        print pack("vvvvvv", $left, $right, $center, $lfe,
                   int($left/2 + 0.5), int($right/2 + 0.5));
    }' \
    >test-surround.pcm

perl -e '
    use Math::Trig;
    binmode STDOUT;
    for ($i = 0; $i < 29400; $i += 100) {
	print pack("v", 32767) x 50;
	print pack("v", -32768) x 50;
    }' \
    >test-overflow.pcm

# -threads 1 to ensure deterministic behavior.
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 2 -i test.pcm \
    -codec:v libvpx -crf 4 -codec:a libvorbis -y test.webm
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 1 -i test-mono.pcm \
    -codec:v libvpx -crf 4 -codec:a libvorbis -y test-mono.webm
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 6 -i test-surround.pcm \
    -codec:v libvpx -crf 4 -codec:a libvorbis -y test-surround.webm
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 1 -i test-overflow.pcm \
    -codec:v libvpx -crf 4 -codec:a libvorbis -y test-overflow.webm
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -codec:v libvpx -crf 4 -y test-nosound.webm
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -codec:v libvpx-vp9 -crf 4 -y test-vp9-nosound.webm

# -codec:a aac seems to insert 1024 samples of silence at the beginning of
# the audio stream, so cut that much from the input data to compensate.
tail -c+4097 test.pcm >test2.pcm
tail -c+2049 test-mono.pcm >test2-mono.pcm
tail -c+12289 test-surround.pcm >test2-surround.pcm
# -strict experimental currently (ffmpeg-1.2.5) needed for -codec:a aac.
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 2 -i test2.pcm \
    -strict experimental \
    -f mp4 \
        -codec:v libx264 -pix_fmt yuv420p -profile:v main -level 2.1 -crf 30 \
            -refs 1 -keyint_min 2 -bf 0 -8x8dct 0 -mixed-refs 0 -aud 1 \
            -r 30 -force_fps \
        -codec:a aac -b:a 128k -y test.mp4
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 1 -i test2-mono.pcm \
    -strict experimental \
    -f mp4 \
        -codec:v libx264 -pix_fmt yuv420p -profile:v main -level 2.1 -crf 30 \
            -refs 1 -keyint_min 2 -bf 0 -8x8dct 0 -mixed-refs 0 -aud 1 \
            -r 30 -force_fps \
        -codec:a aac -b:a 64k -y test-mono.mp4
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 6 -i test2-surround.pcm \
    -strict experimental \
    -f mp4 \
        -codec:v libx264 -pix_fmt yuv420p -profile:v main -level 2.1 -crf 30 \
            -refs 1 -keyint_min 2 -bf 0 -8x8dct 0 -mixed-refs 0 -aud 1 \
            -r 30 -force_fps \
        -codec:a aac -b:a 64k -y test-surround.mp4
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f mp4 \
        -codec:v libx264 -pix_fmt yuv420p -profile:v main -level 2.1 -crf 30 \
            -refs 1 -keyint_min 2 -bf 0 -8x8dct 0 -mixed-refs 0 -aud 1 \
            -r 30 -force_fps \
        -y test-nosound.mp4
ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 2 -i test2.pcm \
    -strict experimental \
    -f mp4 \
        -codec:v libx264 -pix_fmt yuv422p -profile:v high422 -level 2.1 \
            -crf 30 -refs 1 -keyint_min 2 -bf 0 -8x8dct 0 -mixed-refs 0 \
            -aud 1 -r 30 -force_fps \
        -codec:a aac -b:a 128k -y test-yuv422.mp4

ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f s16le -ar 44100 -ac 2 -i test.pcm \
    -strict experimental \
    -f avi \
        -codec:v libx264 -pix_fmt yuv420p -profile:v main -level 2.1 -crf 30 \
            -refs 1 -keyint_min 2 -bf 0 -8x8dct 0 -mixed-refs 0 -aud 1 \
            -r 30 -force_fps \
        -codec:a copy -y test-pcm.avi

ffmpeg \
    -threads 1 \
    -codec:v rawvideo -f rawvideo -video_size 64x32 -pixel_format yuv420p \
        -framerate 30 -i test.raw \
    -f rawvideo \
        -codec:v libx264 -pix_fmt yuv420p -profile:v main -level 2.1 -crf 30 \
            -refs 1 -keyint_min 2 -bf 0 -8x8dct 0 -mixed-refs 0 -aud 1 \
            -r 30 -force_fps \
        -y test.264
"${TOPDIR}/tools/streamux" test.264 test.pcm 30 >test.str

# Rewrite all the Matroska segment UIDs to 0x000...001 for consistent output.
for f in *.webm; do
    perl -e '
        open F, "+<$ARGV[0]" or die "$ARGV[0]: $!\n";
        binmode F;
        undef $/;
        my $data = <F>;
        my $uid_pos = undef;
        for (my $i = 0; $i < length($data) - 3; $i++) {
            if (substr($data,$i,3) eq "\x73\xA4\x90") {
                die "$ARGV[0]: Multiple SegmentUIDs\n" if defined($uid_pos);
                $uid_pos = $i+3;
            }
        }
        die "$ARGV[0]: SegmentUID not found\n" if !defined($uid_pos);
        seek F, $uid_pos, 0 or die "seek($ARGV[0]): $!\n";
        print F "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1";
        close F;' \
        "$f"
done

# Make a version of test.webm with a frame rate field of 0 for testing
# code paths that deal with unknown-framerate movies.
perl -pe 's/\x23\xE3\x83\x84\x01\xFC\xA0\x55/\x23\xE3\x83\x84\0\0\0\0/' \
    <test.webm >framerate-0.webm

rm -f test.raw test.pcm test.264 test-mono.pcm test2.pcm test2-mono.pcm
rm -f test-surround.pcm test2-surround.pcm test-overflow.pcm
