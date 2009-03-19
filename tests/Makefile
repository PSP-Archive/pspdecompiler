TARGET = test
OBJS = test.o test_asm.o

INCDIR = 
CFLAGS = -O2 -G0 -Wall -fno-builtin-printf -fno-builtin-log
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS = -mno-crt0 -nostartfiles

LIBDIR =

LIBS = -lpspuser 

# Use the kernel's small inbuilt libc
USE_KERNEL_LIBC = 1
# Use only kernel libraries
USE_KERNEL_LIBS = 1

BUILD_PRX=1

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
