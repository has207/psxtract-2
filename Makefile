# Cross-compilation Makefile for psxtract (Windows target from Linux)

CC = i686-w64-mingw32-gcc
CXX = i686-w64-mingw32-g++
WINDRES = i686-w64-mingw32-windres
TARGET = psxtract.exe

CXXFLAGS = -std=c++11 -O2 -Wall -D_CRT_SECURE_NO_WARNINGS
CFLAGS = -O2 -Wall
# -mwindows builds a GUI-subsystem app so no console window appears when the GUI
# is launched. main() still attaches to the parent console for command-line runs.
LDFLAGS = -static -static-libgcc -static-libstdc++ -mwindows
LIBS = -lkernel32 -luser32 -ladvapi32 -lmsacm32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lshlwapi

SRCDIR = src
OBJDIR = obj

# Embedded CUE data. generate_rc.py turns the cue/ directory into two generated
# sources (src/psxtract.rc + src/cue_lookup_table.autogen); both must stay in
# lockstep with the cue/ set or resource IDs desync from the lookup table.
CUE_DIR = cue
CUE_FILES = $(wildcard $(CUE_DIR)/*.cue)
GEN_RC = $(SRCDIR)/psxtract.rc
GEN_TABLE = $(SRCDIR)/cue_lookup_table.autogen

# Source files
CPP_SOURCES = $(SRCDIR)/psxtract.cpp $(SRCDIR)/crypto.cpp $(SRCDIR)/cdrom.cpp $(SRCDIR)/lz.cpp $(SRCDIR)/utils.cpp $(SRCDIR)/md5_verify.cpp $(SRCDIR)/at3acm.cpp $(SRCDIR)/gui.cpp $(SRCDIR)/cue_resources.cpp
C_SOURCES = $(SRCDIR)/libkirk/AES.c $(SRCDIR)/libkirk/amctrl.c $(SRCDIR)/libkirk/bn.c $(SRCDIR)/libkirk/DES.c $(SRCDIR)/libkirk/ec.c $(SRCDIR)/libkirk/kirk_engine.c $(SRCDIR)/libkirk/SHA1.c

# Object files
CPP_OBJECTS = $(CPP_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
C_OBJECTS = $(C_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RESOURCE_OBJECTS = $(OBJDIR)/psxtract_resources.o $(OBJDIR)/atrac3_resources.o
OBJECTS = $(CPP_OBJECTS) $(C_OBJECTS) $(RESOURCE_OBJECTS)

# Create obj directory structure
OBJDIRS = $(OBJDIR) $(OBJDIR)/libkirk

all: $(TARGET)

$(TARGET): $(OBJDIRS) $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build complete: $(TARGET)"


# Create directories
$(OBJDIRS):
	mkdir -p $@

# Compile C++ files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile C files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regenerate the embedded CUE resources whenever the cue/ set changes: content
# edits are caught via $(CUE_FILES), and additions/removals via the cue/ dir
# mtime. psxtract.rc is the canonical target; the same generate_rc.py invocation
# writes the lookup table beside it (run from src/ so paths resolve as ../cue).
$(GEN_RC): $(SRCDIR)/generate_rc.py $(CUE_DIR) $(CUE_FILES)
	cd $(SRCDIR) && python3 generate_rc.py ../$(CUE_DIR)

# Produced by the same recipe as $(GEN_RC); tie it to the rc so it is never
# generated twice and never ends up older than its consumers.
$(GEN_TABLE): $(GEN_RC)
	@touch $@

# Force a regeneration on demand (also covers pure deletions on odd filesystems).
regen:
	cd $(SRCDIR) && python3 generate_rc.py ../$(CUE_DIR)

# Compile resources
$(OBJDIR)/psxtract_resources.o: $(GEN_RC)
	$(WINDRES) $< -o $@

$(OBJDIR)/atrac3_resources.o: src/atrac3_resources.rc
	$(WINDRES) $< -o $@

# cue_resources.cpp #includes the generated lookup table, so it must be
# recompiled whenever the table is regenerated (the %.o: %.cpp rule supplies
# the recipe; this line only adds the missing header prerequisite).
$(OBJDIR)/cue_resources.o: $(GEN_TABLE)

clean:
	rm -rf $(OBJDIR) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/ 2>/dev/null || echo "Note: Could not install to /usr/local/bin (may need sudo)"

release: $(TARGET)
	@echo "Creating release package..."
	mkdir -p release/psxtract-2
	cp $(TARGET) release/psxtract-2/
	cp -r cue release/psxtract-2/ 2>/dev/null || true
	cp README.md release/psxtract-2/ 2>/dev/null || true
	cp LICENSE release/psxtract-2/ 2>/dev/null || true
	cd release && zip -r ../psxtract-2.zip psxtract-2 && cd ..
	rm -rf release
	@echo "Release package created: psxtract-2.zip"
	@echo "Contents:"
	@unzip -l psxtract-2.zip

.PHONY: all clean install release regen
