![AirBitz Core Library](http://airbitz.co/static/img/bitcoin-wallet/section-bitcoin-wallet-platform-bg.jpg)
# Airbitz Wallet Core
[https://airbitz.co/developer-api-library](https://airbitz.co/developer-api-library)

This library implements the core Bitcoin functionality for the AirBitz wallet.
It manages accounts, syncing, and Bitcoin transactions.

## Building

The build process requires several pieces of software to be installed on the
host system:

* autoconf
* automake
* cmake
* git
* libtool
* pkgconfig
* protobuf

To install these on the Mac, please use [Homebrew](http://brew.sh/):

    brew install autoconf automake cmake git libtool pkgconfig protobuf

The 'wget' and 'cmake' that come from MacPorts are known to be broken.
If you are building for iOS or Mac native, you also need a working installation
of the XCode command-line tools.

For Linux native builds, you need the clang compiler.
The following command will install the necessary dependencies on Ubuntu:

    apt-get install autoconf automake cmake git libtool pkg-config protobuf-compiler clang

Assuming your system has the necessary command-line tools installed, it should
be possible to build ABC by doing something like:

    cd deps
    make

This will build a core and cli that can run your current system.
If you would like to cross-compile for another platform,
following commands are available inside the `deps` directory:

    make abc.build-android-arm
    make abc.build-android-x86
    make abc.ios-universal (only available on Mac)
    make abc.osx-universal (only available on Mac)
    make abc.build-native

The 'deps' system automatically downloads and builds the various open-source
libraries that the AirBitz core depends on. If you want to bypass the deps
system and run 'make' directly from the top-level directory,
you will need to manually pre-install these libraries on your system.
This approach makes the most sense for heavy day-to-day development,
even if it requires more up-front setup work.

## Directory structure

The entire library used to live in the "src" directory, but we are in the
process of re-designing the library's API. The new (work-in-progress) library
lives in the "abcd" directory, and the code in the "src" is just a shim that
adapts the new library to the old API. Once the GUI's switch to the new API,
the "src" directory will go away.

The "deps" directory contains a system for downloading and building all the
libraries that ABC depends on, as well as ABC itself, for mobile platforms.

The "minilibs" directory contains small support libraries that don't have
a standalone distribution.

The "cli" directory contains a command-line tool for exercising the core.
We use this internally for debugging and testing.

The "test" directory contains unit tests.

The "util" directory contains ancillary utilities,
such as a script for generating private keys from an exported wallet seed.
