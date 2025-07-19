emcc -DEMU -DWASM gfx.c plinky.c -Os -s WASM=1 -s TOTAL_MEMORY=67108864 -o plinky.js   
