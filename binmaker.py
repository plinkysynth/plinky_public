def main():
    # bin file maker
    ver1 = '0'
    ver2 = '0'
    ver3 = '0'
    try:
        with open("bootloader/Release/plinkybl.bin", "rb") as f1:
            bl_content = f1.read()
        with open("sw/Release/plinkyblack.bin", "rb") as f2:
            app_content = f2.read()
    except IOError as e:
        print(f"Failed to open file: {e}")
        exit(2)
    appsize = len(app_content)
    print(f'{appsize} app, {len(bl_content)} bootloader')
    # pad bl_content and app_content to reach their sizes
    bl_content += b'\xff' * (65536 - len(bl_content))
    app_content += b'\xff' * (1024 * 1024 - 65536 - len(app_content))

    app = bl_content + app_content

    if chr(app[appsize + 65536 - 6])=='v' and chr(app[appsize + 65536 - 4]) == '.':
        ver1 = chr(app[appsize + 65536 - 5])
        ver2 = chr(app[appsize + 65536 - 3])
        ver3 = chr(app[appsize + 65536 - 2])
    else:
        print("!!!!!!!!!!!!!!!! NO VERSION FOUND IN BIN FILE")
        exit(2)
    print(f"bootloader size {len(bl_content)}, app size {appsize}, version {ver1}{ver2}{ver3}")
    fname = f"plink{ver1}{ver2}{ver3}.bin"
    print(f'outputting {fname}...')
    try:
        with open(fname, "wb") as fo:
            fo.write(app)
    except IOError as e:
        print(f"Failed to write file: {e}")
        exit(2)
    fname = f"plink{ver1}{ver2}{ver3}.uf2"
    print(f'outputting {fname}...')
    # python ../../uf2conv.py -o ${ProjName}.uf2 -c -f STM32L4 ${ProjName}.hex
    class DotAccessibleObject:
        def __getattr__(self, key): return False
    uf2args = DotAccessibleObject()
    uf2args.output=fname
    uf2args.convert=True
    uf2args.family='STM32L4'
    uf2args.input='sw/Release/plinkyblack.hex'
    uf2args.base='0x2000'
    import uf2conv
    uf2conv.uf2conv(uf2args)


if __name__ == "__main__":
    main()
