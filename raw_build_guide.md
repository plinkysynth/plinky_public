# rough notes on building a plinky from scratch

## hardware

order from JLC :)
TODO more info

## software

### flashing a blank plinky
It's possible to flash a freshly manufactured plinky without building the software. you can get the latest firmware as a .bin file from (somewhere TODO link probably a github release). 

The bin file is typically named `plink09z.bin` or similar, where the last 3 digits represent the version number. Note that the .uf2 version of this file is just the main firmware, and can be installed via plinky's bootloader (as detailed in the manual). However, that only works once the bootloader is put onto plinky in the first place! a freshly manufactured plinky has no such bootloader, and that's why you need the .bin file. 

The bin file can be placed onto the plinky using STM32CubeProgrammer, which is available for free from [ST here](https://www.st.com/en/development-tools/stm32cubeprog.html). 

At the time of writing, stm32cubeprog seems to have problems running on a mac. Even after allowing it to run in the Settings & Security part of the OSX control panel, it often hangs or fails to open entirely. you may need to use a PC, linux, wait for ST to fix it, or fight through various forum posts online. 

Assuming you have got it to run...

First, find the 'boot0' and nearby '3v3' pad on the back of plinky. you need to short these together using a piece of metal or pliers or similar, as you plug plinky in to a PC via USB for the first time. Once power is applied over USB, you can remove your piece of metal. Then, when you run STM32CubeProg, you should be able to load your bin file, then on the right side select 'USB' as the connection type and hit the 'refresh' button to show the USB device. you can then click 'DOWNLOAD' button to flash the plinky. That's it!

### building the firmware from source

* check out the github repo https://github.com/plinkysynth/plinky_public 
* install a fresh install of STM32CubeIDE, available for free from [ST here](https://www.st.com/en/development-tools/stm32cubeide.html) for mac, windows and linux
    * at the time of writing, the latest version is 1.13
* you need python 3.x to be installed and working (check if you can run python from the commandline)
* more steps TODO

## booting a raw plinky for the first time

analog voltage calibration process docs todo
