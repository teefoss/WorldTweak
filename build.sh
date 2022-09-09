#!/bin/bash
set -x
cc *.c mylib/*.c -I/usr/local/include/SDL2 -o worldtweak -lSDL2
