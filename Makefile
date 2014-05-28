TARGET ?= native
CFLAGS += -std=c99 -Wall -D_GNU_SOURCE -DDEBUG
CXXFLAGS += -std=c++11 -Wall -D_GNU_SOURCE -DDEBUG
LDFLAGS+= -lbitcoin-client -lobelisk -lzmq -lsodium -lwallet -lbitcoin \
          -lboost_thread -lboost_system -lboost_regex -lboost_filesystem \
          -lqrencode -lcurl -ljansson -lssl -lcrypto -ldl -lz -lm -lscrypt

# We need -lpthread if not on Android:
ifeq (,$(findstring android,$(TARGET)))
	LDFLAGS += -lpthread
endif

c_sources=$(wildcard src/*.c)
cpp_sources=$(wildcard src/*.cpp)
objects=$(patsubst src/%.c,build/$(TARGET)/%.o,$(c_sources)) \
        $(patsubst src/%.cpp,build/$(TARGET)/%.o,$(cpp_sources))

# Top-level targets:
libabc.so: build/$(TARGET)/libabc.so link-test
libabc.a:  build/$(TARGET)/libabc.a

build/$(TARGET)/libabc.a: $(objects)
	$(AR) rcs $@ $^

build/$(TARGET)/libabc.so: $(objects)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

link-test: build/$(TARGET)/libabc.so
	$(CXX) -o build/$(TARGET)/link-test -Lbuild/$(TARGET) -labc link-test.cpp

clean:
	$(RM) libabc.a
	$(RM) -r build

# Automatic dependency rules:
build/$(TARGET)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -MMD $(CFLAGS) -o $@ $<

build/$(TARGET)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c -MMD $(CXXFLAGS) -o $@ $<

include $(shell find build/$(TARGET) -name *.d)
%.h: ;
