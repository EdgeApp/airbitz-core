TARGET ?= native
OBJDIR = build/tests/$(TARGET)
LIBDIR = ../deps/build/prefix/$(TARGET)/lib
CFLAGS   += -Wall -Wno-unused-variable  -Wno-write-strings -std=c99 -D_BSD_SOURCE -I../deps/build/prefix/$(TARGET)/include
CXXFLAGS += -Wall -Wno-unused-variable  -Wno-write-strings -I../src -I../deps/build/prefix/$(TARGET)/include
LDFLAGS = -L$(LIBDIR) -lscrypt -lpthread \
	`pkg-config --static --libs \
	libbitcoin libwallet libqrencode \
	libcurl jansson libcrypto \
	openssl `
PKG_CONFIG_PATH = ../deps/build/prefix/${TARGET}/lib/pkgconfig

c_sources=$(wildcard ../src/*.c)
cpp_sources=$(wildcard ../src/*.cpp)
test_sources=$(wildcard *.c)
objects=$(patsubst %.cpp,$(OBJDIR)/%.o,$(test_sources)) \
		$(patsubst ../src/%.c,$(OBJDIR)/%.o,$(c_sources)) \
        $(patsubst ../src/%.cpp,$(OBJDIR)/%.o,$(cpp_sources))

tests: $(objects)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $(OBJDIR)/$@

$(OBJDIR)/%.o: ../src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -MMD $(CFLAGS) -std=c99 -o $@ $<

$(OBJDIR)/%.o: ../src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c -MMD $(CXXFLAGS) -std=c++11 -o $@ $<

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c -MMD $(CFLAGS) -std=c99 -o $@ $<

clean:
	@rm -rf $(OBJDIR)

run: tests
	./$(OBJDIR)/tests

.PHONY: clean run
