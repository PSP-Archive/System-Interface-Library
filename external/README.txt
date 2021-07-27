SIL external library repository
===============================

The source code stored under this directory is generally the unmodified
source code distribution for the library identified by the directory
name.  See below for details about each library's license and
distribution source, as well as any modifications that have been made
to the distribution as downloaded from the listed location.

The license descriptions below are intended to be informative only; the
license terms included with each library's source code are definitive.
Please notify the author of SIL if you find any discrepancy between the
descriptions in this file and a library's actual license terms.


freetype
--------
Version: 2.6.3
License: Dual-licensed, either FreeType license (BSD-like) or GNU GPL
         version 2; certain portions fall under the X Window System
         license (also known as the MIT license), which is compatible
         with either of the two licenses available for FreeType itself.
         See freetype/docs/LICENSE.TXT for details.
Distribution source: http://download.savannah.gnu.org/releases/freetype/

The "builds" and "docs/reference" directories of the distribution have
been removed to save space.

The following patches have been applied to the source code (see the file
00-freetype.diff for a cumulative diff against the unmodified source
code):
   - http://achurch.org/patch-pile/freetype/2.6.3/intptr-fix.diff


libnogg
-------
Version: 1.14
License: BSD (see libnogg/COPYING)
Distribution source: http://achurch.org/libnogg/

The "tests" directory of the distribution has been removed to save
space.  No changes have been made to the source code itself.


libpng
------
Version: 1.6.29
License: libpng license (see libpng/LICENSE)
Distribution source: http://www.libpng.org/pub/png/libpng.html

The "contrib" directory of the distribution has been removed to save
space.  No changes have been made to the source code itself.


libvpx
------
Version: 1.6.1
License: BSD (see libvpx/LICENSE), with an additional patent rights
         grant (see libvpx/PATENTS)
Distribution source: https://chromium.googlesource.com/webm/libvpx/

The "external", "test", and "third_party" directories of the distribution,
excluding "third_party/x86inc", have been removed to save space.

The following patches have been applied to the source code (see the file
00-libvpx.diff for a cumulative diff against the unmodified source
code):
   - http://achurch.org/patch-pile/libvpx/1.5.0/oom-fixes.diff


libwebmdec
----------
Version: 0.9
License: BSD (see libwebmdec/COPYING)
Distribution source: http://achurch.org/

No changes have been made to the source code distribution.


nestegg
-------
Version: Git commit febf18700cfa133003d787ca93a4d60251517aee
License: ISC/BSD (see nestegg/README)
Distribution source: https://github.com/kinetiknz/nestegg

The nestegg project does not have a versioned source distribution, so
the source code included here is the source code checked out from the
Git repository at the commit ID given above.

The "test" directory of the distribution has been removed to save space.
No changes have been made to the source code itself.


opengl-headers
--------------
Versions: 20160209 (GL/*), 20160108 (GLES2/*)
License: MIT (see opengl-headers/*/*.h)
Distribution sources:
   - http://www.opengl.org/registry/ (GL/*)
   - http://www.khronos.org/registry/gles/ (GLES2/*)

This directory contains the official OpenGL extension headers, which are
used to supplement (sometimes incomplete) headers in platform SDKs.

No changes have been made to the headers.


zlib
----
Version: 1.2.11
License: zlib license (see zlib/README)
Distribution source: http://zlib.net/

The "contrib" directory of the distribution has been removed to save
space.

The following patches have been applied to the source code (see the file
00-zlib.diff for a cumulative diff against the unmodified source code):
   - http://achurch.org/patch-pile/zlib/1.2.11/remove-bogus-64bit-funcs.diff
