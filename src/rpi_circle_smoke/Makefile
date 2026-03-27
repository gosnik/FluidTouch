CIRCLEHOME ?= ../../third_party/circle-stdlib/libs/circle
PROJECTROOT ?= ../..
CIRCLE_STDLIB_HOME ?= $(abspath $(CIRCLEHOME)/../..)

-include $(CIRCLE_STDLIB_HOME)/Config.mk

ifneq ($(strip $(NEWLIB_ARCH)),)
NEWLIBDIR := $(CIRCLE_STDLIB_HOME)/install/$(NEWLIB_ARCH)
endif

OBJS = main.o

LIBS = $(if $(NEWLIBDIR),$(NEWLIBDIR)/lib/libm.a $(NEWLIBDIR)/lib/libc.a $(NEWLIBDIR)/lib/libcirclenewlib.a,) \
       $(CIRCLEHOME)/addon/lvgl/liblvgl.a \
       $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/sched/libsched.a \
       $(CIRCLEHOME)/lib/libcircle.a

EXTRAINCLUDE += -I $(PROJECTROOT)/include \
                $(if $(NEWLIBDIR),-I $(NEWLIBDIR)/include,) \
                $(if $(STDDEF_INCPATH),-I $(STDDEF_INCPATH),) \
                $(if $(wildcard $(CIRCLE_STDLIB_HOME)/include),-I $(CIRCLE_STDLIB_HOME)/include,) \
                -I $(CIRCLEHOME)/addon/lvgl \
                -I $(CIRCLEHOME)/addon/lvgl/lvgl

DEFINE += -DLV_FONT_MONTSERRAT_18=1 -DLV_FONT_MONTSERRAT_32=1
CHECK_DEPS := 0
STDLIB_SUPPORT := 3

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
