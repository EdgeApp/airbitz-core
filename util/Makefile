CFLAGS   += -Wall -D_GNU_SOURCE -I../src -std=c99 -O2
CXXFLAGS += -Wall -D_GNU_SOURCE -I../src -std=c++11 -O2 \
	$(shell pkg-config --cflags libwallet)
LDFLAGS  += -L../build/
LIBS     += -labc -lpthread \
	$(shell pkg-config --libs libwallet)

default: build/account-decrypt.bin \
         build/account-encrypt.bin \
         build/add-category.bin \
         build/change-password.bin \
         build/check-recovery-answers.bin \
         build/create-account.bin \
         build/create-wallet.bin \
         build/data-sync.bin \
         build/generate-addresses.bin \
         build/get-bitcoin-seed.bin \
         build/get-categories.bin \
         build/get-question-choices.bin \
         build/get-exchange-rate.bin \
         build/get-settings.bin \
         build/list-wallets.bin \
         build/remove-category.bin \
         build/set-nickname.bin \
         build/set-pin.bin \
         build/set-question-choices.bin \
         build/search-bitcoin-seed.bin \
         build/sign-in.bin \
         build/upload-logs.bin \
         build/wallet-decrypt.bin \
         build/wallet-encrypt.bin \
         build/wallet-get-address.bin \
         build/washer.bin

build/%.bin: build/%.o build/common.o
	@mkdir -p build
	$(CXX) -g $(LDFLAGS) -o $@ $^ $(LIBS)

build/%.o: %.c common.h
	@mkdir -p build
	$(CC) -c -g $(CFLAGS) -o $@ $<

build/%.o: %.cpp common.h
	@mkdir -p build
	$(CXX) -c -g $(CXXFLAGS) -o $@ $<

clean:
	$(RM) -r build
