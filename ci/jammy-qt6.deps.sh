#!/bin/bash -eux

echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections

apt-get update -qq
apt-get install -y \
    --allow-change-held-packages \
    --allow-downgrades \
    --allow-remove-essential \
    --allow-unauthenticated \
     binutils gcc-12 g++-12 clang-14 clang++-14 lld-14 \
     software-properties-common wget \
     libasound-dev \
     ninja-build cmake \
     libfftw3-dev \
     libsuil-dev liblilv-dev lv2-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev \
     libqt6core5compat6-dev \
     libqt6opengl6-dev \
     libqt6websockets6-dev \
     libqt6serialport6-dev \
     libqt6shadertools6-dev \
     libbluetooth-dev libsdl2-dev libsdl2-2.0-0 \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
     file \
     dpkg-dev

source ci/common.deps.sh
