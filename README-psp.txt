SIL: System Interface Library for games
=======================================
Platform notes for PSP


C runtime library
-----------------
SIL programs can be built against either Newlib, a full-fledged runtime
library, or psplibc, a minimal library included with the unofficial PSP
SDK.  psplibc offers a smaller memory footprint, but in order to do that
it omits many standard library features, notably including many stdio
functions.  (SIL includes replacements for a few of the missing
functions; see stdio.c and stdlib.c in the src/sysdep/psp directory.)
To enable the use of psplibc, set the build variable USE_PSPLIBC to 1.

The PSP has no hardware support for double-precision floating-point
values, so any operations using double-precision numbers will slow down
the program significantly.  One possibly unexpected place this arises is
the standard library function strtof(), which returns a single-precision
floating-point value but is implemented in Newlib as a wrapper around
the double-precision strtod() function.  In turn, the C++ stream-input
operator >> calls strtof() when reading into a "float" type variable, so
it will also run more slowly than might be expected.  It is possible to
replace Newlib's strtof() function with a single-precision-only version
included in SIL, by setting the SIL_STRTOF_CUSTOM and
SIL_STRTOF_OVERRIDE_LIBRARY build variables to 1.  However, with a
vanilla build of Newlib, this will cause a multiple-definition error at
link time; to resolve this, edit libc/stdlib/strtod.c in the Newlib
source to add __attribute__((weak)) to the definition of strtof().


Graphics rendering
------------------
The PSP does not natively support textures larger than 512x512 pixels.
SIL does not enforce this restriction when creating textures (since some
textures might be created for image storage rather than rendering, for
example), and the behavior of drawing a textured primitive using a
texture larger than 512 pixels wide or high is generally undefined.
However, in the specific case of a GRAPHICS_PRIMITIVE_QUADS primitive
with a texture no wider than 512 pixels, the rendering code will
subdivide the texture into virtual subtextures of 512 pixels high and
select the correct subtexture for each quad in the primitive, as long as
no quads cross a 512-pixel boundary in the vertical direction.  For
stored primitives, the proper texture must be applied when the primitive
is first drawn, and the primitive must always be drawn using a texture
of the same size.  This is notably useful for rendering bitmap fonts
with large character sets, such as Japanese, which cannot fit into a
512x512 texture.

The PSP also does not natively support textures with non-power-of-two
(NPOT) sizes.  SIL includes code to compensate for this using the
hardware's texture scale registers, and simple draw operations using
NPOT textures will usually work correctly.  However, attempting to draw
a repeating texture across an NPOT dimension or, more generally, use
texture coordinates outside the range [0,1] will give incorrect results.

The PSP supports user-created framebuffers, but limitations of the PSP
hardware impose a 16k-byte alignment restriction for depth buffer data,
so creating many small framebuffers with nonzero depth_bits can quickly
exhaust VRAM.  Always set depth_bits to zero for framebuffers which do
not need a depth component.  In a similar vein, setting the display
attribute "depth_bits" to 0 before calling graphics_set_display_mode()
will free up 272k of VRAM for framebuffer use.

The PSP does not support shaders, and any attempt to call shader-related
functions will fail.

When drawing ("blitting") images or solid rectangles directly to the
screen using a 2D parallel projection, SIL can make use of a high-speed
drawing mode on the PSP to significantly reduce draw time.  In order to
take advantage of this mode, the following must be true when the
primitive is drawn:
   - Rendering is being done directly to the screen (not to an offscreen
        framebuffer).
   - The viewport is set to the entire screen.
   - The projection matrix is set to a parallel projection.
   - The view and model matrices are set to the identity matrix.
   - The primitive consists of exactly one axis-aligned, textured or
        untextured quad.
   - The primitive does not use per-vertex colors.
If the quad is textured, the following must also be true:
   - Primitive coloring is disabled (the fixed color is set to
        (1,1,1,1)).
   - The current texture's pixel format is either RGBA8888 or
        PALETTE8_RGBA8888.
   - The texture has antialiasing disabled.
   - The current texture is of size 512x512 pixels or less.  (As with
        normal rendering, this is not checked by the code; the result of
        rendering with an oversize texture is undefined.)
   - All texture coordinates in the primitive are in the range [0,1].
   - The texture coordinates define an axis-aligned rectangle of the
        same size (in pixels) as the region on the screen to be drawn.
   - The texture coordinates are set so that the smallest U and V
        coordinates are at the left and top edges, respectively, of
        the region to be drawn.
   - No texture offsets are applied.


Log files
---------
Debug log files created with log_to_file() are written to the directory
in which the executable is located.


Memory management
-----------------
Unlike all other systems supported by SIL, the PSP does not include
virtual memory management; consequently, the program's address space is
equal to the amount of memory available to the program, around 24MB (or
52MB on 64MB devices which make high memory available to user programs;
see the note about USE_64MB below).  As a result, memory fragmentation
is a much greater issue on the PSP than on other platforms, and client
code must take care that allocation patterns do not leave holes in the
memory space.  In particular, the C++ STL should be avoided since it
often leads to significant memory fragmentation.

When using SIL's mem_alloc() interface, the MEM_ALLOC_TOP and
MEM_ALLOC_TEMP flags can help alleviate fragmentation by allocating
objects of different lifetimes in different parts of the address space.
For example, a program might allocate all objects whose lifetime is only
the current frame using MEM_ALLOC_TEMP; such objects will then not
interfere with allocations of longer-lived objects in the main memory
pool, avoiding holes in the address space when loading long-lived data
from storage.

Note that the mem_alloc() interface is designed for infrequent, larger
allocations, and it has moderate overhead in terms of both runtime and
memory usage.  For code which requires frequent small allocations (such
as temporary strings), consider using malloc() instead.  Also note that
the mem_alloc() interface on PSP is not thread-safe; if you need to
allocate memory from multiple threads, you must use appropriate
synchronization to ensure that multiple threads cannot enter the memory
allocation functions at the same time.

If you use C++ and you define static objects which need to dynamically
allocate memory in their constructors (this is a bad idea, don't do it),
set the build variable CXX_CONSTRUCTOR_HACK to 1 to avoid crashes.

For programs using the Lua scripting language, SIL includes an allocator
specifically optimized for typical Lua allocation patterns.  See the
header include/SIL/sysdep/psp/lalloc.h for details.

By default, SIL uses a temporary memory pool size of 512k (512*1024
bytes), leaves an additional 512k for system use such as thread stacks,
and uses the largest remaining contiguous block of memory as the main
memory pool.  These sizes can be adjusted with the MEMORY_POOL_SIZE and
MEMORY_POOL_TEMP_SIZE build variables; see build/psp/build.mk for
details.  In particular, setting MEMORY_POOL_SIZE to a positive value
will ensure that the given amount of memory is available to the program,
but it may cause the program to fail at startup on some custom firmware
configurations if the user has too many plugins loaded.

It is also necessary to declare the maximum stack size for the main
thread at build time.  The default stack size is 128k; this can be
changed by setting the build variable STACK_SIZE to the desired stack
size in bytes.

By default, the PSP's OS only makes 24MB available to user-mode
programs.  On PSP models which include 64MB RAM (all models except the
original PSP-1000), some custom firmwares allow user programs to request
access to "high" memory (beyond the original PSP's 32MB); this adds 28MB
to the amount of RAM available to the program.  SIL programs can be
built to request this additional RAM by setting the build variable
USE_64MB to 1.


Movie playback
--------------
Since software video decoding will generally run too slow on the PSP for
realtime playback, SIL supports a custom movie format which wraps MPEG-2
video and PCM audio.  The "streamux" tool in the tools directory creates
files in this format.

Hardware playback of MPEG-2 video does not support selection of whether
to linearly interpolate chroma data when upsampling (the smooth_chroma
parameter to movie_open()); playback always uses the hardware's default
behavior (nearest-point sampling).

If the audio stream for a movie will be played back at a fixed volume
without any special effects such as panning or audio filters and there
is no need to synchronize with other sounds, the movie's audio stream
can be sent directly to the audio hardware, bypassing the software mixer
and saving a small amount of CPU time.  Call the function
psp_movie_set_direct_audio() to enable or disable this behavior.


Sound playback
--------------
SIL supports MP3 audio streams on the PSP via the system software
interface, which uses the secondary Media Engine CPU for decoding.
Since Ogg Vorbis decoding is very slow on the PSP, MP3 format is
recommended when compressed audio is desired.  Note that even with MP3
data, decoding will fall behind if more than 20-30 streams are being
decoded at once, so consider using WAV format for sounds which may be
heavily layered.

As currently implemented, the MP3 decoder requires loops to be longer
than one MP3 frame (typically 1152 samples).  Attempting to set a
shorter loop will cause a warning to be emitted and the loop to be
ignored when the stream is played.  Note that loop information is not
read from MP3 files, so it must be set manually by the caller on each
Sound instance.


User data management
--------------------
SIL uses standard PSP save data files for user data, allowing icons and
titles to be stored with each file and letting the user manipulate the
save file's via the PSP system menu (XMB).  In order for this to work,
client code must call userdata_set_program_name() with a specially
formatted string: the first 9 characters of the string must be a
Sony-style application ID, such as "GAME12345", and the remainder of the
string is taken as the program name for use in save file pathnames.

SIL does _not_ currently provide an interface to the PSP's built-in save
UI; client code is responsible for creating an appropriate UI.

Statistics data (such as achievements) is stored in an ordinary
user-visible save file.  SIL currently does not provide a cross-platform
interface for setting ancilliary data such as the title or description
on that file, but this data can be set on the PSP using the function
psp_userdata_set_stats_file_info().

By default, load and save operations run at a priority higher than
ordinary program threads and lower than internal SIL I/O threads; this
avoids audio interruptions while the OS is processing the load or save
operation and also ensures that the operation is not blocked by a busy
program thread, but it may cause occasional frame drops depending on
CPU load.  If this is a problem, call the function
psp_userdata_set_low_priority_mode() to set the load/save operation
priority to the lowest possible, below program threads.  This will
generally give smoother performance, but client code must then take
care not to monopolize the CPU while a load/save operation is in
progress.  (On the PSP, thread priorities are absolute: lower-priority
threads will never run unless all higher-priority threads are blocked.)

If using userdata_override_file_path() to force specific pathnames for
save files, the override string must be of the form "GAME12345dir/file",
where "GAME12345" is the nine-character game ID in standard Sony format.


Known quirks
------------
- On at least the PSP-2000, the GE (graphics processor) does not appear
  to render correctly when the viewport is a single pixel in width or
  height.
