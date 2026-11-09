TARGET = libcloudseed

MODULE_DIR = .
MODULES = \
FastSin \
MultitapDiffuser \
AudioLib/Biquad \
AudioLib/ShaRandom \
AudioLib/ValueTables \
Utils/Sha256 \


######################################
# building variables
######################################
# optimization (make DEBUG=1 adds debug info)
OPT = -O3

#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

######################################
# source
######################################

# CPP sources
CPP_SOURCES += $(addsuffix .cpp, $(MODULE_DIR)/$(MODULES))

#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CXX = $(GCC_PATH)/$(PREFIX)g++
AR = $(GCC_PATH)/$(PREFIX)ar
else
CXX = $(PREFIX)g++
AR = $(PREFIX)ar
endif

#######################################
# CFLAGS
#######################################

# language standard
CPP_STD = -std=gnu++14

# cpu
CPU = -mcpu=cortex-m7

# fpu
FPU = -mfpu=fpv5-d16

# float-abi
FLOAT-ABI = -mfloat-abi=hard

# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

C_INCLUDES = \
-I. \

# compile gcc flags
CFLAGS = $(MCU) $(C_INCLUDES) $(OPT) -Wall -Werror -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# Generate dependency information. The default rule target (the .o) makes a header
# edit rebuild every object that includes it.
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

CPPFLAGS = $(CFLAGS)
CPPFLAGS += \
-fno-exceptions \
-ffast-math \
-finline-functions \
-fno-aggressive-loop-optimizations \
-Wdouble-promotion

# default action: build all
all: $(BUILD_DIR)/$(TARGET).a 

#######################################
# build the library
#######################################

# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(CPP_SOURCES:.cpp=.o)))
vpath %.cpp $(sort $(dir $(CPP_SOURCES)))

$(BUILD_DIR)/%.o: %.cpp Makefile | $(BUILD_DIR)
	$(CXX) -c $(CPPFLAGS) $(CPP_STD) -static -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.cpp=.lst)) $< -o $@

$(BUILD_DIR)/$(TARGET).a: $(OBJECTS) Makefile
	$(AR) rcs $@ $(OBJECTS)

$(BUILD_DIR):
	mkdir $@        

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)
#######################################

# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)
