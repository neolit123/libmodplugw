CC = gcc
CFLAGS = -c -Wall -O1
LDFLAGS =  

AR = ar
ARFLAGS = rvs

INCLUDES = -I./include
INCLUDES += $(shell pkg-config --cflags libmodplug)
LIBS = $(shell pkg-config --libs libmodplug)

SRC = ./src/modplugw.c
OBJ = ./obj/modplugw.o
OBJ_DYN = ./obj/modplugw.dyn.o
LIBNAME = libmodplugw
LIBFILE = ./lib/$(LIBNAME).a

ifeq ($(OS),Windows_NT)
    DLLFILE = ./lib/$(LIBNAME).dll
else
    DLLFILE = ./lib/$(LIBNAME).so
    
endif
LIBFILE_DYN = $(DLLFILE).a

all: $(LIBFILE) $(LIBFILE_DYN)

$(LIBFILE): $(OBJ)
	$(AR) $(ARFLAGS) $(LIBFILE) $(OBJ)

$(LIBFILE_DYN): $(OBJ_DYN)
	$(CC) -shared $(OBJ_DYN) -o $(DLLFILE) $(LIBS) -Wl,--out-implib,$(LIBFILE_DYN)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) -o $(OBJ)

$(OBJ_DYN): $(SRC)
	$(CC) $(CFLAGS) -DMODPLUGW_DYNAMIC $(INCLUDES) $(SRC) -o $(OBJ_DYN)

clean:
	rm -f $(OBJ) $(OBJ_DYN) $(LIBFILE) $(LIBFILE_DYN) $(DLLFILE)

install:
	echo 'make install' not implemented!
