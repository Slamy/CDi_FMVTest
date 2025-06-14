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
XCDEF  = $(OS68K)/defs
CLIB   = $(OS9CDI)/lib
CDEF   = $(OS9CDI)/defs
PATH   = $(OS9C);%PATH%
DOSBOX = C:/Software/DOSBox-0.74-3/DOSBox.exe
#COMPILER CONFIGURATION
CC      = xcc
CCFLAGS = -eas=$(OUTPUT) -tp=68K,sc -v=$(CDEF) -bc -r
ASFLAGS = -O=0 -S -R=$(OUTPUT) -T=$(OUTPUT) -TO=osk -TP=68kI
LD      = l68

#FILES TO COMPILE
FILES   = $(OUTPUT)/main.r $(OUTPUT)/cdio.r $(OUTPUT)/video.r $(OUTPUT)/input.r $(OUTPUT)/graphics.r $(OUTPUT)/mpeg.r

#LINKER CONFIGURATION
LDPARAM = -a -n=cdi_$(NAME) -o=$(BUILD)/$(NAME) $(CLIB)/cstart.r $(FILES) -l=$(CLIB)/cdi.l -l=$(CLIB)/cdisys.l -l=$(CLIB)/clib.l -l=$(CLIB)/cio.l -l=$(CLIB)/math.l -l=$(CLIB)/sys.l -l=$(CLIB)/usr.l -l=$(XCLIB)/os_csl.l

cd: link
	$(DOSBOX) master.bat -exit

all: link

rebuild: clean cd

link_app: $(FILES)
	$(LD) -z=link.txt -o=build\$(NAME).app -n=play
	fixmod -ua=80ff $(BUILD)/$(NAME).app

link_cd: $(FILES)
	$(LD) -z=link.txt -o=build\$(NAME)
	fixmod -uo=0.0 $(BUILD)/$(NAME)

$(OUTPUT)/cdio.r : $(SRC)/cdio.c
	$(CC) $(CCFLAGS) -O=2 $(SRC)/cdio.c

$(OUTPUT)/graphics.r : $(SRC)/graphics.c
	$(CC) $(CCFLAGS) -O=2 $(SRC)/graphics.c

$(OUTPUT)/input.r : $(SRC)/input.c
	$(CC) $(CCFLAGS) -O=2 $(SRC)/input.c

$(OUTPUT)/main.r : $(SRC)/main.c
	$(CC) $(CCFLAGS) -O=2 $(SRC)/main.c

$(OUTPUT)/mpeg.r : $(SRC)/mpeg.c
	$(CC) $(CCFLAGS) -O=2 $(SRC)/mpeg.c

$(OUTPUT)/video.r : $(SRC)/video.c
	$(CC) $(CCFLAGS) -O=2 $(SRC)/video.c


clean:
	-@erase $(OUTPUT)/cm*
	-@erase $(OUTPUT)/*.tmp
	-@erase $(OUTPUT)/*.r
	-@erase $(BUILD)/$(NAME)

purge:
	-@erase $(OUTPUT)/cm*
	-@erase $(OUTPUT)/*.tmp
