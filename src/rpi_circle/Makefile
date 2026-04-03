CIRCLEHOME ?= ../../third_party/circle-stdlib/libs/circle
PROJECTROOT ?= ../..
CIRCLE_STDLIB_HOME ?= $(abspath $(CIRCLEHOME)/../..)
PIOENV ?= rpi_circle

-include $(CIRCLE_STDLIB_HOME)/Config.mk

ifneq ($(strip $(NEWLIB_ARCH)),)
NEWLIBDIR := $(CIRCLE_STDLIB_HOME)/install/$(NEWLIB_ARCH)
endif

UI_CPP_SRCS := $(shell find $(PROJECTROOT)/src/ui -type f -name '*.cpp')
UI_C_SRCS :=
ENV_CPP_SRCS := $(shell find $(PROJECTROOT)/src/env/common $(PROJECTROOT)/src/env/rpi -type f -name '*.cpp')
CORE_CPP_SRCS := $(PROJECTROOT)/src/core/qtdial_button_mapping.cpp

OBJS = main.o \
       $(UI_CPP_SRCS:.cpp=.o) \
       $(UI_C_SRCS:.c=.o) \
       $(CORE_CPP_SRCS:.cpp=.o) \
       $(ENV_CPP_SRCS:.cpp=.o)

LIBS = $(if $(NEWLIBDIR),$(NEWLIBDIR)/lib/libm.a $(NEWLIBDIR)/lib/libc.a $(NEWLIBDIR)/lib/libcirclenewlib.a,) \
       $(CIRCLEHOME)/addon/lvgl/liblvgl.a \
       $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/sched/libsched.a \
       $(CIRCLEHOME)/lib/libcircle.a

EXTRACLEAN += $(UI_CPP_SRCS:.cpp=.o) $(UI_CPP_SRCS:.cpp=.d) \
              $(UI_C_SRCS:.c=.o) $(UI_C_SRCS:.c=.d) \
              $(CORE_CPP_SRCS:.cpp=.o) $(CORE_CPP_SRCS:.cpp=.d) \
              $(ENV_CPP_SRCS:.cpp=.o) $(ENV_CPP_SRCS:.cpp=.d)

ARDUINOJSON_INCLUDE := $(PROJECTROOT)/.pio/libdeps/$(PIOENV)/ArduinoJson/src
WEBSOCKETS_INCLUDE := $(PROJECTROOT)/.pio/libdeps/$(PIOENV)/WebSockets/src

EXTRAINCLUDE += -I $(PROJECTROOT)/include \
                -I $(PROJECTROOT)/src/env/rpi/compat \
                -I $(PROJECTROOT)/src/pc/compat \
                $(if $(wildcard $(ARDUINOJSON_INCLUDE)),-I $(ARDUINOJSON_INCLUDE),) \
                $(if $(wildcard $(WEBSOCKETS_INCLUDE)),-I $(WEBSOCKETS_INCLUDE),) \
                $(if $(NEWLIBDIR),-I $(NEWLIBDIR)/include,) \
                $(if $(STDDEF_INCPATH),-I $(STDDEF_INCPATH),) \
                $(if $(wildcard $(CIRCLE_STDLIB_HOME)/include),-I $(CIRCLE_STDLIB_HOME)/include,) \
                -I $(CIRCLEHOME)/addon/lvgl \
                -I $(CIRCLEHOME)/addon/lvgl/lvgl

DEFINE += -DFT_PLATFORM_RPI -DFT_WIFI_ENABLED=0 -DENABLE_SCREENSHOT_SERVER=0 -DLV_FONT_MONTSERRAT_26=1 -DLV_FONT_MONTSERRAT_32=1
CHECK_DEPS := 0
STDLIB_SUPPORT := 3

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
