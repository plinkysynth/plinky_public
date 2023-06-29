// plinkyfwpkg.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <windows.h>
typedef unsigned char u8;

int main(int argc, char **argv)
{
	// bin file maker
	{
		FILE* ff = fopen("resetpresets.bin", "wb");
		int minus1 = -1;
		for (int i = 0; i < 510 * 1024; ++i) fwrite(&minus1, 1, 1, ff);
		fclose(ff);
		char ver1 = '0', ver2 = '0', ver3 = '0';
		FILE* f1 = fopen("../../bootloader/Release/plinkybl.bin", "rb");
		FILE* f2 = fopen("../Release/plinkyblack.bin", "rb");
		u8* app = (u8*)malloc(1024 * 1024);
		memset(app, 0xff, 1024 * 1024);
		int blsize = fread(app, 1, 65536, f1);
		int appsize = fread(app + 65536, 1, 1024 * 1024 - 65536, f2);
		if (app[appsize + 65536 - 6] == 'v' && app[appsize + 65536 - 4] == '.') {
			ver1 = app[appsize + 65536 - 5];
			ver2 = app[appsize + 65536 - 3];
			ver3 = app[appsize + 65536 - 2];
		}
		else {
			printf("!!!!!!!!!!!!!!!! NO VERSION FOUND IN BIN FILE\n");
			exit(2);
		}
		printf("bootloader size %d, app size %d, version %c%c%c\n", blsize, appsize, ver1, ver2, ver3);
		fclose(f1);
		fclose(f2);
		char fname[256];
		sprintf(fname, "../../docs/web/fw/plink%c%c%c.bin", ver1, ver2, ver3);
		FILE* fo = fopen(fname, "wb");
		fwrite(app, 1, appsize + 65536, fo);
		fclose(fo);
		sprintf(fname, "../../docs/web/fw/plink%c%c%c.uf2", ver1, ver2, ver3);
		CopyFileA("../Release/plinkyblack.uf2", fname, false);
		sprintf(fname, "../../docs/web/fw/pldbg%c%c%c.uf2", ver1, ver2, ver3);
		CopyFileA("../Debug/plinkyblack.uf2", fname, false);

	}
}

