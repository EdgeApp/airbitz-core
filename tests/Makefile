BUILD_DIR ?= ../deps/build

ifndef TARGET
	TARGET = native
	LDFLAGS += -L$(BUILD_DIR)/prefix/$(TARGET)/lib -labc
else
	LDFLAGS += -labc
endif

ifeq (native, $(TARGET))
	LDFLAGS += -lpthread
endif

CFLAGS   += -Wall -D_GNU_SOURCE -I../src -std=c99
CXXFLAGS += -Wall -D_GNU_SOURCE -I../src -std=c++11

test_core: test_core.o $(objects)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

default: all

all: $(test_cases)

clean:
	@rm -rf $(OBJDIR) *.o $(test_cases)

LD_LIBRARY_PATH=$(BUILD_DIR)/prefix/$(TARGET)/lib/
run: $(test_cases)
	$<

.PHONY: all clean run objdir
