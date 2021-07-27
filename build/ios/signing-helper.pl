#!/usr/bin/perl
#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/ios/signing-helper.pl: iOS helper tool for signing.  Searches for
# valid provisioning profiles and certificates, and either prints out the
# relevant names for use by the Makefile or displays an error message.
#
# Usage:
#     signing-helper.pl [--verbose] [--profile=name | --profile-file=path] \
#         bundle-id output.mobileprovision output.xcent
# On success, outputs the certificate name to pass to codesign, and
# generates output.mobileprovision and output.xcent with the provisioning
# profile and entitlements, respectively.
#

use strict;
use warnings;
use Mime::Base64;
use File::Temp;

###########################################################################

my $profile = undef;
my $profile_file = undef;
my $verbose = 0;
my $bundle_id = undef;
my $output_mobileprovision = undef;
my $output_xcent = undef;
while ($ARGV[0] =~ /^-/) {
    if ($ARGV[0] =~ /^-+h/) {
        die "Usage: $0 [--verbose] [--profile=name | --profile-file=path] \\\n"
          . "    bundle-id output.mobileprovision output.xcent\n";
    } elsif ($ARGV[0] =~ s/^--profile=//) {
        if (defined($profile_file)) {
            die "--profile and --profile-file are mutually exclusive\n";
        }
        $profile = $ARGV[0];
    } elsif ($ARGV[0] =~ s/^--profile=//) {
        if (defined($profile)) {
            die "--profile and --profile-file are mutually exclusive\n";
        }
        $profile_file = $ARGV[0];
    } elsif ($ARGV[0] eq "--verbose") {
        $verbose = 1;
    } else {
        die "Unknown option: $ARGV[0]\n";
    }
    shift @ARGV;
}
if (@ARGV != 3) {
    die "Wrong number of arguments (try $0 --help)\n";
}
($bundle_id, $output_mobileprovision, $output_xcent) = @ARGV;

my $default_keychain = `security default-keychain`;
die "'security default-keychain' failed\n" if $? != 0;
$default_keychain =~ s/^\s+//;
$default_keychain =~ s/\s+$//;
die "'security default-keychain' returned the empty string\n"
    if $default_keychain eq "";

my @profile_files;
if (defined($profile_file)) {
    if (! -f $profile_file) {
        die "$profile_file not found\n";
    }
    @profile_files = ($profile_file);
} else {
    my $profile_dir = "$ENV{HOME}/Library/MobileDevice/Provisioning Profiles";
    $profile_dir =~ s/'/'\\''/g;  # Probably unnecessary, but be paranoid.
    @profile_files = <'$profile_dir'/*.mobileprovision>;
    if (!@profile_files) {
        die "No profiles found in ${profile_dir}\n"
          . "Have you imported them into Xcode?\n";
    }
}
my @chosen_files = ();
my @chosen_names = ();
my @chosen_certs = ();
my @chosen_profiles = ();
my @chosen_plists = ();
foreach my $file (@profile_files) {
    local $/ = undef;
    local *F;
    if (!open(F, "<$file")) {
        print STDERR "Skipping $file: $!\n";
        next;
    }
    local $_ = <F>;
    close F;
    my $profile_data = $_;
    if (!m:<plist[^>]*>([\0-\377]*?)</plist>:) {
        print STDERR "Skipping $file: plist not found\n";
        next;
    }
    my $plist = &parse_plist_dict($1);
    if (!$plist) {
        print STDERR "Skipping $file: failed to parse plist\n";
        next;
    }
    if (!defined($plist->{Name})) {
        print STDERR "Skipping $file: Name key not found\n";
        next;
    }
    if (defined($profile) && $plist->{Name} !~ /\Q$profile\E/) {
        if ($verbose) {
            print STDERR "Skipping $file: Name \"$plist->{Name}\" does not match requested name \"$profile\"\n";
        }
        next;
    }
    if (!defined($plist->{Entitlements})) {
        print STDERR "Skipping $file: Entitlements key not found\n";
        next;
    }
    my $entitlement_id = $plist->{Entitlements}{'application-identifier'};
    if (!defined($entitlement_id)) {
        print STDERR "Skipping $file: application-identifier entitlement not found\n";
        next;
    }
    $entitlement_id =~ s/^[^.]+\.//
        or die "$file: Invalid application-identifier entitlement: $entitlement_id\n";
    if ($entitlement_id ne "*" && $entitlement_id ne $bundle_id) {
        if ($verbose) {
            print STDERR "Skipping $file: Entitlement ID \"$entitlement_id\" does not match requested bundle ID \"$bundle_id\"\n";
        }
        next;
    }
    if (!defined($plist->{DeveloperCertificates})) {
        print STDERR "Skipping $file: DeveloperCertificates key not found\n";
        next;
    }
    if ($plist->{DeveloperCertificates} !~ /^ARRAY/) {
        print STDERR "Skipping $file: DeveloperCertificates key not an array\n";
        next;
    }
    my $found_cert = undef;
    foreach my $cert (@{$plist->{DeveloperCertificates}}) {
        $cert =~ s/\s//g;
        $cert =~ s:^<data>([\0-\377]*)</data>$:$1:
           or die "$file: DeveloperCertificates entry is not <data>...</data>\n";
        $cert = MIME::Base64::decode($cert);
        my ($tempf, $temppath) = File::Temp::tempfile();
        print $tempf $cert;
        local *PIPE;
        open PIPE, "-|", "openssl", "x509", "-in", $temppath, "-inform", "der", "-subject", "-noout"
            or die "$file: failed to open pipe to openssl: $!\n";
        my $subject = <PIPE>;
        close PIPE or die "$file: 'openssl x509' failed\n";
        close $tempf;
        if ($subject !~ m:/CN=([^/]+):) {
            die "$file: bad certificate subject: $subject\n";
        }
        $subject = $1;
        $subject =~ s/'/'\\''/g;
        system "security find-certificate -c '$subject' $default_keychain &>/dev/null";
        if ($? == 0) {
            $found_cert = $subject;
            last;
        }
    }
    if (!defined($found_cert)) {
        print STDERR "Skipping $file: no matching certificate in default keychain\n";
        next;
    }
    push @chosen_files, $file;
    push @chosen_names, $plist->{Name};
    push @chosen_certs, $found_cert;
    push @chosen_profiles, $profile_data;
    push @chosen_plists, $plist;
}
if (!@chosen_files) {
    die "No valid provisioning profile found!\n";
} elsif (@chosen_files > 1) {
    print STDERR "Multiple valid provisioning profiles found:\n";
    for (my $i = 0; $i < @chosen_names; $i++) {
        print STDERR "    $chosen_names[$i] ($chosen_files[$i])\n";
    }
    exit 1;
} else {
    if ($verbose) {
        print STDERR "Using provisioning profile \"$chosen_names[0]\" ($chosen_files[0])\n";
    }
}

my %plist = %{$chosen_plists[0]};
my $ubiquity_keys = "";
if ($plist{Entitlements}{"com.apple.developer.ubiquity-container-identifiers"}) {
    $ubiquity_keys .= "\t<key>com.apple.developer.ubiquity-container-identifiers</key>\n";
    $ubiquity_keys .= "\t<array>\n";
    foreach my $container_id (@{$plist{Entitlements}{"com.apple.developer.ubiquity-container-identifiers"}}) {
        if ($container_id =~ /^([^.]+)\.\*$/) {
            $ubiquity_keys .= "\t\t<string>$1.${bundle_id}</string>\n";
        } elsif ($container_id =~ /^([^.]+)\.(.+)$/ && $2 eq $bundle_id) {
            $ubiquity_keys .= "\t\t<string>${container_id}</string>\n";
        }
    }
    $ubiquity_keys .= "\t</array>\n";
}
if ($plist{Entitlements}{"com.apple.developer.ubiquity-kvstore-identifier"}) {
    my $kvstore_id = $plist{Entitlements}{"com.apple.developer.ubiquity-kvstore-identifier"};
    if ($kvstore_id =~ /^([^.]+)\.\*$/) {
        $ubiquity_keys .= "\t<key>com.apple.developer.ubiquity-kvstore-identifier</key>\n";
        $ubiquity_keys .= "\t<string>$1.${bundle_id}</string>\n";
    } elsif ($kvstore_id =~ /^([^.]+)\.(.+)$/ && $2 eq $bundle_id) {
        $ubiquity_keys .= "\t<string>${kvstore_id}</string>\n";
    }
}

open F, ">$output_xcent" or die "$output_xcent: $!\n";
print F <<EOT;
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>application-identifier</key>
	<string>$plist{ApplicationIdentifierPrefix}[0].${bundle_id}</string>
${ubiquity_keys}	<key>get-task-allow</key>
	$plist{Entitlements}{'get-task-allow'}
	<key>keychain-access-groups</key>
	<array>
		<string>$plist{ApplicationIdentifierPrefix}[0].${bundle_id}</string>
	</array>
</dict>
</plist>
EOT
close F;

open F, ">$output_mobileprovision" or die "$output_mobileprovision: $!\n";
print F $chosen_profiles[0];
close F;

print $chosen_certs[0];
exit 0;

#-------------------------------------------------------------------------#

# Parse a plist dictionary into a hash and return a reference to the hash,
# or undef on error.  Key values are handled as follows:
#     <array>...</array>       -> array ref
#     <dict>...</dict>         -> hash ref
#     <string>[^<]...</string> -> raw string
#     anything else            -> literal XML
sub parse_plist_dict
{
    local $_ = $_[0];
    if (!s:^\s*<dict>\s*([\0-\377]*?)\s*</dict>\s*$:$1:) {
        print STDERR "Failed to parse plist dict: not <dict>...</dict>\n";
        return undef;
    }
    my %dict = ();
    while (s:^<key>([^<]*)</key>\s*::) {
        my $key = $1;
        my $value;
        if (s:^<array>\s*::) {
            my @array = ();
            while (!s:^</array>\s*::) {
                my $element = &parse_plist_value(\$_);
                if (!defined($element)) {
                    return undef;
                }
                push @array, $element;
            }
            $value = \@array;
        } elsif (s:^(<dict>[\0-\377]*?</dict>)\s*::) {
            $value = &parse_plist_dict($1);
            if (!$value) {
                return undef;
            }
        } else {
            $value = &parse_plist_value(\$_);
            if (!defined($value)) {
                return undef;
            }
        }
        $dict{$key} = $value;
    }
    if ($_ ne "") {
        print STDERR "Failed to parse plist dict: leftover text: $_\n";
        return undef;
    }
    return \%dict;
}

sub parse_plist_value
{
    my ($strref) = @_;
    if ($$strref =~ s:^<string>([^<][\0-\377]*?)</string>\s*::) {
        return $1;
    } elsif ($$strref =~ s:^(<(string|date|data|integer|real)>[\0-\377]*?</\2>|<(true|false)/>)\s*::) {
        return $1;
    } else {
        print STDERR "Failed to parse plist dict: bad value format at: $$strref\n";
        return undef;
    }
}

###########################################################################
