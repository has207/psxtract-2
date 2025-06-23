# Cross-compilation Makefile for psxtract (Windows target from Linux)

CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
TARGET = psxtract.exe

CXXFLAGS = -std=c++11 -O2 -Wall -D_CRT_SECURE_NO_WARNINGS
CFLAGS = -O2 -Wall
LDFLAGS = -static-libgcc -static-libstdc++
LIBS = -lkernel32 -luser32

SRCDIR = src
OBJDIR = obj

# Source files
CPP_SOURCES = $(SRCDIR)/psxtract.cpp $(SRCDIR)/crypto.cpp $(SRCDIR)/cdrom.cpp $(SRCDIR)/lz.cpp $(SRCDIR)/utils.cpp
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
	@if [ ! -f at3tool.exe ]; then \
		echo "ERROR: at3tool.exe not found in current directory"; \
		echo "Please obtain at3tool.exe and place it in the project root"; \
		exit 1; \
	fi
	@if [ ! -f msvcr71.dll ]; then \
		echo "ERROR: msvcr71.dll not found in current directory"; \
		echo "Please obtain msvcr71.dll and place it in the project root"; \
		exit 1; \
	fi
	mkdir -p release/psxtract-2
	cp $(TARGET) release/psxtract-2/
	cp at3tool.exe release/psxtract-2/
	cp msvcr71.dll release/psxtract-2/
	cp README.md release/psxtract-2/ 2>/dev/null || true
	cp LICENSE release/psxtract-2/ 2>/dev/null || true
	cd release && zip -r ../psxtract-2.zip psxtract-2 && cd ..
	rm -rf release
	@echo "Release package created: psxtract-2.zip"
	@echo "Contents:"
	@unzip -l psxtract-2.zip

.PHONY: all clean install release