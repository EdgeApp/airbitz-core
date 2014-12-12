![AirBitz Core Library](http://airbitz.co/static/img/bitcoin-wallet/section-bitcoin-wallet-platform-bg.jpg)
# AirBitz Wallet Core
[https://airbitz.co/bitcoin-wallet-library](https://airbitz.co/bitcoin-wallet-library)

This library implements the core Bitcoin functionality for the AirBitz wallet.
It manages accounts, syncing, and Bitcoin transactions.

## Building

The build process requires several pieces of software to be installed on the
host system:

    git wget autoconf automake libtool pkgconfig cmake

To install these on the Mac, please use Homebrew. The 'wget' and 'cmake' that
come from MacPorts are known to be broken.

If you are building for iOS or Mac native, you also need a working installation
of the XCode command-line tools. For Linux native builds, you need the clang
compiler.

Assuming your system has the necessary command-line tools installed, it should
be possible to build an Android or iOS-compatible version of ABC by doing
something like:

    cd deps
    make

This will automatically guess the type of build you want to do. If you would
like to build for a specific platform, use one of the following inside the
'deps' directory:

    make abc.build-android-arm
    make abc.build-ios-universal (only works on a Mac)
    make abc.build-native

The 'deps' system automatically downloads and builds the various open-source
libraries that the AirBitz core depends on. If you want to bypass the deps
system and run 'make' directly from the top-level directory, you need to
manually compile all these dependencies and install them on your system. This
approach is a lot of work, but it makes sense if you often find yourself
modifying the dependencies.

## Directory structure

The entire library used to live in the "src" directory, but we are in the
process of re-designing the library's API. The new (work-in-progress) library
lives in the "abcd" directory, and the code in the "src" is just a shim that
adapts the new library to the old API. Once the GUI's switch to the new API,
the "src" directory will go away.

The "deps" directory contains a system for downloading and building all the
libraries that ABC depends on, as well as ABC itself, for mobile platforms.

The "util" directory contains a set of command-line tools that exercise the
core. We use these internally for debugging and testing.

The "server" directory contains the source code for our git replication
utility. We run this on our sync servers each time a wallet pushes new data,
copying the data to the other servers as well.

The "tests" directory contains some random test code. This is largely
unmaintained, and doesn't even compile. We will scrap this once the command-
line utilites in "util" are more complete, since those will allow us to do
all the testing we need with some simple shell-script wrappers. We should
also think about doing proper unit tests at some point in the future.
