BUILD_DIR ?= ../deps/build

TARGET ?= native
OBJDIR = build/tests/$(TARGET)
LIBDIR = $(BUILD_DIR)/prefix/$(TARGET)/lib
CFLAGS   += -std=c99 -Wall -D_GNU_SOURCE -I../src/ -isystem $(BUILD_DIR)/prefix/$(TARGET)/include
CXXFLAGS += -std=c++11  -Wall -I../src -isystem $(BUILD_DIR)/prefix/$(TARGET)/include
LDFLAGS = -L$(LIBDIR) -lscrypt -lpthread \
	`pkg-config --static --libs \
	libbitcoin libwallet libqrencode \
	libcurl jansson libcrypto \
	openssl `
PKG_CONFIG_PATH = $(BUILD_DIR)/prefix/${TARGET}/lib/pkgconfig

test_cases=test_core
c_sources=$(wildcard ../src/*.c)
cpp_sources=$(wildcard ../src/*.cpp)
objects=$(patsubst ../src/%.c,$(OBJDIR)/%.o,$(c_sources)) \
        $(patsubst ../src/%.cpp,$(OBJDIR)/%.o,$(cpp_sources))

$(OBJDIR)/%.o: ../src/%.c | objdir
	@mkdir -p $(OBJDIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: ../src/%.cpp | objdir
	@mkdir -p $(OBJDIR)
	$(CXX) -c -MMD $(CXXFLAGS) -o $@ $<

test_core: test_core.o $(objects)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

default: all

all: $(test_cases)

clean:
	@rm -rf $(OBJDIR) *.o $(test_cases)

run: $(test_cases)
	$<

.PHONY: all clean run objdir
