emcc -DEMU -DWASM gfx.c plinky.c -O0 -g -s WASM=1 -s INITIAL_MEMORY=67108864 -o plinky.js   -s ENVIRONMENT=web -s EXPORT_NAME="Module" -s MODULARIZE=0
