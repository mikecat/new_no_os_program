#!/usr/bin/perl

use strict;
use warnings;

if (@ARGV != 2) {
	warn "Usage: exe2efi.pl input_exe output_efi\n";
	exit 1;
}

my $exeName = $ARGV[0];
my $efiName = $ARGV[1];

# read exe file
open(EXE, "< $exeName") or die("failed to open $exeName\n");
binmode(EXE);
my $exeData = "";
while (<EXE>) { $exeData .= $_; }
close(EXE);

# check parameters
my $exeDataLen = length($exeData);
if ($exeDataLen < 0x40) { die("exe too small (check 1)\n"); }
if (substr($exeData, 0, 2) ne "\x4D\x5A") { die("not exe file (check 1)\n"); }
my $newHeaderPos = unpack("L", substr($exeData, 0x3C, 4));
if ($exeDataLen < $newHeaderPos + 0x5E) { die("exe too small (check 2)\n"); }
if (substr($exeData, $newHeaderPos, 4) ne "\x50\x45\x00\x00") { die("not exe file (check 2)\n"); }
my $optHeaderSize = unpack("S", substr($exeData, $newHeaderPos + 0x14));
if ($optHeaderSize < 0x46) { die("opt header too small\n"); }
my $ibOffset = 0;
if (substr($exeData, $newHeaderPos + 0x18, 2) eq "\x0B\x01") { $ibOffset = 0x34; }
elsif (substr($exeData, $newHeaderPos + 0x18, 2) eq "\x0B\x02") { $ibOffset = 0x30; }
else { die("unsupported magic\n"); }

# set imagebase to 0x00400000
substr($exeData, $newHeaderPos + $ibOffset, 4) = "\x00\x00\x40\x00";

# set subsys to 0x000A
substr($exeData, $newHeaderPos + 0x5C, 2) = "\x0A\x00";

# write efi

open(EFI, "> $efiName") or die("failed to open $efiName\n");
binmode(EFI);
print EFI $exeData;
close(EFI);
