#!/usr/bin/perl

use strict;
use warnings;

#!/usr/bin/perl

use strict;
use warnings;

if (@ARGV != 2) {
	warn "Usage: exe_tox64.pl input_exe output_exe\n";
	exit 1;
}

my $inName = $ARGV[0];
my $outName = $ARGV[1];

# read exe file
open(EXE, "< $inName") or die("failed to open $inName\n");
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
if ($optHeaderSize < 0x58) { die("opt header too small\n"); }
if (substr($exeData, $newHeaderPos + 0x18, 2) ne "\x0B\x01") { die("unsupported magic\n"); }

# extract header
my $headerSize = unpack("L", substr($exeData, $newHeaderPos + 0x54));
my $exeHeader = substr($exeData, 0, $headerSize);
my $exeBody = substr($exeData, $headerSize);

# modify File.Machine
substr($exeHeader, $newHeaderPos + 0x4, 2) = "\x64\x86";
# modify File.OptHeaderSize
substr($exeHeader, $newHeaderPos + 0x14, 2) = pack("S", $optHeaderSize + 0x10);
# modify File.Flags (clear IMAGE_FILE_32BIT_MACHINE flag)
my $flags = unpack("S", substr($exeHeader, $newHeaderPos + 0x16, 2));
substr($exeHeader, $newHeaderPos + 0x16, 2) = pack("S", $flags & ~0x100);
# modify Opt.Magic
substr($exeHeader, $newHeaderPos + 0x18, 2) = "\x0B\x02";
# modify Opt.ImageBase
substr($exeHeader, $newHeaderPos + 0x30, 8) = substr($exeHeader, $newHeaderPos + 0x34, 4) . "\x00\x00\x00\x00";
# modify Opt\.(Stack|Heap)Rsv(Size|Commit)
my $sizeData = substr($exeHeader, $newHeaderPos + 0x60, 16);
for (my $i = 4; $i >= 1; $i--) {
	$sizeData = substr($sizeData, 0, $i * 4) . "\x00\x00\x00\x00" . substr($sizeData, $i * 4);
}
substr($exeHeader, $newHeaderPos + 0x60, 16) = $sizeData;

# merge new header with body
my $newExe = substr($exeHeader, 0, $headerSize) . $exeBody;

# write exe file
open(OUTEXE, "> $outName") or die("failed to open $outName\n");
binmode(OUTEXE);
print OUTEXE $newExe;
close(OUTEXE);
