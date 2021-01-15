#!/usr/bin/perl

use strict;
use warnings;

if (@ARGV != 2) {
	warn "Usage: exe2img.pl input_exe output_img\n";
	exit 1;
}

my $exeName = $ARGV[0];
my $imgName = $ARGV[1];

# read exe file
open(EXE, "< $exeName") or die("failed to open $exeName\n");
binmode(EXE);
my $exeData = "";
while (<EXE>) { $exeData .= $_; }
close(EXE);

# check parameters
my $exeDataLen = length($exeData);
if ($exeDataLen < 0x40) { die("exe too small (check 1)\n"); }
if (substr($exeData, 0, 2) ne "\x4D\x5A") { die ("not exe file (check 1)\n"); }
my $newHeaderPos = unpack("L", substr($exeData, 0x3C, 4));
if ($exeDataLen < $newHeaderPos + 0x5E) { die("exe too small (check 2)\n"); }
if (substr($exeData, $newHeaderPos, 4) ne "\x50\x45\x00\x00") { die ("not exe file (check 2)\n"); }
my $optHeaderSize = unpack("S", substr($exeData, $newHeaderPos + 0x14));
if ($optHeaderSize < 0x46) { die("opt header too small\n"); }

# set subsys to 0x000A
substr($exeData, $newHeaderPos + 0x5C, 2) = "\x0A\x00";

# build disk image

# first sector
my $diskImage =
	"\xEB\x3C\x90\x78\x78\x78\x78\x78\x78\x78\x78\x00\x02\x01\x01\x00" .
	"\x02\xE0\x00\x40\x0B\xF0\x09\x00\x12\x00\x02\x00\x00\x00\x00\x00" .
	"\x00\x00\x00\x00\x00\x00\x29\xDE\xAD\xBE\xEF\x4E\x4F\x20\x4E\x41" .
	"\x4D\x45\x20\x20\x20\x20\x46\x41\x54\x31\x32\x20\x20\x20\xB8\x01" .
	"\x13\xBB\x0F\x00\xB9\x19\x00\x31\xD2\x8E\xC2\xBD\x54\x7C\xCD\x10" .
	"\xFA\xF4\xEB\xFC\x42\x6F\x6F\x74\x65\x64\x20\x66\x72\x6F\x6D\x20" .
	"\x42\x49\x4F\x53\x2C\x20\x73\x74\x6F\x70\x2E\x0D\x0A";
$diskImage .= ("\x90" x (0x200 - 2 - length($diskImage))) . "\x55\xAA";

# FAT
my @fatData = (0xFF0, 0xFFF, 0xFFF, 0xFFF);

my $sectNum = ($exeDataLen + 511) >> 9;
for (my $i = 0; $i < $sectNum - 1; $i++) {
	push(@fatData, $i + 5);
}
push(@fatData, 0xFFF);
if (@fatData % 2 != 0) { push(@fatData, 0); }

my $fat = "";
for (my $i = 0; $i < @fatData; $i += 2) {
	$fat .= pack("CCC",
		$fatData[$i] & 0xFF,
		(($fatData[$i] >> 8) & 0xF) | (($fatData[$i + 1] & 0xF) << 4),
		($fatData[$i + 1] >> 4) & 0xFF);
}
if (length($fat) > 0x200 * 9) { die ("file too large\n"); }
$fat .= ("\x00" x (0x200 * 9 - length($fat)));

$diskImage .= $fat . $fat;

# directory info
my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime;
my $timeDate = pack("SS",
	(($hour & 0x1F) << 11) | (($min & 0x3F) << 5) | (($sec >> 1) & 0x1f),
	((($year - 80) & 0x7F) << 9) | ((($mon + 1) & 0xF) << 5) | ($mday & 0x1F));

my $rootDir = "EFI        \x10" . ("\x00" x 10) . $timeDate . "\x02\x00" . "\x00\x00\x00\x00";
$rootDir .= ("\x00" x (0x200 * 14 - length($rootDir)));

my $efiDir =
	".          \x10" . ("\x00" x 10) . $timeDate . "\x02\x00" . "\x00\x00\x00\x00" .
	"..         \x10" . ("\x00" x 10) . $timeDate . "\x00\x00" . "\x00\x00\x00\x00" .
	"BOOT       \x10" . ("\x00" x 10) . $timeDate . "\x03\x00" . "\x00\x00\x00\x00";
$efiDir .= ("\x00" x (0x200 - length($efiDir)));

my $bootDir = 
	".          \x10" . ("\x00" x 10) . $timeDate . "\x03\x00" . "\x00\x00\x00\x00" .
	"..         \x10" . ("\x00" x 10) . $timeDate . "\x02\x00" . "\x00\x00\x00\x00" .
	"BOOTIA32EFI\x00" . ("\x00" x 10) . $timeDate . "\x04\x00" . pack("L", $exeDataLen);
$bootDir .= ("\x00" x (0x200 - length($bootDir)));

$diskImage .= $rootDir . $efiDir . $bootDir;

# file and rest

$diskImage .= $exeData;
$diskImage .= ("\x00" x (0x168000 - length($diskImage)));

# write image

open(IMG, "> $imgName") or die("failed to open $imgName\n");
binmode(IMG);
print IMG $diskImage;
close(IMG);
