# $Id: Makefile,v 1.3 2001/05/03 13:43:42 fma Exp $

#######################################
# Base dir of your m68k gcc toolchain #
#######################################

BASEDIR = $(NEODEV)
AS = as
LD = gcc
CC = gcc
OBJC = objcopy
BIN2O = bin2elf
GFXCC = gfxcc
FIXCNV = fixcnv

#######################################
# Path to libraries and include files #
#######################################

INCDIR = $(BASEDIR)/m68k/include
LIBDIR = $(BASEDIR)/m68k/lib
TMPDIR = $(BASEDIR)/tmp

###################################
# Output: {cart, cd} *lower case* #
###################################
OUTPUT = cart

############################
# Settings for cart output #
############################
ROMSIZE = 0x20000
PADBYTE = 0x00

##############################
# Object Files and Libraries #
##############################

OBJS = $(TMPDIR)/crt0_$(OUTPUT).o $(TMPDIR)/main.o
LIBS = -linput -lvideo -lc -lgcc -lprocess

#####################
# Compilation Flags #
#####################

ASFLAGS = -m68000 --register-prefix-optional
LDFLAGS = -Wl,-T$(BASEDIR)/src/system/neo$(OUTPUT).x
CCFLAGS = -m68000 -O3 -Wall -fomit-frame-pointer -ffast-math -fno-builtin -nostartfiles -nodefaultlibs -D__$(OUTPUT)__

ARFLAGS = cr

DEBUG = -g

##################
# FIX Definition #
##################
FIXFILES = $(BASEDIR)/src/shared/fix_font.bmp 

#############
# GFX Files #
#############

GRAPHICS = 	fix.pal gfx/onePlayerBg.bmp gfx/twoPlayerBg.bmp gfx/blocks_12by12.bmp \
		gfx/menuscreen.bmp gfx/arrow.bmp gfx/blocks_16by16.bmp 

####################
# GFX Object Files #
####################

GFXOBJ =	$(TMPDIR)/palettes.o  $(TMPDIR)/onePlayerBg.o $(TMPDIR)/twoPlayerBg.o \
		$(TMPDIR)/blocks_12by12.o $(TMPDIR)/blocks_16by16.o $(TMPDIR)/menuscreen.o $(TMPDIR)/arrow.o 

##############
# Make rules #
##############

ifeq ($(OUTPUT),cart)
dev_p1.rom : test.o
	$(OBJC) --gap-fill=$(PADBYTE) --pad-to=$(ROMSIZE) -R .data -O binary $< $@
else
test.prg : test.o
	$(OBJC) -O binary $< $@
endif

test.o : test.fix test.spr $(OBJS) $(GFXOBJ)
	$(LD) -L$(LIBDIR) $(CCFLAGS) $(LDFLAGS) $(OBJS) $(GFXOBJ) $(LIBS) -o $@

$(TMPDIR)/%.o: %.c
	$(CC) -I$(INCDIR) $(CCFLAGS) -c $< -o $@

$(TMPDIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -f $(TMPDIR)/*.*
	rm -f fix.pal
	rm -f test.o
	rm -f test.prg
	rm -f test.fix
	rm -f test.spr
	rm -f dev_p1.rom

############
# FIX Rule #
############

test.fix : $(FIXFILES)
	$(FIXCNV) $(FIXFILES) -o $@ -pal fix.pal

#############
# GFX Rules #
#############

test.spr : $(GRAPHICS)
	$(GFXCC) -black $(GRAPHICS) -o $@

$(TMPDIR)/palettes.o: test.pal
	$(BIN2O) $< palettes $@
	
$(TMPDIR)/blocks_12by12.o: gfx/blocks_12by12.map
	$(BIN2O) $< blocks12 $@	
	
$(TMPDIR)/blocks_16by16.o: gfx/blocks_16by16.map
	$(BIN2O) $< blocks16 $@	
	
$(TMPDIR)/menuscreen.o: gfx/menuscreen.map
	$(BIN2O) $< menuscreen $@	
	
$(TMPDIR)/arrow.o: gfx/arrow.map
	$(BIN2O) $< arrow $@	
	
$(TMPDIR)/onePlayerBg.o: gfx/onePlayerBg.map
	$(BIN2O) $< onePlayerBg $@	
	
$(TMPDIR)/twoPlayerBg.o: gfx/twoPlayerBg.map
	$(BIN2O) $< twoPlayerBg $@