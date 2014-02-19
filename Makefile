TARGET ?= native
CFLAGS += -Wall -std=c99 -D_BSD_SOURCE -I./src/Scrypt

sources=$(wildcard src/*.c) $(wildcard src/Scrypt/*.c)

build/$(TARGET)/libabc.a: $(patsubst src/%.c,build/$(TARGET)/%.o,$(sources))
	$(AR) rcs $@ $^

clean:
	$(RM) libabc.a
	$(RM) -r build

# Automatic dependency rules:
build/$(TARGET)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -MMD $(CFLAGS) -o $@ $<

include $(shell find build/$(TARGET) -name *.d)
%.h: ;
