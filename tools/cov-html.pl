#!/usr/bin/perl
#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# tools/cov-html.pl: Read coverage analysis output and generate a set of
# HTML files (inspired by LCOV's genhtml output) showing code coverage for
# each source file.  If no input file is specified, the script reads from
# standard input.
#
# Usage:
#     cov-html.pl [options] [coverage.out]
#
# Options:
#     --outdir=/output/dir
#         Write output files under the given directory.  The default is to
#         write to the subdirectory "coverage" in the current directory.
#

use strict;
use warnings;

my $DEFAULT_UNREACHABLE_REGEX = '(//\s*NOTREACHED|^\s*UNREACHABLE;)$';

###########################################################################
###########################################################################

my $output_path = "coverage";
my $unreachable_regex = $DEFAULT_UNREACHABLE_REGEX;
while (@ARGV && $ARGV[0] =~ /^-/) {
    if ($ARGV[0] =~ /^-+h/) {
        die "Usage: $0 [options] files...\n"
          . "    --outdir=PATH: Set the top directory for output files\n"
          . "        (default: \"coverage\")\n"
          . "    --unreachable-regex=REGEX: Treat lines matching REGEX as unreachable\n"
          . "        (default: $DEFAULT_UNREACHABLE_REGEX)\n";
    } elsif ($ARGV[0] =~ s/^--outdir=//) {
        $output_path = $ARGV[0];
        shift @ARGV;
    } elsif ($ARGV[0] =~ s/^--unreachable-regex=//) {
        $unreachable_regex = $ARGV[0];
        shift @ARGV;
    } else {
        die "Unknown option: $ARGV[0]\n";
    }
}

my %sources;
my $infile;
local *F;
if (@ARGV) {
    $infile = $ARGV[0];
    open F, "<$infile" or die "$infile: $!\n";
} else {
    $infile = "standard input";
    open F, "<-" or die "standard input: $!\n";
}
if (<F> !~ /^COV-MERGED\s*$/) {
    die "$infile:$.: invalid header line\n";
}
while (my $line = <F>) {
    chomp $line;
    my ($source, $linenum, $count, $branches, $taken, $text) =
        split(/:/, $line, 6);
    if ($text =~ m|$unreachable_regex|) {
        $branches = $taken = 0;  # Don't show branch data on unreachable lines.
    }
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
        if ($text ne $sources{$source}[$linenum][2]) {
            die "$infile:$.: Text mismatch on $source:$linenum: old=[$sources{$source}[$linenum][2]] new=[$text]\n";
        }
        $sources{$source}[$linenum][0] += $count if defined($count);
        $sources{$source}[$linenum][1] = 0 if !$partial;
        $sources{$source}[$linenum][2][0] += $branches;
        $sources{$source}[$linenum][2][1] += $taken;
    }
}

my %dirs = ();
foreach my $source (keys(%sources)) {
    my @path = split('/', $source);
    my $dir_ref = \%dirs;
    for (my $i = 0; $i+1 < @path; $i++) {
        $dir_ref->{$path[$i]} = {} if !defined($dir_ref->{$path[$i]});
        $dir_ref->{$path[$i]}{$path[$i+1]} = undef
            if !exists($dir_ref->{$path[$i]}{$path[$i+1]});
        $dir_ref = $dir_ref->{$path[$i]};
    }
}

my %dir_coverage = ();
foreach my $source (sort(keys(%sources))) {
    my @lines = @{$sources{$source}};
    my %coverage = (lines_total => 0, lines_hit => 0,
                    lines_unreachable => 0, lines_wrongly_reached => 0,
                    branches_total => 0, branches_hit => 0);
    for (my $linenum = 1; $linenum < @lines; $linenum++) {
        my ($count, $partial, $branches, $text) = @{$lines[$linenum]};
        if (defined($count)) {
            if ($text =~ /$unreachable_regex/) {
                $coverage{lines_unreachable}++;
                if ($count > 0) {
                    $coverage{lines_wrongly_reached}++;
                }
            } else {
                $coverage{lines_total}++;
                if ($count > 0) {
                    $coverage{lines_hit}++;
                }
            }
        }
        $coverage{branches_total} += $branches->[0];
        $coverage{branches_hit} += $branches->[1];
    }
    &write_source_html($source, \@lines, \%coverage);
    $dir_coverage{$source} = \%coverage;
    while ($source) {
        $source =~ s:/?[^/]*$::;
        $dir_coverage{$source} = {lines_total => 0, lines_hit => 0,
                                  lines_unreachable => 0,
                                  lines_wrongly_reached => 0}
            if !defined($dir_coverage{$source});
        foreach my $key (keys(%coverage)) {
            $dir_coverage{$source}{$key} += $coverage{$key};
        }
    }
}

&write_index_html("", \%dirs, \%dir_coverage, "style.css");

&mkdir_p($output_path);
open F, ">$output_path/style.css" or die "$output_path/style.css: $!\n";
print F &style_css();
close F;

exit 0;

###########################################################################

sub write_source_html {
    my ($path, $lines, $coverage) = @_;

    my $dir = $path;
    $dir =~ s:/?[^/]*$::;
    &mkdir_p("$output_path/$dir");

    my $stylesheet = $dir;
    $stylesheet =~ s:[^/]+:..:g;
    $stylesheet .= "/style.css";
    my $lines_pct = &make_pct($coverage->{lines_hit},
                              $coverage->{lines_total});
    my $branches_pct = &make_pct($coverage->{branches_hit},
                                 $coverage->{branches_total});

    open F, ">$output_path/$path.html" or die "$output_path/$path.html: $!\n";
    print F <<EOT;
<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
  <head>
    <title>Code Coverage Report for $path</title>
    <meta http-equiv="Content-Style-Type" content="text/css" />
    <link rel="stylesheet" href="$stylesheet" />
  </head>
  <body>
    <h1>Code Coverage Report for $path</h1>
    <hr />
    <table>
      <tr>
        <th></th>
        <th>Hit</th>
        <th>Total</th>
        <th>Coverage</th>
      </tr>
      <tr>
        <th style="text-align: right">Lines:</th>
        <td style="text-align: right">$coverage->{lines_hit}</td>
        <td style="text-align: right">$coverage->{lines_total}</td>
        <td style="text-align: right">$lines_pct</td>
      </tr>
      <tr>
        <th style="text-align: right">Branches:</th>
        <td style="text-align: right">$coverage->{branches_hit}</td>
        <td style="text-align: right">$coverage->{branches_total}</td>
        <td style="text-align: right">$branches_pct</td>
      </tr>
    </table>
EOT
    if ($coverage->{lines_wrongly_reached}) {
        my ($lines, $were);
        if ($coverage->{lines_wrongly_reached} == 1) {
            $lines = "line";
            $were = "was";
        } else {
            $lines = "lines";
            $were = "were";
        }
        print F <<EOT;
        <p style="font-weight: bold"><span style="color: red">WARNING:</span>
            $coverage->{lines_wrongly_reached} $lines $were marked
            <span class="wrongly-reached">unreachable</span> but $were
            reached anyway!</p>
EOT
    }
    print F <<EOT;
    <hr />
EOT
    for (my $linenum = 1; $linenum < @$lines; $linenum++) {
        my ($count, $partial, $branches, $linetext) = @{$lines->[$linenum]};
        my $class;
        if (!defined($count)) {
            $class = "no-code";
        } elsif (!$count) {
            if ($linetext =~ /$unreachable_regex/) {
                $class = "no-code";
            } else {
                $class = "not-hit";
            }
        } else {
            if ($linetext =~ /$unreachable_regex/) {
                $class = "wrongly-reached";
            } else {
                $class = "was-hit";
            }
        }
        my $branch_span;
        if ($branches->[0]) {
            my $branch_class =
                ($branches->[1] < $branches->[0]) ? "branch-ng" : "branch-ok";
            $branch_span = sprintf("<span class=\"%s\">%3s/%-3s</span>",
                                   $branch_class, '(' . $branches->[1],
                                   $branches->[0] . ')');
        } else {
            $branch_span = "       ";
        }
        $linetext =~ s/&/&amp;/g;
        $linetext =~ s/</&lt;/g;
        $linetext =~ s/>/&gt;/g;
        printf F "    <div class=\"code-line %s\">%5d %s %s</div>\n",
            $class, $linenum, $branch_span, $linetext;
    }
    print F <<EOT;
    <hr />
  </body>
</html>
EOT
    close F;
}

###########################################################################

sub write_index_html {
    my ($path, $dir_ref, $coverage_ref, $stylesheet) = @_;
    $path .= "/" if $path;
    my $real_path = "$output_path/$path";
    &mkdir_p($real_path);
    open F, ">$real_path/index.html" or die "$path/index.html: $!\n";

    my $have_wrongly_reached = 0;
    foreach my $file (keys(%$dir_ref)) {
        my $file_path = $path . $file;
        if ($coverage_ref->{$file_path}{lines_wrongly_reached} > 0) {
            $have_wrongly_reached = 1;
            last;
        }
    }

    print F <<EOT;
<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
  <head>
    <title>Code Coverage Report for $path*</title>
    <meta http-equiv="Content-Style-Type" content="text/css" />
    <link rel="stylesheet" href="$stylesheet" />
  </head>
  <body>
    <h1>Code Coverage Report for $path*</h1>
    <hr />
    <table>
      <tr>
        <th>File/Directory</th>
        <th colspan="3">Lines</th>
        <th>Line Coverage</th>
EOT
    if ($have_wrongly_reached) {
        print F <<EOT;
        <th colspan="3" class="wrongly-reached">Unreachable<br>Lines Reached</th>
EOT
    }
    print F <<EOT;
        <th colspan="3">Branches</th>
        <th>Branch Coverage</th>
      </tr>
EOT

    my @sorted_files = sort {
        my ($c, $d) = ($a, $b);
        $c =~ s|\.[^./]*$||;
        $d =~ s|\.[^./]*$||;
        return $c cmp $d || $a cmp $b;
    } keys(%$dir_ref);
    foreach my $file (@sorted_files) {
        my $file_path = $path . $file;
        my $displayed_file_path = $file_path;
        my $link_target;
        if (defined($dir_ref->{$file})) {
            $displayed_file_path .= "/";
            $link_target = "$file/index.html";
        } else {
            $link_target = "$file.html";
        }
        my $lines_hit = $coverage_ref->{$file_path}{lines_hit};
        my $lines_total = $coverage_ref->{$file_path}{lines_total};
        my $lines_pct = &make_pct($lines_hit, $lines_total);
        my $lines_unreachable = $coverage_ref->{$file_path}{lines_unreachable};
        my $lines_wrongly_reached =
            $coverage_ref->{$file_path}{lines_wrongly_reached};
        my $branches_hit = $coverage_ref->{$file_path}{branches_hit};
        my $branches_total = $coverage_ref->{$file_path}{branches_total};
        my $branches_pct = &make_pct($branches_hit, $branches_total);
        print F <<EOT;
      <tr>
        <td><a href="$link_target">$displayed_file_path</a></td>
        <td class="merged-left">$lines_hit</td>
        <td class="merged-center">/</td>
        <td class="merged-right">$lines_total</td>
        <td>$lines_pct</td>
EOT
        if ($have_wrongly_reached) {
            my $class_extra = ($lines_wrongly_reached > 0)
                ? " wrongly-reached" : "";
            print F <<EOT;
        <td class="merged-left$class_extra">$lines_wrongly_reached</td>
        <td class="merged-center$class_extra">/</td>
        <td class="merged-right$class_extra">$lines_unreachable</td>
EOT
        }
        print F <<EOT;
        <td class="merged-left">$branches_hit</td>
        <td class="merged-center">/</td>
        <td class="merged-right">$branches_total</td>
        <td>$branches_pct</td>
      </tr>
EOT
    }

    print F <<EOT;
    </table>
    <hr />
  </body>
</html>
EOT
    close F;

    foreach my $dir (keys(%$dir_ref)) {
        if (defined($dir_ref->{$dir})) {
            &write_index_html($path . $dir, $dir_ref->{$dir}, $coverage_ref,
                              "../$stylesheet");
        }
    }
}

###########################################################################

sub make_pct {
    my ($numerator, $denominator) = @_;
    return "-----" if !$denominator;
    my $pct = int(1000 * $numerator / $denominator) / 10;
    # Don't return "0%" or "100%" unless the ratio is exactly that value.
    if ($pct == 0 && $numerator > 0) {
        $pct = 0.1;
    } elsif ($pct == 100 && $numerator < $denominator) {
        $pct = 99.9;
    }
    return sprintf("%.1f%%", $pct);
}

###########################################################################

sub mkdir_p {
    my ($dir) = @_;
    $dir =~ s:/+$::;
    return if -d $dir;
    my $parent = $dir;
    if ($parent =~ s:/[^/]*$:: && $parent ne "") {
        &mkdir_p($parent);
    }
    mkdir $dir, 0777 or die "mkdir($dir): $!";
}

###########################################################################

sub style_css {
    return <<'EOT';
body  {font-family: verdana, sans-serif; font-size: 9pt;}
h1    {font-size: 150%; margin-top: 0.5em; margin-bottom: 0.5em;
       text-align: center;}
table {border: 4px ridge gray; border-spacing: 0; border-collapse: collapse;}
th    {border: 2px ridge gray; padding: 2px 5px;}
td    {border: 2px ridge gray; padding: 2px 5px;}
td.merged-left   {border-right: none; padding-right: 2px; text-align: right;}
td.merged-center {border-left: none; padding-left: 2px;
                  border-right: none; padding-right: 2px; text-align: center;}
td.merged-right  {border-left: none; padding-left: 2px; text-align: left;}

div.code-line    {font-family: monospace; font-size: 83%; white-space: pre;}
.no-code         {color: #808080;}
.not-hit         {background: #FFCCCC;}
.was-hit         {background: #CCFFCC;}
.wrongly-reached {background: #FFCCFF;}

span.branch-ok   {color: #008000;}
span.branch-ng   {color: #CC0000; background: #FFCCCC;}
EOT
}

###########################################################################
###########################################################################
