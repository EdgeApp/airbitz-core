#!/bin/bash

# Needed packages are:
# wget autoconf automake libtool pkgconfig

cross=arm-linux-androideabi
prefix=$(pwd)/prefix

# Create directories:
mkdir -p build
mkdir -p download
mkdir -p log

export PATH=$(pwd)/toolchain/bin:$PATH
export PKG_CONFIG_PATH=$prefix/lib/pkgconfig

# Helper function to download sources and unpack them:
# $1: download URL
# $2: folder name
unpack() {
    filename="download/$(echo "$1" | sed -e's@.*/@@'  -e's@\?.*@@')"
    dirname="$(echo "$1" | sed -e's@.*/@@' -e's@\.tar.*@@')"

    # Do the download:
    [ -e $filename ] || wget -O "$filename" "$1"

    # Unpack the contents:
    echo Unpacking $2
    tar -xf $filename
    rm -rf build/$2
    mv $dirname build/$2
}

setup-toolchain()
{
    case $(uname -sm) in
    "Linux x86_64")
        system=linux-x86_64
        download="http://dl.google.com/android/ndk/android-ndk-r9c-linux-x86_64.tar.bz2";;
    "Darwin x86_64")
        system=darwin-x86_64
        download="http://dl.google.com/android/ndk/android-ndk-r9c-darwin-x86_64.tar.bz2";;
    *)
        echo "Unknown host platform!"
        exit 1;;
    esac
    filename="download/$(echo "$download" | sed -e's@.*/@@'  -e's@\?.*@@')"
    [ -e $filename ] || wget -O "$filename" "$download"

    echo Unpacking NDK...
    [ -d build/ndk ] || ( tar -xf $filename ; mv android-ndk-r9c build/ndk )

    echo Unpacking toolchain...
    rm -r toolchain
    mkdir -p toolchain
    cd build/ndk
    ./build/tools/make-standalone-toolchain.sh --toolchain=arm-linux-androideabi-4.8 --install-dir=../../toolchain --system=$system
    cd ../..

    echo Patching toolchain...
    cat to_string.h >> toolchain/include/c++/4.8/bits/basic_string.h
}

build-zlib() {
    unpack http://zlib.net/zlib-1.2.8.tar.gz zlib

    echo Building zlib...
    cd build/zlib
    CHOST=$cross ./configure --static --prefix=$prefix
    make
    make install
    cd ../..
}

build-openssl() {
    unpack https://www.openssl.org/source/openssl-1.0.1f.tar.gz openssl

    echo Building openssl...
    cd build/openssl
    ./Configure --prefix=$prefix linux-generic32 -DL_ENDIAN no-shared zlib
    make CC="${cross}-gcc" AR="${cross}-ar r" RANLIB="${cross}-ranlib"
    make install_sw
    cd ../..
}

build-curl() {
    unpack http://curl.haxx.se/download/curl-7.34.0.tar.gz curl

    echo Building curl...
    cd build/curl
    ./configure --enable-static --disable-shared --host=$cross --prefix=$prefix LDFLAGS="-L${prefix}/lib" CPPFLAGS="-fPIC -I${prefix}/include"
    make
    make install
    cd ../..
}

build-zeromq() {
    unpack http://download.zeromq.org/zeromq-4.0.3.tar.gz zeromq

    echo Building zeromq...
    cd build/zeromq
    ./autogen.sh
    ./configure --enable-static --disable-shared --host=$cross --prefix=$prefix LDFLAGS="-L${prefix}/lib" CPPFLAGS="-fPIC -I${prefix}/include"
    make
    make install
    cd ../..
}

build-boost() {
    LIBRARIES=--with-libraries=date_time,filesystem,program_options,regex,signals,system,thread,iostreams

    unpack "http://downloads.sourceforge.net/project/boost/boost/1.55.0/boost_1_55_0.tar.bz2?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fboost%2Ffiles%2Fboost%2F1.55.0%2Fboost_1_55_0.tar.bz2%2Fdownload&ts=1390285679&use_mirror=softlayer-ams" boost
    [ -e download/boost.patch ] || curl -o download/boost.patch "https://raw2.github.com/MysticTreeGames/Boost-for-Android/master/patches/boost-1_55_0/boost-1_55_0.patch"
    [ -e download/boost.jam ] || curl -o download/boost.jam "https://raw2.github.com/MysticTreeGames/Boost-for-Android/master/configs/user-config-boost-1_55_0.jam"

    cd build/boost

    echo Bootstrapping boost...
    ./bootstrap.sh --prefix=$prefix $LIBRARIES

    echo Patching boost...
    cp ../../download/boost.jam tools/build/v2/user-config.jam
    patch -p1 < ../../download/boost.patch

    echo Building boost...
    export NO_BZIP2=1
    ./bjam -q toolset=gcc-androidR8b link=static threading=multi install

    cd ../..
}

build-libbitcoin() {
    unpack http://libbitcoin.dyne.org/download/libbitcoin-2.0.tar.bz2 libbitcoin

    echo Building libbitcoin...
    cd build/libbitcoin
    autoreconf -i
    ./configure --enable-static --disable-shared --host=$cross --prefix=$prefix LDFLAGS="-L${prefix}/lib" CPPFLAGS="-fPIC -I${prefix}/include"
    make
    make install
    cd ../..
}

build-jansson() {
    unpack www.digip.org/jansson/releases/jansson-2.5.tar.bz2 jansson

    echo Building jansson...
    cd build/jansson
    ./configure --enable-static --disable-shared --host=$cross --prefix=$prefix LDFLAGS="-L${prefix}/lib" CPPFLAGS="-fPIC -I${prefix}/include"
    make
    make install
    cd ../..
}

setup-toolchain

rm -r prefix
build-zlib 2>&1 | tee log/zlib.log
build-openssl 2>&1 | tee log/openssl.log
build-curl 2>&1 | tee log/curl.log
build-zeromq 2>&1 | tee log/zeromq.log
build-boost 2>&1 | tee log/boost.log
build-libbitcoin 2>&1 | tee log/libbitcoin.log
build-jansson 2>&1 | tee log/jansson.log
