#!/bin/bash
cd /var/home/sanya/wisper/core/.mounted/Neko_Throne
mkdir -p build && cd build
cmake -GNinja ..
ninja
