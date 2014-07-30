CFLAGS   += -Wall -D_GNU_SOURCE -I../src -std=c99
CXXFLAGS += -Wall -D_GNU_SOURCE -I../src -std=c++11
LDFLAGS  += -L../build/
LIBS     += -labc -lpthread

default: build/account-decrypt.bin \
         build/add-category.bin \
         build/change-password.bin \
         build/check-recovery-answers.bin \
         build/create-account.bin \
         build/create-wallet.bin \
         build/get-bitcoin-seed.bin \
         build/get-categories.bin \
         build/get-pin.bin \
         build/get-question-choices.bin \
         build/get-settings.bin \
         build/remove-category.bin \
         build/set-nickname.bin \
         build/set-pin.bin \
         build/set-question-choices.bin \
         build/sign-in.bin

build/%.bin: build/%.o build/common.o
	@mkdir -p build
	$(CC) -g $(LDFLAGS) -o $@ $^ $(LIBS)

build/%.o: %.c common.h
	@mkdir -p build
	$(CC) -c -g $(CFLAGS) -o $@ $<

clean:
	$(RM) -r build
