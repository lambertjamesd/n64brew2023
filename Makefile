#!smake
# --------------------------------------------------------------------
#        Copyright (C) 1998 Nintendo. (Originated by SGI)
#        
#        $RCSfile: Makefile,v $
#        $Revision: 1.1.1.1 $
#        $Date: 2002/05/02 03:27:21 $
# --------------------------------------------------------------------
include /usr/include/n64/make/PRdefs


SKELATOOL64:=tools/skeletool64
VTF2PNG:=vtf2png
SFZ2N64:=sfz2n64

OPTIMIZER		:= -O0
LCDEFS			:= -DDEBUG -g -Isrc/ -I/usr/include/n64/nustd -Werror -Wall
N64LIB			:= -lultra_rom -lnustd

ifeq ($(WITH_DEBUGGER),1)
LCDEFS += -DWITH_DEBUGGER
endif

BASE_TARGET_NAME = build/megatextures

LD_SCRIPT	= megatextures.ld
CP_LD_SCRIPT	= build/megatextures

SCENE_SCALE = 128

ASMFILES    =	$(shell find asm/ -type f -name '*.s')

ASMOBJECTS  =	$(patsubst %.s, build/%.o, $(ASMFILES))

CODEFILES = $(shell find src/ -type f -name '*.c')

ifeq ($(WITH_GFX_VALIDATOR),1)
LCDEFS += -DWITH_GFX_VALIDATOR
CODEFILES += gfxvalidator/validator.c gfxvalidator/error_printer.c gfxvalidator/command_printer.c
endif

CODESEGMENT =	build/codesegment

BOOT		=	/usr/lib/n64/PR/bootcode/boot.6102
BOOT_OBJ	=	build/boot.6102.o

OBJECTS		=	$(ASMOBJECTS) $(BOOT_OBJ)

DEPS = $(patsubst %.c, build/%.d, $(CODEFILES)) $(patsubst %.c, build/%.d, $(DATAFILES))

-include $(DEPS)

LCINCS =	-I/usr/include/n64/PR 
LCDEFS +=	-DF3DEX_GBI_2 -DSCENE_SCALE=${SCENE_SCALE}
#LCDEFS +=	-DF3DEX_GBI_2 -DFOG
#LCDEFS +=	-DF3DEX_GBI_2 -DFOG -DXBUS
#LCDEFS +=	-DF3DEX_GBI_2 -DFOG -DXBUS -DSTOP_AUDIO

LDIRT  =	$(BASE_TARGET_NAME).elf $(CP_LD_SCRIPT) $(BASE_TARGET_NAME).z64 $(BASE_TARGET_NAME)_no_debug.map $(ASMOBJECTS)

LDFLAGS =	-L/usr/lib/n64 $(N64LIB)  -L$(N64_LIBGCCDIR) -lgcc

default:	$(BASE_TARGET_NAME).z64

include $(COMMONRULES)

.s.o:
	$(AS) -Wa,-Iasm -o $@ $<

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MM $^ -MF "$(@:.o=.d)" -MT"$@"
	$(CC) $(CFLAGS) -c -o $@ $<

build/%.o: %.s
	@mkdir -p $(@D)
	$(AS) -Wa,-Iasm -o $@ $<

####################
## Materials
####################

build/assets/materials/static.h build/assets/materials/static_mat.c: assets/materials/static.skm.yaml
	@mkdir -p $(@D)
	$(SKELATOOL64) --name static -m $< --material-output -o build/assets/materials/static.h

####################
## Models
####################
#
# Source engine scale is 64x
#

MODEL_LIST = assets/models/chapel.blend

MODEL_HEADERS = $(MODEL_LIST:%.blend=build/%.h)
MODEL_OBJECTS = $(MODEL_LIST:%.blend=build/%_geo.o)

build/assets/models/%.h build/assets/models/%_geo.c build/assets/models/%_anim.c: build/assets/models/%.fbx assets/models/%.flags assets/materials/static.skm.yaml $(TEXTURE_IMAGES) $(SKELATOOL64)
	$(SKELATOOL64) --fixed-point-scale ${SCENE_SCALE} --model-scale 0.01 --name $(<:build/assets/models/%.fbx=%) $(shell cat $(<:build/assets/models/%.fbx=assets/models/%.flags)) -o $(<:%.fbx=%.h) $<

build/src/scene/scene.o: build/assets/models/chapel.h

####################
## Megatextures
####################

LUA_FILES = $(shell find tools/ -type f -name '*.lua')

WORLD_FILES = assets/world/test.blend

WORLD_HEADERS = $(WORLD_FILES:%.blend=build/%.h)
WORLD_OBJECTS = $(WORLD_FILES:%.blend=build/%_geo.o) $(WORLD_FILES:%.blend=build/%_img.o)

build/%.fbx: %.blend
	@mkdir -p $(@D)
	$(BLENDER_3_0) $< --background --python tools/export_fbx.py -- $@

build/assets/world/%.h build/assets/world/%_geo.c: build/assets/world/%.fbx build/assets/materials/static.h $(SKELATOOL64) $(TEXTURE_IMAGES) $(LUA_FILES)
	$(SKELATOOL64) --script tools/export_level.lua --fixed-point-scale ${SCENE_SCALE} --model-scale 0.01 --name $(<:build/assets/world/%.fbx=%) -m assets/materials/static.skm.yaml -o $(<:%.fbx=%.h) $<

build/assets/world/%.o: build/assets/world/%.c build/assets/materials/static.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MM $^ -MF "$(@:.o=.d)" -MT"$@"
	$(CC) $(CFLAGS) -c -o $@ $<
	
build/assets/materials/%_mat.o: build/assets/materials/%_mat.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MM $^ -MF "$(@:.o=.d)" -MT"$@"
	$(CC) $(CFLAGS) -c -o $@ $<

build/assets/world/level_list.h: $(WORLD_HEADERS) tools/generate_level_list.js
	@mkdir -p $(@D)
	node tools/generate_level_list.js $@ $(WORLD_HEADERS)

build/levels.ld: $(WORLD_OBJECTS) tools/generate_level_ld.js
	@mkdir -p $(@D)
	node tools/generate_level_ld.js $@ 0x02000000 $(WORLD_OBJECTS)

build/src/levels/level_list.o: build/assets/world/level_list.h

####################
## Sounds
####################

SOUND_CLIPS = $(shell find assets/ -type f -name '*.ins') $(shell find assets/ -type f -name '*.wav')

build/assets/sound/sounds.sounds build/assets/sound/sounds.sounds.tbl: $(SOUND_CLIPS)
	@mkdir -p $(@D)
	$(SFZ2N64) -o $@ $^


build/asm/sound_data.o: build/assets/sound/sounds.sounds build/assets/sound/sounds.sounds.tbl

build/src/audio/clips.h: tools/generate_sound_ids.js $(SOUND_CLIPS)
	@mkdir -p $(@D)
	node tools/generate_sound_ids.js -o $@ -p SOUNDS_ $(SOUND_CLIPS)

build/src/audio/clips.o: build/src/audio/clips.h
build/src/decor/decor_object_list.o: build/src/audio/clips.h

####################
## Linking
####################

$(BOOT_OBJ): $(BOOT)
	$(OBJCOPY) -I binary -B mips -O elf32-bigmips $< $@

# without debugger

CODEOBJECTS = $(patsubst %.c, build/%.o, $(CODEFILES)) $(MODEL_OBJECTS) build/assets/materials/static_mat.o

CODEOBJECTS_NO_DEBUG = $(CODEOBJECTS)

DATA_OBJECTS = 

ifeq ($(WITH_DEBUGGER),1)
CODEOBJECTS_NO_DEBUG += build/debugger/debugger_stub.o build/debugger/serial.o 
endif

$(CODESEGMENT)_no_debug.o:	$(CODEOBJECTS_NO_DEBUG)
	$(LD) -o $(CODESEGMENT)_no_debug.o -r $(CODEOBJECTS_NO_DEBUG) $(LDFLAGS)


$(CP_LD_SCRIPT)_no_debug.ld: $(LD_SCRIPT) build/levels.ld
	cpp -P -Wno-trigraphs $(LCDEFS) -DCODE_SEGMENT=$(CODESEGMENT)_no_debug.o -o $@ $<

$(BASE_TARGET_NAME).z64: $(CODESEGMENT)_no_debug.o $(OBJECTS) $(DATA_OBJECTS) $(CP_LD_SCRIPT)_no_debug.ld
	$(LD) -L. -T $(CP_LD_SCRIPT)_no_debug.ld -Map $(BASE_TARGET_NAME)_no_debug.map -o $(BASE_TARGET_NAME).elf
	$(OBJCOPY) --pad-to=0x100000 --gap-fill=0xFF $(BASE_TARGET_NAME).elf $(BASE_TARGET_NAME).z64 -O binary
	makemask $(BASE_TARGET_NAME).z64

# with debugger
CODEOBJECTS_DEBUG = $(CODEOBJECTS) 

ifeq ($(WITH_DEBUGGER),1)
CODEOBJECTS_DEBUG += build/debugger/debugger.o build/debugger/serial.o 
endif

$(CODESEGMENT)_debug.o:	$(CODEOBJECTS_DEBUG)
	$(LD) -o $(CODESEGMENT)_debug.o -r $(CODEOBJECTS_DEBUG) $(LDFLAGS)

$(CP_LD_SCRIPT)_debug.ld: $(LD_SCRIPT) build/levels.ld
	cpp -P -Wno-trigraphs $(LCDEFS) -DCODE_SEGMENT=$(CODESEGMENT)_debug.o -o $@ $<

$(BASE_TARGET_NAME)_debug.z64: $(CODESEGMENT)_debug.o $(OBJECTS) $(DATA_OBJECTS) $(CP_LD_SCRIPT)_debug.ld
	$(LD) -L. -T $(CP_LD_SCRIPT)_debug.ld -Map $(BASE_TARGET_NAME)_debug.map -o $(BASE_TARGET_NAME)_debug.elf
	$(OBJCOPY) --pad-to=0x100000 --gap-fill=0xFF $(BASE_TARGET_NAME)_debug.elf $(BASE_TARGET_NAME)_debug.z64 -O binary
	makemask $(BASE_TARGET_NAME)_debug.z64

clean:
	rm -rf build

fix:
	wine tools/romfix64.exe build/portal.z64 
.SECONDARY:
