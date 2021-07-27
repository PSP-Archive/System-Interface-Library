#!/usr/bin/perl
#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# src/sysdep/misc/gen-joystick-db-i.pl: Script to convert the joystick
# database into compilable source code.
#

use strict;
use warnings;

die "Usage: $0 joystick-db.txt >joystick-db.i\n"
    if @ARGV != 1 || $ARGV[0] =~ /^-/;

my @all_buttons = ();
my $thisdir = $0;
$thisdir =~ s|/[^/]*$||;
my $input_h = "$thisdir/../../../include/SIL/input.h";
open F, "<$input_h" or die "$input_h: $!\n";
while (<F>) {
    push @all_buttons, $1 if /^\s*(INPUT_JOYBUTTON_[A-Z]\w+)/;
}
close F;

while (<>) {
    s/^\s+//;
    s/\s+$//;
    next if /^(\#|$)/;
    s/^(\S+)(?=\s)// or die "$ARGV:$.: Invalid syntax\n";
    my $config = $1;
    my @names = ();
    my $name_wildcard;
    if (/^\s+\*$/) {
        $name_wildcard = 1;
    } else {
        while (s/^\s+("([^\"]|\\.)*")//) {
            push @names, $1;
        }
        die "$ARGV:$.: Invalid name syntax\n" if $_ ne "";
    }
    die "$ARGV:$.: Too many names\n" if @names > 2;

    my $os = undef;
    my %fields = ();
    my %buttons = ();
    my $have_vidpid = 0;
    foreach (split(/:/, $config)) {
        die "$ARGV:$.: Invalid syntax at: $_\n" if !/=/ || /=.*=/;
        my ($field, $value) = split(/=/);
        if ($field eq "os") {
            $value =~ /^([0-9A-Z_]+)$/
                or die "$ARGV:$.: Invalid value for $field\n";
            $os = $value;
        } elsif ($field eq "vidpid") {
            $value =~ /^([0-9A-Fa-f]{1,4}),([0-9A-Fa-f]{1,4})$/
                or die "$ARGV:$.: Invalid value for $field\n";
            $fields{"vendor_id"} = "0x$1";
            $fields{"product_id"} = "0x$2";
            $have_vidpid = 1;
        } elsif ($field eq "ver") {
            $value =~ /^([0-9A-Fa-f]{1,8}),([0-9A-Fa-f]{1,8})$/
                or die "$ARGV:$.: Invalid value for $field\n";
            $fields{"dev_version"} = "0x$1";
            $fields{"version_mask"} = "0x$2";
        } elsif ($field =~ /^([lr])stick/) {
            my $lr = $1;
            $value =~ /^x-(r?[xyz]),y-(r?[xyz])(,b-b(\d+))?$/
                or die "$ARGV:$.: Invalid value for $field\n";
            my ($x, $y, $button) = (uc($1), uc($2), $4);
            $fields{"${lr}stick_x"} = "JOYSTICK_VALUE_$x";
            $fields{"${lr}stick_y"} = "JOYSTICK_VALUE_$y";
            $buttons{"INPUT_JOYBUTTON_".uc($lr)."_STICK"} = $button
                if defined($button);
        } elsif ($field eq "dpad") {
            if ($value eq "native") {
                $fields{"dpad_type"} = "JOYSTICK_DPAD_NATIVE";
            } elsif ($value eq "xy") {
                $fields{"dpad_type"} = "JOYSTICK_DPAD_XY";
            } elsif ($value eq "hat") {
                $fields{"dpad_type"} = "JOYSTICK_DPAD_HAT";
            } else {
                my ($up, $down, $left, $right) = &parse_udlr($value)
                    or die "$ARGV:$.: Invalid value for $field\n";
                $fields{"dpad_type"} = "JOYSTICK_DPAD_BUTTONS";
                $fields{"dpad_up"} = "$up";
                $fields{"dpad_down"} = "$down";
                $fields{"dpad_left"} = "$left";
                $fields{"dpad_right"} = "$right";
            }
        } elsif ($field eq "face") {
            my ($up, $down, $left, $right) = &parse_udlr($value)
                or die "$ARGV:$.: Invalid value for $field\n";
            $buttons{"INPUT_JOYBUTTON_FACE_UP"} = $up;
            $buttons{"INPUT_JOYBUTTON_FACE_DOWN"} = $down;
            $buttons{"INPUT_JOYBUTTON_FACE_LEFT"} = $left;
            $buttons{"INPUT_JOYBUTTON_FACE_RIGHT"} = $right;
        } elsif ($field eq "shoulder") {
            $value =~ /^([lr])-b(\d+),([lr])-b(\d+)$/ && $1 ne $3
                or die "$ARGV:$.: Invalid value for $field\n";
            $buttons{"INPUT_JOYBUTTON_".uc($1)."1"} = $2;
            $buttons{"INPUT_JOYBUTTON_".uc($3)."1"} = $4;
        } elsif ($field eq "trigger") {
            $value =~ /^([lr])-(b\d+|r?[xyz]),([lr])-(b\d+|r?[xyz])$/ && $1 ne $3
                or die "$ARGV:$.: Invalid value for $field\n";
            my ($lr1, $value1, $lr2, $value2) = ($1, $2, $3, $4);
            if ($value1 =~ /^b(\d+)$/) {
                $buttons{"INPUT_JOYBUTTON_".uc($lr1)."2"} = $1;
            } else {
                $fields{"${lr1}2_value"} = "JOYSTICK_VALUE_" . uc($value1);
            }
            if ($value2 =~ /^b(\d+)$/) {
                $buttons{"INPUT_JOYBUTTON_".uc($lr2)."2"} = $1;
            } else {
                $fields{"${lr2}2_value"} = "JOYSTICK_VALUE_" . uc($value2);
            }
        } elsif ($field =~ /^(start|select|menu)$/) {
            $value =~ /^b(\d+)$/
                or die "$ARGV:$.: Invalid value for $field\n";
            my $name = uc($field eq "menu" ? "home" : $field);
            $buttons{"INPUT_JOYBUTTON_$name"} = $1;
        } elsif ($field eq "linux_rumble") {
            $value =~ /^(left|right)_strong$/
                or die "$ARGV:$.: Invalid value for $field\n";
            $fields{"linux_rumble"} = "JOYSTICK_LINUX_RUMBLE_" . uc($value);
        } else {
            die "$ARGV:$.: Invalid field name: $field\n";
        }
    }
    $fields{"ignore_vid_pid"} = "1" if !$have_vidpid;
    $fields{"button_map"} = "{" . join(", ", map {"[$_] = " . (defined($buttons{$_}) ? $buttons{$_} : -1)} @all_buttons) . "}";
    $fields{"dpad_up"} = "-1" if !defined($fields{"dpad_up"});
    $fields{"dpad_down"} = "-1" if !defined($fields{"dpad_down"});
    $fields{"dpad_left"} = "-1" if !defined($fields{"dpad_left"});
    $fields{"dpad_right"} = "-1" if !defined($fields{"dpad_right"});

    if (defined($os)) {
        print "#ifdef SIL_PLATFORM_$os\n";
    }
    print "{";
    if ($name_wildcard) {
        print ".ignore_name = 1";
    } else {
        print ".names = {" . join(", ", @names) . "}";
    }
    print join("", map {", .$_ = $fields{$_}"} sort(keys(%fields)));
    print "},\n";
    if (defined($os)) {
        print "#endif\n";
    }
}


# Parses an up/down/left/right button set (regardless of the order in which
# the buttons are specified) and returns a list (up,down,left,right) of
# button numbers, or the empty list if a parse error occurs.
sub parse_udlr
{
    my ($value) = @_;
    my @fields = split(/,/, $value);
    return () if @fields != 4;

    my @udlr = ();
    my %udlr_map = (u => 0, d => 1, l => 2, r => 3);
    foreach my $field (@fields) {
        $field =~ /^([udlr])-b(\d+)$/ or return ();
        my ($dir, $button) = ($1, $2);
        return () if defined($udlr[$udlr_map{$dir}]);
        $udlr[$udlr_map{$dir}] = $button;
    }
    return @udlr;
}
