# Chip8 Emulator Lib and SDL Frontend

Public domain roms obtained from:
https://www.zophar.net/pdroms/chip8.html

## Building and Running

```
meson setup --buildtype release build
ninja -C build
./build/front/chip8 roms/TICTAC
```
