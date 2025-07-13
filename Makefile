# Cross-compilation Makefile for psxtract (Windows target from Linux)

CC = i686-w64-mingw32-gcc
CXX = i686-w64-mingw32-g++
TARGET = psxtract.exe

CXXFLAGS = -std=c++11 -O2 -Wall -D_CRT_SECURE_NO_WARNINGS
CFLAGS = -O2 -Wall
LDFLAGS = -static-libgcc -static-libstdc++
LIBS = -lkernel32 -luser32 -ladvapi32 -lmsacm32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lole32

SRCDIR = src
OBJDIR = obj

# Source files
CPP_SOURCES = $(SRCDIR)/psxtract.cpp $(SRCDIR)/crypto.cpp $(SRCDIR)/cdrom.cpp $(SRCDIR)/lz.cpp $(SRCDIR)/utils.cpp $(SRCDIR)/md5_data.cpp $(SRCDIR)/at3acm.cpp $(SRCDIR)/gui.cpp
C_SOURCES = $(SRCDIR)/libkirk/AES.c $(SRCDIR)/libkirk/amctrl.c $(SRCDIR)/libkirk/bn.c $(SRCDIR)/libkirk/DES.c $(SRCDIR)/libkirk/ec.c $(SRCDIR)/libkirk/kirk_engine.c $(SRCDIR)/libkirk/SHA1.c

# Object files
CPP_OBJECTS = $(CPP_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
C_OBJECTS = $(C_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
OBJECTS = $(CPP_OBJECTS) $(C_OBJECTS)

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

.PHONY: all clean install release
