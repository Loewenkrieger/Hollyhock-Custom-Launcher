MAKEFLAGS = rR


SOURCEDIR = src
TARGET ?= hh3
ifeq ($(TARGET),hh2)
OS_VERSION = 2000
OS_FULL = 02.01.2000.0000
OS_GUARD = 2017.0512.1515
DEFINES = -DLAUNCHER_HH2=1
else ifeq ($(TARGET),hh3)
OS_VERSION = 7002
OS_FULL = 02.01.7002.0001
OS_GUARD = 2023.0209.1500
DEFINES = -DLAUNCHER_HH2=0
else
$(error TARGET must be hh2 or hh3)
endif
BUILDDIR = obj-$(TARGET)
OUTDIR = dist/$(TARGET)
DEPDIR = .deps-$(TARGET)
METADIR = meta

AS:=sh4a_nofpueb-elf-gcc
AS_FLAGS:=-gdwarf-5

SDK_DIR?=meta/hollyhock-3/sdk
SDK_PATH=$(if $(filter /%,$(SDK_DIR)),$(SDK_DIR),../../$(SDK_DIR))
ADDRESSES_FILE=meta/hollyhock-3/versions/v$(OS_FULL)/addresses.json

DEPFLAGS=-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
WARNINGS=-Wall -Wextra -Wno-prio-ctor-dtor -pedantic -Werror -pedantic-errors
INCLUDES=-I$(SDK_DIR)/include -I$(SOURCEDIR)
FUNCTION_FLAGS=-flto=auto -ffat-lto-objects -fno-builtin -ffunction-sections -fdata-sections -gdwarf-5 -Os
COMMON_FLAGS=$(FUNCTION_FLAGS) $(INCLUDES) $(WARNINGS) $(DEFINES)

CC:=sh4a_nofpueb-elf-gcc
CC_FLAGS=-std=c23 $(COMMON_FLAGS)

CXX:=sh4a_nofpueb-elf-g++
CXX_FLAGS=-std=c++23 $(COMMON_FLAGS)

LD:=sh4a_nofpueb-elf-g++
LD_FLAGS:=$(FUNCTION_FLAGS) -Wl,--gc-sections --specs=custom-start.specs
LIBS:=-L$(SDK_DIR) -lsdk

READELF:=sh4a_nofpueb-elf-readelf
OBJCOPY:=sh4a_nofpueb-elf-objcopy
STRIP:=sh4a_nofpueb-elf-strip

PYTHON?=python3

VERSIONS := $(OS_VERSION)
OVERRIDES := $(OUTDIR)/addresses.$(OS_GUARD).override
$(OVERRIDES): $(BUILDDIR)/addresses.$(OS_VERSION).prep

APP_ELFS := $(VERSIONS:%=$(OUTDIR)/run.%.elf)
APP_BINS := $(APP_ELFS:.elf=.bin)
APP_HH3S := $(APP_ELFS:.elf=.adjusted.hh3)

AS_SOURCES:=$(shell find $(SOURCEDIR) -name '*.S' -not -name '*.template.*')
CC_SOURCES:=$(shell find $(SOURCEDIR) -name '*.c' -not -name '*.template.*')
CXX_SOURCES:=$(shell find $(SOURCEDIR) -name '*.cpp' -not -name '*.template.*')
ifeq ($(TARGET),hh3)
CXX_SOURCES:=$(filter-out $(SOURCEDIR)/loaders/hh2/%,$(CXX_SOURCES))
endif
OBJECTS := $(addprefix $(BUILDDIR)/,$(sort $(AS_SOURCES:.S=.o))) \
	$(addprefix $(BUILDDIR)/,$(sort $(CC_SOURCES:.c=.o))) \
	$(addprefix $(BUILDDIR)/,$(sort $(CXX_SOURCES:.cpp=.o)))

NOLTOOBJS := $(foreach obj, $(OBJECTS), $(if $(findstring /nolto/, $(obj)), $(obj)))

DEPFILES := $(OBJECTS:$(BUILDDIR)/%.o=$(DEPDIR)/%.d)

bin: $(APP_BINS) Makefile
adjusted: $(if $(filter hh3,$(TARGET)),$(APP_HH3S)) Makefile
elf: $(APP_ELFS) Makefile

overrides: $(OVERRIDES) Makefile

all: launcher smoke
.DEFAULT_GOAL := launcher
.SECONDARY: # Prevents intermediate files from being deleted

sdk:
	@test -f "$(SDK_DIR)/Makefile" || { echo 'SDK missing: run git submodule update --init --recursive'; exit 1; }
	$(MAKE) -C "$(SDK_DIR)"

$(SDK_DIR)/libsdk.a:
	$(MAKE) sdk

launcher: sdk
	$(MAKE) bin elf adjusted overrides

smoke: sdk
	$(MAKE) -C smoke/$(TARGET) SDK_DIR="$(SDK_PATH)" APP_NAME=launcher-test
ifeq ($(TARGET),hh2)
	$(MAKE) -C smoke/hh3 SDK_DIR="$(SDK_PATH)" APP_NAME=launcher-test
endif

test:
	$(PYTHON) tools/run_tests.py host --variant $(TARGET) --outdir "$(OUTDIR)"

target-test: launcher smoke
	$(PYTHON) tools/run_tests.py target --variant $(TARGET) --outdir "$(OUTDIR)"

package: launcher smoke
	$(PYTHON) tools/package.py --variant $(TARGET) --outdir "$(OUTDIR)"

clean:
	rm -rf $(BUILDDIR) $(DEPDIR)
	rm -f $(APP_BINS) $(APP_ELFS) $(APP_ELFS:%=%.map) $(APP_HH3S) $(OVERRIDES)
	$(MAKE) -C smoke/$(TARGET) SDK_DIR="$(SDK_PATH)" APP_NAME=launcher-test clean


%.bin: %.elf
	$(OBJCOPY) --output-target=binary --set-section-flags .bss=alloc,load,contents,data $^ $@

%.hh3: %.elf
	$(STRIP) -o $@ $^

$(OUTDIR)/run.%.elf: $(OBJECTS) linker.ld $(BUILDDIR)/addresses.%.o $(SDK_DIR)/libsdk.a
	@mkdir -p $(dir $@)
	$(LD) -T linker.ld -Wl,-Map $@.map -o $@ $(LD_FLAGS) $(filter-out linker.ld $(SDK_DIR)/libsdk.a, $^) $(LIBS)

$(BUILDDIR)/run.%.adjusted.elf: $(OUTDIR)/run.%.elf
	@mkdir -p $(dir $@)
	$(OBJCOPY) --change-addresses +0x20000 $^ $@

$(OUTDIR)/addresses.%.override:
	@mkdir -p $(dir $@)
	cp $< $@

$(OUTDIR)/run.%.adjusted.hh3: $(BUILDDIR)/run.%.adjusted.hh3
	@mkdir -p $(dir $@)
	cp $< $@

$(NOLTOOBJS): FUNCTION_FLAGS+=-fno-lto

$(BUILDDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) -c $< -o $@ $(AS_FLAGS)

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@mkdir -p $(dir $(DEPDIR)/$<)
	$(CC) -c $< -o $@ $(CC_FLAGS) $(DEPFLAGS)

$(BUILDDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@mkdir -p $(dir $(DEPDIR)/$<)
	$(CXX) -c $< -o $@ $(CXX_FLAGS) $(DEPFLAGS)

$(BUILDDIR)/addresses.$(OS_VERSION).prep: $(ADDRESSES_FILE) convert_json.py
	@mkdir -p $(dir $@)
	$(PYTHON) convert_json.py $< $@

$(BUILDDIR)/addresses.%.o: $(SOURCEDIR)/addresses.template.c $(BUILDDIR)/addresses.%.prep
	$(CC) -c $< -o $@ $(CC_FLAGS) --embed-dir=$(BUILDDIR) -D PREP_FILE=\<$(@:$(BUILDDIR)/addresses.%.o=addresses.%.prep)\>

#clangd gets confused on these flags
compile_commands.json: clean
	bear --output $@.tmp -- sh -c "$(MAKE) --keep-going all || exit 0"
	#sed -i 's/-m4a-nofpu//' compile_commands.json.tmp
	mv compile_commands.json.tmp compile_commands.json

.PHONY: bin elf adjusted overrides all sdk launcher smoke test target-test package clean compile_commands.json

-include $(DEPFILES)

hh2 hh3:
	$(MAKE) TARGET=$@ all

.PHONY: hh2 hh3
