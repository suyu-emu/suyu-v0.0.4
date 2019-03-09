#!/bin/sh -ex

brew update
brew install p7zip qt5 sdl2 ccache
brew outdated cmake || brew upgrade cmake
pip3 install macpack
