cd sw/nocube_makefile
make clean
make -j BUILD_TYPE=RELEASE
python nocube_binmaker.py
cd ../..

