#/usr/bin/perl

use strict;
use warnings;

my $inputName = @ARGV > 0 ? $ARGV[0] : "font.bmp";

open(FONT, "< $inputName") or die("failed to open $inputName\n");
binmode(FONT);
my $fontData = "";
while (<FONT>) { $fontData .= $_; }
close(FONT);

if (length($fontData) < 0x22) { die("too small\n"); }
if (substr($fontData, 0, 2) ne "\x42\x4d") { die("not BMP file\n"); }
if (substr($fontData, 0x1A, 8) ne "\x01\x00\x18\x00\x00\x00\x00\x00") { die ("unsupported format\n"); }

my $offset = unpack("L", substr($fontData, 0x0A, 4));
my $width = unpack("L", substr($fontData, 0x12, 4));
my $height = unpack("L", substr($fontData, 0x16, 4));
my $reverse = 1;
if ($height < 0) { $reverse = 0; $height = -$height; }

my $lineSize = $width * 3 + (4 - (($width * 3) % 4)) % 4;
my $expectedSize = $offset + $lineSize * $height;
if (length($fontData) < $expectedSize) { die("too small\n"); }

if (($width - 1) % 16 != 0 || ($height - 1) % 16 != 0) { die("invalid size\n"); }
my $oneWidth = (($width - 1) >> 4) - 1;
my $oneHeight = (($height - 1) >> 4) - 1;
if ($oneWidth < 1 || $oneWidth > 8 || $oneHeight < 1) { die("invalid size\n"); }

print "#include \"font.h\"\n";
print "\n";
print "unsigned char font_data[256][$oneHeight] = {\n";

for (my $i = 0; $i < 16; $i++) {
	printf("\t/* 0x%02X */\n", $i * 16);
	for (my $j = 0; $j < 16; $j++) {
		print "\t{";
		for (my $k = 0; $k < $oneHeight; $k++) {
			if ($k > 0) { print ", "; }
			my $value = 0;
			for (my $l = 0; $l < $oneWidth; $l++) {
				my $y = ($oneHeight + 1) * $i + 1 + $k;
				my $x = ($oneWidth + 1) * $j + 1 + $l;
				my $pos = $offset + $lineSize * ($reverse ? $height - 1 - $y : $y) + 3 * $x;
				my @pixelData = unpack("C3", substr($fontData, $pos, 3));
				if ($pixelData[0] + $pixelData[1] + $pixelData[2] < 64 * 3) {
					$value |= 1 << ($oneWidth - 1 - $l);
				}
			}
			printf("0x%02X", $value);
		}
		print "}";
		if ($i < 15 || $j < 15) { print ","; }
		print "\n";
	}
}
print "};\n";
