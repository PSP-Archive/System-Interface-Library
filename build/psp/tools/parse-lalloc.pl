#!/usr/bin/perl
#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/psp/tools/parse-lalloc.pl: Quick-and-dirty script to parse a Lua
# memory allocation trace log (as output by TRACE_ALLOCS in lalloc.c).

use strict;
use integer;
my %heaps = ();
my %heapalloc = ();
while (<>) {
    s/\r//;
    next if !/lalloc\.c:/;
#print "$.:$_";
    if (/Allocated new array at (0x[0-9A-F]+) for block size (\d+) \((\d+) slots, array size (\d+)\)/) {
	my ($ptr, $bsize, $slots, $asize) = ($1, $2, $3, $4);
	die "$ptr already allocated" if $heaps{$ptr};
	$heaps{$ptr} = [$bsize, hex($ptr)+24+$slots/8, hex($ptr)+$asize];
    } elsif (/Deleted array at (0x[0-9A-F]+)/) {
	my $ptr = $1;
	die "$ptr not allocated" if !$heaps{$ptr};
	delete $heaps{$ptr};
    } elsif (/malloc\((\d+)\) -> (0x[0-9A-F]+) \(block size (\d+), array (0x[0-9A-F]+)\)/) {
	my ($size, $ptr, $bsize, $heaptest) = ($1, $2, $3, $4);
	my $found = 0;
	foreach my $heap (keys %heaps) {
	    if (hex($ptr) >= $heaps{$heap}[1] && hex($ptr) < $heaps{$heap}[2]) {
		die "wrong heap! want=$heaptest got=$heap\n" if hex($heap) != hex($heaptest);
		die "block size mismatch (alloc $bsize, heap $heap $heaps{$heap}[0])" if $bsize != $heaps{$heap}[0];
		die "ptr $ptr not multiple of $bsize from $heaps{$heap}[1]" if (hex($ptr) - $heaps{$heap}[1]) % $bsize != 0;
		my $slot = (hex($ptr) - $heaps{$heap}[1]) / $bsize;
		die "ptr $ptr already allocated" if $heapalloc{$heap}[$slot];
		$heapalloc{$heap}[$slot] = 1;
		$found = 1;
		last;
	    }
	}
	die "no heap found for $ptr (heap $heaptest)" if !$found;
    } elsif (/free\((0x[0-9A-F]+)\) array (0x[0-9A-F]+)/) {
	my ($ptr, $heaptest) = ($1, $2);
	my $found = 0;
	foreach my $heap (keys %heaps) {
	    if (hex($ptr) >= $heaps{$heap}[1] && hex($ptr) < $heaps{$heap}[2]) {
		die "wrong heap! want=$heaptest got=$heap\n" if hex($heap) != hex($heaptest);
		my $bsize = $heaps{$heap}[0];
		die "ptr $ptr not multiple of $bsize from $heaps{$heap}[1]" if (hex($ptr) - $heaps{$heap}[1]) % $bsize != 0;
		my $slot = (hex($ptr) - $heaps{$heap}[1]) / $bsize;
		die "ptr $ptr not allocated" if !$heapalloc{$heap}[$slot];
		$heapalloc{$heap}[$slot] = 0;
		$found = 1;
		last;
	    }
	}
	die "no heap found for $ptr (heap $heaptest)" if !$found;
    } elsif (!/WARNING/ && !/nfree/ && !/passing to system/) {
	chomp;
	die $_;
    }
}
