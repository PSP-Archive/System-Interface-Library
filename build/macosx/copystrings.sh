#!/bin/sh
#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/macosx/copystrings.sh: Replacement for the "copystrings" tool in
# Xcode <= 4.2 (internalized as "builtin-copyStrings" in Xcode 4.3).
#

usage () {
    cat <<EOT >&2

Usage: copystrings.sh [OPTION]... [INPUT-FILE]...

Options:

   --inputencoding=ENCODING
        Specify encoding of input files (default UTF-8).

   --outputencoding=ENCODING
        Write output files in the given encoding (default binary).

   --outdir=PATH
        Write output files to the given directory (default is the
        current directory).

   --validate
        Validate the input file before writing.

EOT
    exit 1
}


set -e

INPUT_ENCODING="utf-8"
OUTPUT_ENCODING="binary"
OUTDIR="."
VALIDATE=""
FILES=""

while test -n "$1"; do
    case "$1" in
      --inputencoding)
        shift
        INPUT_ENCODING=$1
        shift
        ;;
      --inputencoding=*)
        INPUT_ENCODING=`echo "x$1" | cut -d= -f2-`
        shift
        ;;
      --outputencoding)
        shift
        OUTPUT_ENCODING=$1
        shift
        ;;
      --inputencoding=*)
        OUTPUT_ENCODING=`echo "x$1" | cut -d= -f2-`
        shift
        ;;
      --outdir)
        shift
        OUTDIR=$1
        shift
        ;;
      --outdir=*)
        OUTDIR=`echo "x$1" | cut -d= -f2-`
        shift
        ;;
      --validate)
        VALIDATE=1
        shift
        ;;
      -h|--help)
        usage
        ;;
      -*)
        echo >&2 "Unknown option: $1"
        usage
        ;;
      *)
        temp=`echo "x$1" | cut -c2- | sed -e 's/_/__/g; s/ /_s/g; s/	/_t/g'`
        FILES="$FILES $temp"
        shift
        ;;
    esac
done
if test -z "$FILES"; then
    usage
fi

for i in $FILES; do
    in=`echo "x$i" | cut -c2- | sed -e 's/_s/ /g; s/_t/	/g; s/__/_/g'`
    out=$OUTDIR/`basename "x/$in"`
    if test -n "$VALIDATE"; then
        plutil -lint -s "$in"
    fi
    if test "binary" = "$OUTPUT_ENCODING"; then
        iconv -f "$INPUT_ENCODING" -t utf-8 <"$in" >"$out"
        plutil -convert binary1 -s "$out"
    else
        iconv -f "$INPUT_ENCODING" -t "$OUTPUT_ENCODING" <"$in" >"$out"
    fi
done
