TARGET ?= native
CFLAGS += -Wall -std=c99 -D_BSD_SOURCE

c_sources=$(wildcard src/*.c)
cpp_sources=$(wildcard src/*.cpp)
objects=$(patsubst src/%.c,build/$(TARGET)/%.o,$(c_sources)) \
        $(patsubst src/%.cpp,build/$(TARGET)/%.o,$(cpp_sources))

build/$(TARGET)/libabc.a: $(objects)
	$(AR) rcs $@ $^

clean:
	$(RM) libabc.a
	$(RM) -r build

# Automatic dependency rules:
build/$(TARGET)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -MMD $(CFLAGS) -o $@ $<

build/$(TARGET)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c -MMD $(CXXFLAGS) -std=c++11 -o $@ $<

include $(shell find build/$(TARGET) -name *.d)
%.h: ;
