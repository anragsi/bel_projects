PLATFMAKEFILE    := $(PLATFPATH)/Makefile

# variables (actually RAM_SIZE) are exported to allow recursive build of ram.ld in top make
export PLATFORM  := $(shell grep -m1 TARGET    $(PLATFMAKEFILE) | cut -d'=' -f2 | sed 's/[^a-zA-Z0-9]//g')
export DEVICE    := $(shell grep -m1 DEVICE    $(PLATFMAKEFILE) | cut -d'=' -f2 | sed 's/[^a-zA-Z0-9]//g')
export FLASH     := $(shell grep -m1 FLASH     $(PLATFMAKEFILE) | cut -d'=' -f2 | sed 's/[^a-zA-Z0-9]//g')
export SPI_LANES := $(shell grep -m1 SPI_LANES $(PLATFMAKEFILE) | cut -d'=' -f2 | sed 's/[^a-zA-Z0-9]//g')
export RAM_SIZE  := $(shell grep -m1 RAM_SIZE  $(PLATFMAKEFILE) | cut -d'=' -f2 | sed 's/[^a-zA-Z0-9]//g')

# obtain the number of MPS channels
FBAS_CMN_HDR_FILE:= fbas_common.h
N_MPS_CH         := $(shell grep -m1 N_MPS_CHANNELS $(FBAS_CMN_HDR_FILE) | tr -s ' ' | cut -d' ' -f3)
ifneq ($(N_MPS_CH), 1)
  export N_MPS_CH
else
  undefine N_MPS_CH
endif

$(info    N_MPS       is $(N_MPS_CH))

CFLAGS        = -I../include -I../../common-libs/include -I../../wb_timer -I../../../ip_cores/saftlib/drivers -I$(PATHFW) \
                -DPLATFORM=$(PLATFORM) -DDEBUGLEVEL=$(DEBUGLVL) $(EXTRA_FLAGS)
SRC_FILES     = $(PATHFW)/$(TARGET).c $(PATHFW)/tmessage.c  \
		$(PATHFW)/ioctl.c $(PATHFW)/measure.c       \
		$(PATHFW)/timer.c $(PATHFW)/fwlib.c \
		$(INCPATH)/ebm.c $(PATHFW)/../../common-libs/fw/common-fwlib.c

$(info    >>>>)
$(info    building is done by importing the following data from $(PLATFMAKEFILE):)
$(info    PLATFORM    is $(PLATFORM))
$(info    DEVICE      is $(DEVICE))
$(info    FLASH       is $(FLASH))
$(info    SPI_LANES   is $(SPI_LANES))
$(info    RAM_SIZE    is $(RAM_SIZE))
$(info    ----)
$(info    building firmware using)
$(info    SHARED_SIZE is $(SHARED_SIZE))
$(info    USRCPUCLK   is $(USRCPUCLK))
$(info    VERSION     is $(VERSION))
$(info    CFLAGS      is $(CFLAGS))
$(info    SRC_FILES   is $(SRC_FILES))
$(info    <<<<)

include ../../../syn/build.mk

fwbin: $(TARGET).bin
	@mv $^ $(TARGET)$(N_MPS_CH).$(PLATFORM).bin

$(TARGET).elf: $(SRC_FILES)
