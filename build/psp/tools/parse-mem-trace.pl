#!/usr/bin/perl
#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/psp/tools/parse-mem-trace.pl: Quick-and-dirty script to parse a memory
# allocation trace log (as output by TRACE_ALLOCS in src/{malloc,memory}.c)
# and print the amount of memory owned by each caller at the end of the log.

%owner = ();	# Owner of each memory block
%size = ();	# Allocated size of each memory block
%full = ();	# Actual space used for each memory block
%owned = ();	# Allocated bytes owned by each caller
%used = ();	# Actual space used by each caller

while (<>) {
	if (/\[(.*?)\] [mc]?alloc\((\d+)(?:,\d+)?\) -\> (0x[0-9A-F]+)/
	 || /\[(.*?)\] realloc\(0x0,(\d+)\) -\> (0x[0-9A-F]+)/) {
		my ($caller, $size, $ptr) = ($1, $2, $3);
		my $used = int(($size+(hex($ptr)&63||64)+63)/64) * 64;
		if (m@: src/malloc@) {
			$caller = "$caller [malloc]";
			$used = int(($size+8+7)/8) * 8;
		}
		$owner{$ptr} = $caller;
		$size{$ptr} = $size;
		$full{$ptr} = $used;
		$owned{$caller} += $size;
		$used{$caller} += $used;
	} elsif (/\[(.*?)\] realloc\((0x[0-9A-F]+),(\d+),\d+\) -\> (0x[0-9A-F]+)/) {
		my ($caller, $old, $size, $ptr) = ($1, $2, $3, $4);
		if (!$owner{$old}) {
			print STDERR "WARNING: $old has no owner, skipping\n";
			next;
		}
		$owned{$owner{$old}} -= $size{$old};
		$used{$owner{$old}} -= $full{$old};
		delete $owner{$old};
		delete $size{$old};
		delete $full{$old};
		my $used = int(($size+(hex($ptr)&63||64)+63)/64) * 64;
		if (m@: src/malloc@) {
			$caller = "$caller [malloc]";
			$used = int(($size+8+7)/8) * 8;
		}
		$owner{$ptr} = $caller;
		$size{$ptr} = $size;
		$full{$ptr} = $used;
		$owned{$caller} += $size;
		$used{$caller} += $used;
	} elsif (/\[(.*?)\] free\((0x[0-9A-F]+)\)/ || /\[(.*?)\] realloc\((0x[0-9A-F]+),\d+\) -\> free/) {
		my ($caller, $ptr) = ($1, $2);
		if (!$owner{$ptr}) {
			print STDERR "WARNING: $ptr has no owner, skipping\n";
			next;
		}
		$owned{$owner{$ptr}} -= $size{$ptr};
		$used{$owner{$ptr}} -= $full{$ptr};
		delete $owner{$ptr};
		delete $size{$ptr};
		delete $full{$ptr};
	} elsif (/\] ([mc]?alloc|realloc|free)/) {
		chomp;
		die "Unable to parse: $_";
	}
}

print "Alloc'ed  SizeUsed  Caller\n";
print "--------  --------  ------\n";
foreach (sort {$owned{$b} <=> $owned{$a}} keys %owned) {
	printf "%8d  %8d  %s\n", $owned{$_}, $used{$_}, $_;
}

print "\n";
print "Contiguous allocated/free regions\n";
print "---------------------------------\n";
my $base = undef;
my $top = undef;
foreach (sort {hex($a) <=> hex($b)} keys %full) {
	my $addr = hex($_) - (hex($_)&63 || 64);
	if (!defined($top) || $addr != $top) {
		if (defined($base) && defined($top)) {
			printf "0x%X - 0x%X   %9s allocated\n",
				$base, $top, sprintf("0x%X",$top-$base);
			printf "0x%X - 0x%X   %9s free\n",
				$top, $addr, sprintf("0x%X",$addr-$top);
		}
		$base = $addr;
	}
	$top = $addr + $full{$_};
}
if (defined($base) && defined($top)) {
	printf "0x%X - 0x%X   %9s allocated\n",
		$base, $top, sprintf("0x%X",$top-$base);
}
