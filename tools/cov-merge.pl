#!/usr/bin/perl
#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# tools/cov-merge.pl: Merge a set of output files from gcov or a compatible
# coverage analysis tool and write the merged result to standard output.
# The result can be used as input to the cov-html.pl script, or it can be
# merged with other coverage output files in a subsequent invocation of
# this script.
#
# Usage:
#     cov-merge.pl [options] file1.gcov [file2.gcov...] >merged.txt
#
# Options:
#     --exclude=path/to/exclude
#         Exclude files whose paths match the given prefix from coverage
#         report generation.  This option may be given more than once;
#         files which match any prefix will be excluded.
#
#     --ignore-branch-regex='regex'
#         Ignore branches reported on lines which match the given regex.
#         Useful for excluding branches which will never be taken, such as
#         runtime assertions.
#
#     --include=path/to/include
#         Limit coverage report generation to files whose paths match the
#         given prefix.  This option may be given more than once; files
#         which match any prefix will be included in the coverage report.
#         If this option is not given, all files will be included.
#
#     --strip=/path/to/strip
#         Strip the given prefix from all file pathnames in the coverage
#         output.  Stripping is performed before matching against include
#         and exclude paths.
#

use strict;
use warnings;

###########################################################################
###########################################################################

my @exclude_paths = ();
my $ignore_branch_regex = "";
my @include_paths = ();
my $strip_path = "";
while (@ARGV && $ARGV[0] =~ /^-/) {
    if ($ARGV[0] =~ /^-+h/) {
        die "Usage: $0 [options] file1.gcov [file2.gcov...] >merged.txt\n"
          . "    --exclude=PATH: Exclude files beginning with the given path\n"
          . "        (may be given multiple times; default: exclude nothing)\n"
          . "    --ignore-branch-regex=REGEX: Ignore branches on matching lines\n"
          . "    --include=PATH: Include files beginning with the given path\n"
          . "        (may be given multiple times; default: include all files)\n"
          . "    --strip=PATH: Strip the given path from the beginning of\n"
          . "        all source file names in coverage output; ignore files\n"
          . "        which do not begin with that path (default: none)\n";
    } elsif ($ARGV[0] =~ s/^--exclude=//) {
        push @exclude_paths, $ARGV[0];
        shift @ARGV;
    } elsif ($ARGV[0] =~ s/^--ignore-branch-regex=//) {
        $ignore_branch_regex = $ARGV[0];
        shift @ARGV;
    } elsif ($ARGV[0] =~ s/^--include=//) {
        push @include_paths, $ARGV[0];
        shift @ARGV;
    } elsif ($ARGV[0] =~ s/^--strip=//) {
        $strip_path = $ARGV[0];
        shift @ARGV;
    } else {
        die "Unknown option: $ARGV[0]\n";
    }
}

my %sources = ();
FILELOOP: foreach my $file (@ARGV) {
    local *F;
    open F, "<$file" or die "$file: $!\n";
    my ($source, @lines);
    my $last_linenum;
    my $ignore_file = 1;
    while (my $line = <F>) {
        chomp $line;

        if ($. == 1 && $line eq "COV-MERGED") {
            while ($line = <F>) {
                chomp $line;
                my ($source, $linenum, $count, $branches, $taken, $text) =
                    split(/:/, $line, 6);
                $source =~ s/\\([0-9A-F][0-9A-F])/chr(hex($1))/eg;
                $count = undef if $count eq "";
                my $partial = 0;
                $partial = 1 if defined($count) && $count =~ s/\*$//;  # GCC 8+ indicator that the line contains unexecuted basic blocks.
                if (!defined($sources{$source})) {
                    $sources{$source} = [];
                }
                if (!defined($sources{$source}[$linenum])) {
                    $sources{$source}[$linenum] =
                        [$count, $partial, [$branches, $taken], $text];
                } else {
                    if ($text ne $sources{$source}[$linenum][3]) {
                        die "$file:$.: Text mismatch on $source:$linenum: old=[$sources{$source}[$linenum][3]] new=[$text]\n";
                    }
                    $sources{$source}[$linenum][0] += $count if defined($count);
                    $sources{$source}[$linenum][1] &= $partial;
                    $sources{$source}[$linenum][2][0] += $branches;
                    $sources{$source}[$linenum][2][1] += $taken;
                }
            }
            next FILELOOP;
        }

        if ($line =~ /^branch\s+\d+\s+taken\s+(\d+)/) {
            if (!$ignore_file) {
                my $count = $1;
                if (!$ignore_branch_regex
                 || $lines[$last_linenum][3] !~ /$ignore_branch_regex/) {
                    $lines[$last_linenum][2][0]++;
                    $lines[$last_linenum][2][1]++ if $count > 0;
                }
            }
            next;
        } elsif ($line =~ /^branch\s/) {
            if (!$ignore_file) {
                if (!$ignore_branch_regex
                 || $lines[$last_linenum][3] !~ /$ignore_branch_regex/) {
                    $lines[$last_linenum][2][0]++;
                }
            }
            next;
        } elsif ($line =~ /^(function|call)\s/
                 || $line =~ /^-+$/
                 || $line =~ /^\w+:$/) {
            next;
        }
        my ($count, $linenum, $text) = split(/:/, $line, 3);
        if ($linenum == 0) {
            my ($tag, $value) = split(/:/, $text, 2);
            if ($tag eq "Source") {
                &add_source($file, $source, \@lines) if @lines;
                $source = $value;
                $source =~ s:/+:/:g;
                1 while $source =~ s:(^|/)\./:$1:;
                1 while $source =~ s:(^|/)[^/]+/\.\./:$1:;
                @lines = ();
                $ignore_file = 0;
                # If $strip_path is empty, this will still strip a leading
                # slash from the pathname.  This is intentional, to avoid
                # trying to write to absolute paths when generating the HTML.
                $source =~ s:^\Q$strip_path\E/?:: or $ignore_file = 1;
                $ignore_file = 1 if @include_paths && !grep {$source =~ /^\Q$_\E/} @include_paths;
                $ignore_file = 1 if grep {$source =~ /^\Q$_\E/} @exclude_paths;
            }
        } else {
            next if $ignore_file;
            $count =~ s/^\s+//;
            my $partial = 0;
            $partial = 1 if $count =~ s/\*$//;
            $partial = 0 if $text =~ /$ignore_branch_regex/;
            if ($count eq "-") {
                $count = undef;
            } elsif ($count eq "#####") {
                $count = 0;
            } elsif ($count !~ /^\d+$/) {
                die "$file:$.: Invalid execution count: $count\n";
            }
            $lines[$linenum] = [$count, $partial, [0,0], $text];
            $last_linenum = $linenum;
        }
    }

    if (!defined($source)) {
        die "Source tag missing from $file\n";
    }

    &add_source($file, $source, \@lines) if @lines;
}

sub add_source {
    my ($file, $source, $lines_ref) = @_;
    if (!defined($sources{$source})) {
        $sources{$source} = [];
    }
    for (my $linenum = 1; $linenum < @$lines_ref; $linenum++) {
        if (!defined($sources{$source}[$linenum])) {
            $sources{$source}[$linenum] = $$lines_ref[$linenum];
        } else {
            if ($sources{$source}[$linenum][3] ne $$lines_ref[$linenum][3]) {
                die "$file: Text mismatch on $source line $linenum: old=[$sources{$source}[$linenum][3]] new=[$$lines_ref[$linenum][3]]\n";
            }
            if (!defined($sources{$source}[$linenum][0])) {
                $sources{$source}[$linenum][0] = $$lines_ref[$linenum][0];
            } elsif (defined($$lines_ref[$linenum][0])) {
                $sources{$source}[$linenum][0] += $$lines_ref[$linenum][0];
            }
            $sources{$source}[$linenum][1] &= $$lines_ref[$linenum][1];
            $sources{$source}[$linenum][2][0] += $$lines_ref[$linenum][2][0];
            $sources{$source}[$linenum][2][1] += $$lines_ref[$linenum][2][1];
        }
    }
}

print "COV-MERGED\n";
foreach my $source (sort(keys(%sources))) {
    my $source_escaped = $source;
    $source_escaped =~ s/([\\:])/sprintf("\\%02X",ord($1))/eg;
    for (my $linenum = 1; $linenum < @{$sources{$source}}; $linenum++) {
        next if !defined($sources{$source}[$linenum]);
        printf "%s:%d:%s%s:%d:%d:%s\n",
            $source_escaped, $linenum,
            defined($sources{$source}[$linenum][0])
                ? $sources{$source}[$linenum][0] : "",
            $sources{$source}[$linenum][1] ? "*" : "",
            $sources{$source}[$linenum][2][0],
            $sources{$source}[$linenum][2][1],
            $sources{$source}[$linenum][3];
    }
}

###########################################################################
###########################################################################
