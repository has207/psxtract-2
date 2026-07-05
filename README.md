psxtract-2
==========

Tool to decrypt and convert PSOne Classics from PSP/PS3.
Originally written by **Hykem**.

This tool allows you to decrypt a PSOne Classics EBOOT.PBP on your PC.
It features a modified version of libkirk's source code to support DES
encryption/decryption and the AMCTRL functions.
Also features isofix code for ensuring finalized ISO matches real discs.
And uses atrac3 ACM code for ATRAC3 decoding of CDDA audio tracks.


Notes
-------

The tool now supports a graphical interface which is the preferred way to run
it, though commandline is still supported. Batch extraction of multiple EBOOTS
is supported, you can select as many as you want and they will be extracted one
by one without further interaction, unless there's confusion about which cue
file a particular EBOOT requires, but that should be rare.

It also ships with cue files for the known PSX games sourced from redump.org,
processed and manually filtered, so errors are possible and some EBOOTs may not
get an automatically assigned cue file. You can file a bug here if that happens.

The GUI does not support providing custom KEYS.BIN or generating DOCUMENT.DAT
for manual decryption, use the commandline for that.

Native Linux code has diverged significantly and has been removed. In addition
ATRAC3 support essentially requires Windows so any idea of doing a full posix port
has been abandoned. However, the Windows build has been developed/built/run on
Linux using mingw and wine. As a result this tool should work fine on Linux or OSX,
simply use wine to run it.

For more details about the algorithms involved in the extraction process
please check the following sources:
- PBP unpacking: 
  https://github.com/pspdev/pspsdk/blob/master/tools/unpack-pbp.c

- PGD decryption:
  http://www.emunewz.net/forum/showthread.php?tid=3834 (initial research)
  https://code.google.com/p/jpcsp/source/browse/trunk/src/jpcsp/crypto/PGD.java (JPCSP)
  https://github.com/tpunix/kirk_engine/blob/master/npdrm/dnas.c (tpunix)

- AMCTRL functions:
  https://code.google.com/p/jpcsp/source/browse/trunk/src/jpcsp/crypto/AMCTRL.java (JPCSP)
  https://github.com/tpunix/kirk_engine/blob/master/kirk/amctrl.c (tpunix)
  
- CD-ROM ECC/EDC:
  https://github.com/DeadlySystem/isofix (Daniel Huguenin)


Working games and compatibility
-------------------------------

Nearly all PSN eboots should be supported. Some known problems are mentioned in the bug tracker:

  https://github.com/has207/psxtract-2/issues

If you encounter issues with a particular game report it there, but in general the game should
be fully playable even in those cases where we have md5 sum mismatches vs redump.org.

Generally speaking BIN/CUE pair created by this tool SHOULD match information on redump.org,
in other words all .BIN files should be the correct size and match md5 hashes
of a real disc dump.

However, if the game has CDDA audio audio tracks, i.e. 2 or more tracks per BIN, those audio
track md5 sums will NOT match redump.org info. This is working as intended, we generate the
audio tracks with the correct size and as close to original as possible. However, as we're dealing
with a lossy compression to ATRAC3, the conversion back to raw audio necessarily results in md5 hash
mismatches for the audio tracks.

The easiest way to verify hashes is by importing the game into Duckstation and checking hashes from
the game Properties menu.

Credits
-------

Daniel Huguenin (implementation of ECC/EDC CD-ROM patching) 

Draan, Proxima and everyone involved in kirk-engine (libkirk source code)

tpunix (C port and research of the PGD and AMCTRL algorithms)

PSPSDK (PBP unpacking sample code)

zecoxao (Unscrambling and decoding of audio tracks)

Heel (ATRACT3 decoding for CDDA tracks)
