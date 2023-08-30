CC = gcc
CFLAGS = -Wall -I./libkirk

ifeq ($(DEBUG), 1)
CFLAGS+=-g -O0
else
CFLAGS+=-O2
endif

TARGET1 = libkirk/libkirk.a
OBJS1 = libkirk/kirk_engine.o libkirk/AES.o libkirk/DES.o libkirk/SHA1.o libkirk/amctrl.o libkirk/bn.o libkirk/ec.o

TARGET2 = psxtract
OBJS2 = psxtract.o cdrom.o crypto.o lz.o utils.o

all: $(TARGET1)
	@echo
	@echo
	@echo "WARNING: Linux version if woefully out of date, use the Windows code instead"

$(TARGET1): $(OBJS1)
	$(AR) rcs $@ $(OBJS1)

all: $(TARGET2)

$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) -L ./libkirk -lkirk


