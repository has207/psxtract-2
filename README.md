psxtract
========

Tool to decrypt and convert PSOne Classics from PSP/PS3.
Originally written by **Hykem**.

This tool allows you to decrypt a PSOne Classics EBOOT.PBP on your PC.
It features a modified version of libkirk's source code to support DES
encryption/decryption and the AMCTRL functions.
Also features isofix code for ensuring finalized ISO matches real discs.
And uses at3tool for ATRAC3 decoding of CDDA audio tracks.


Notes
-------

Output of running psxtract.exe EBOOT.PBP is two files - CDROM.BIN and
CDROM.cue in the current directory. You should know what to do with those.

Using the "-c" option in the command line, psxtract will clean up any
temporary files it generates while it runs (of where there are many).
This flag is kept for backwards compatibility with old psxtract that used
it to force CDROM creation. Current version always creates a BIN/CUE pair
but we keep the flag for nostalgia.

at3tool.exe and its accompanying dll are required to be in the same
directory for ATRAC3 audio decoding of CDDA tracks.

You may supply a KEYS.BIN file to the tool, but this is not necessary.
Using the internal files' hashes, psxtract can calculate the key by itself.

Game file manual decryption is also supported (DOCUMENT.DAT).

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

All PSN eboots should be supported. If you encounter issues with a particular game
report it here.


Credits
-------

Daniel Huguenin (implementation of ECC/EDC CD-ROM patching) 

Draan, Proxima and everyone involved in kirk-engine (libkirk source code)

tpunix (C port and research of the PGD and AMCTRL algorithms)

PSPSDK (PBP unpacking sample code)

zeroxao (Unscrambling and decoding of audio tracks)

Heel (ATRACT3 decoding for CDDA tracks)
