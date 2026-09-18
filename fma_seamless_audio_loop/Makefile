#Configuration, name, folders
NAME    = fmvtest
OUTPUT  = out
BUILD   = build
SRC     = src

#COMPILER AND LIBRARY LOCATIONS
CDISDK = D:
OS9C   = $(CDISDK)/DOS/BIN
OS68K  = $(CDISDK)/OS9/68000
OS9CDI = $(CDISDK)/OS9/CDI
XCLIB  = $(OS68K)/lib
CLIB   = $(OS9CDI)/lib
CDEF   = $(OS9CDI)/defs
PATH   = $(OS9C);%PATH%
DOSBOX = C:/Software/DOSBox-0.74-3/DOSBox.exe
#COMPILER CONFIGURATION
CC      = xcc
CCFLAGS = -eas=$(OUTPUT) -tp=68K,sc -v=$(CDEF) -bc -r
LD      = l68

#FILES TO COMPILE
FILES   = $(OUTPUT)/main.r $(OUTPUT)/video.r $(OUTPUT)/graphics.r $(OUTPUT)/mpeg.r

#BUILD TARGETS
cd: link_cd
	$(DOSBOX) master.bat -exit

all: link_app

rebuild: clean cd

link_app: $(FILES)
	$(LD) -z=link.txt -s=build\$(NAME).map -o=build\$(NAME).app -n=play
	fixmod -ua=80ff $(BUILD)/$(NAME).app

link_cd: $(FILES)
	$(LD) -z=link.txt -s=build\$(NAME).map -o=build\$(NAME)
	fixmod -uo=0.0 $(BUILD)/$(NAME)

$(OUTPUT)/graphics.r : $(SRC)/graphics.c $(SRC)/config.h $(SRC)/*.h
	$(CC) $(CCFLAGS) -O=2 $(SRC)/graphics.c

$(OUTPUT)/main.r : $(SRC)/main.c $(SRC)/*.h
	$(CC) $(CCFLAGS) -O=2 $(SRC)/main.c

$(OUTPUT)/mpeg.r : $(SRC)/mpeg.c $(SRC)/*.h $(SRC)/sfx_start.c $(SRC)/sfx_restart.c $(SRC)/sfx1.c $(SRC)/sfx2.c $(SRC)/song_sequence.inc
	$(CC) $(CCFLAGS) -O=2 $(SRC)/mpeg.c

$(OUTPUT)/video.r : $(SRC)/video.c $(SRC)/*.h
	$(CC) $(CCFLAGS) -O=2 $(SRC)/video.c


clean:
	-@erase $(OUTPUT)/cm*
	-@erase $(OUTPUT)/*.tmp
	-@erase $(OUTPUT)/*.r
	-@erase $(BUILD)/$(NAME)
	-@erase $(BUILD)/$(NAME).app
	-@erase $(BUILD)/$(NAME).map

purge:
	-@erase $(OUTPUT)/cm*
	-@erase $(OUTPUT)/*.tmp
