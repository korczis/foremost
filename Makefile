
RAW_CC = gcc
RAW_FLAGS = -Wall -O2
LINK_OPT = 
VERSION = 1.5.7
# Try to determine the host system
SYS := $(shell uname -s | tr -d "[0-9]" | tr -d "-" | tr "[A-Z]" "[a-z]")


# You can cross compile this program for Win32 using Linux and the 
# MinGW compiler. See the README for details. If you have already
# installed MinGW, put the location ($PREFIX) here:
CR_BASE = /usr/local/cross-tools/i386-mingw32msvc/bin

# You shouldn't need to change anything below this line
#---------------------------------------------------------------------

# This should be commented out when debugging is done
#RAW_FLAGS += -D__DEBUG -ggdb

NAME = foremost
MAN_PAGES = $(NAME).8.gz

RAW_FLAGS += -DVERSION=\"$(VERSION)\"

# Where we get installed
BIN = /usr/local/bin
MAN = /usr/share/man/man8
CONF= /usr/local/etc
# Setup for compiling and cross-compiling for Windows
# The CR_ prefix refers to cross compiling from OSX to Windows
CR_CC = $(CR_BASE)/gcc
CR_OPT = $(RAW_FLAGS) -D__WIN32
CR_LINK = -liberty
CR_STRIP = $(CR_BASE)/strip
CR_GOAL = $(NAME).exe
WINCC = $(RAW_CC) $(RAW_FLAGS) -D__WIN32

# Generic "how to compile C files"
CC = $(RAW_CC) $(RAW_FLAGS) -D__UNIX
.c.o:   
	$(CC) -c $<


# Definitions we'll need later (and that should rarely change)
HEADER_FILES = main.h ole.h extract.h
SRC =  main.c state.c helpers.c config.c cli.c engine.c dir.c extract.c api.c
OBJ =  main.o state.o helpers.o config.o cli.o engine.o dir.o extract.o api.o
DOCS = Makefile README CHANGES $(MAN_PAGES) foremost.conf
WINDOC = README.txt CHANGES.txt


#---------------------------------------------------------------------
# OPERATING SYSTEM DIRECTIVES
#---------------------------------------------------------------------

all: $(SYS) goals

goals: $(NAME)

linux: CC += -D__LINUX -DLARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
linux: goals

sunos: solaris
solaris: CC += -D__SOLARIS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
solaris: goals

darwin: CC += -D__MACOSX
darwin: goals

mac: CC += -D__MACOSX
mac: goals

netbsd:  unix
openbsd: unix
freebsd: unix
unix: goals

#Fore some reasons BSD variants get confused on how to build engine.o
#so lets make it real clear

engine.o:       engine.c
	$(CC) -c engine.c


# Common commands for compiling versions for Windows. 
# See cross and windows directives below.
win_general: LINK_OPT = $(CR_LINK)
win_general: GOAL = $(CR_GOAL)
win_general: goals
	$(STRIP) $(CR_GOAL)

# Cross compiling from Linux to Windows. See README for more info
cross: CC = $(CR_CC) $(CR_OPT)
cross: STRIP = $(CR_STRIP)
cross: win_general

# See the README for information on Windows compilation
windows: CC = $(WINCC)
windows: STRIP = strip
windows: win_general 

cygwin_nt.: unix
cygwin: unix


#---------------------------------------------------------------------
# COMPILE THE PROGRAMS
#   This section must be updated each time you add an algorithm
#---------------------------------------------------------------------

foremost: $(OBJ)
	$(CC) $(OBJ) -o $(NAME) $(LINK_OPT)


#---------------------------------------------------------------------
# INSTALLATION AND REMOVAL 
#---------------------------------------------------------------------

install: goals
	install -m 755 $(NAME) $(BIN)
	install -m 444 $(MAN_PAGES) $(MAN)
	install -m 444 foremost.conf $(CONF)
macinstall: BIN = /usr/local/bin/
macinstall: MAN = /usr/share/man/man1/
macinstall: CONF = /usr/local/etc/
macinstall: mac install


uninstall:
	rm -f -- $(BIN)/{$(RM_GOALS)}
	rm -f -- $(MAN)/{$(RM_DOCS)}

macuninstall: BIN = /usr/bin
macuninstall: MAN = /usr/share/man/man1
macuninstall: uninstall

#---------------------------------------------------------------------
# CLEAN UP
#---------------------------------------------------------------------

# This is used for debugging
preflight:
	grep -n RBF *.1 *.h *.c README CHANGES

nice:
	rm -f -- *~

clean: nice
	rm -f -- *.o
	rm -f -- $(CR_GOAL) $(NAME) $(WIN_DOC)
	rm -f -- $(TAR_FILE).gz $(DEST_DIR).zip $(DEST_DIR).zip.gpg

#-------------------------------------------------------------------------
# MAKING PACKAGES
#-------------------------------------------------------------------------

EXTRA_FILES = 
DEST_DIR = $(NAME)-$(VERSION)
TAR_FILE = $(DEST_DIR).tar
PKG_FILES = $(SRC) $(HEADER_FILES) $(DOCS) $(EXTRA_FILES)

# This packages me up to send to somebody else
package: clean
	rm -f $(TAR_FILE) $(TAR_FILE).gz
	mkdir $(DEST_DIR)
	cp $(PKG_FILES) $(DEST_DIR)
	tar cvf $(TAR_FILE) $(DEST_DIR)
	rm -rf $(DEST_DIR)
	gzip $(TAR_FILE)


# This Makefile is designed for Mac OSX to package the file. 
# To do this on a linux box, The big line below starting with "/usr/bin/tbl"
# should be replaced with:
#
#	man ./$(MD5GOAL).1 | col -bx > README.txt
#
# and the "flip -d" command should be replaced with dos2unix
#
# The flip command can be found at:
# http://ccrma-www.stanford.edu/~craig/utility/flip/#
win-doc:
	/usr/bin/tbl ./$(MD5GOAL).1 | /usr/bin/groff -S -Wall -mtty-char -mandoc -Tascii | /usr/bin/col > README.txt
	cp CHANGES CHANGES.txt
	flip -d $(WINDOC)

cross-pkg: clean cross win-doc
	rm -f $(DEST_DIR).zip
	zip $(DEST_DIR).zip $(CR_MD5GOAL) $(CR_SHA1GOAL) $(CR_SHA256GOAL) $(WINDOC)
	rm -f $(WINDOC)

world: package cross-pkg
