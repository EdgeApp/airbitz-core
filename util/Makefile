CFLAGS   += -Wall -D_GNU_SOURCE -I../src -std=c99
CXXFLAGS += -Wall -D_GNU_SOURCE -I../src -std=c++11
LDFLAGS  += -L../build/
LIBS     += -labc -lpthread

default: build/create-account.bin \
		 build/change-password.bin \
		 build/get-pin.bin \
		 build/set-pin.bin \
		 build/set-question-choices.bin \
		 build/sign-in.bin

build/%.bin: build/%.o build/common.o
	@mkdir -p build
	clang -g $(LDFLAGS) -o $@ $^ $(LIBS)

build/%.o: %.c common.h
	@mkdir -p build
	$(CC) -c -g $(CFLAGS) -o $@ $<

clean:
	$(RM) -r build
