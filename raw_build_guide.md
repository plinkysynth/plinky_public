# rough notes on building a plinky from scratch

## hardware

order from JLC :)
TODO more info

## software

### flashing a blank plinky
It's possible to flash a freshly manufactured plinky without building the software. you can get the latest firmware as a .bin file, for example from https://github.com/plinkysynth/plinky_public/raw/main/plink09z.bin
You can also build it from source, more information further down this document.

The bin file is typically named `plink09z.bin` or similar, where the last 3 digits represent the version number. Note that the .uf2 version of this file is just the main firmware, and can be installed via plinky's bootloader (as detailed in the manual). However, that only works once the bootloader is put onto plinky in the first place! a freshly manufactured plinky has no such bootloader, and that's why you need the .bin file. 

The bin file can be placed onto the plinky using STM32CubeProgrammer, which is available for free from [ST here](https://www.st.com/en/development-tools/stm32cubeprog.html). 

At the time of writing, stm32cubeprog seems to have problems running on a mac. The first time you run it, you will see:

<img src="imgs/macsarecrap1.png" width="250">

Even after allowing it to run in the Settings & Security part of the OSX control panel by clicking 'open anyway':

<img src="imgs/macsarecrap2.png" width="350">

it often hangs or fails to open entirely. I do not know why.. you may need to use a PC, linux, wait for ST to fix it, or fight through various forum posts online, possibly installing outdated JREs and so forth. The page I found most helpful, that got it running for me, was https://community.st.com/t5/stm32cubeprogrammer-mcu/how-to-download-stm32cubeprogrammer-on-macos-monterey-12-6/m-p/143983 - but follow at your own risk!

Assuming you have got it to run... when you run the programmer, you see this screen

<img src="imgs/programmer.png">

Now, connect the plinky - the boot0 way!

What's that you ask?

First, find the 'boot0' and nearby '3v3' pad on the back of plinky. you need to short these together using a piece of metal or pliers or similar, BEFORE and then as you plug plinky in to the computer via USB for the first time. Once power is applied over USB, you can remove your piece of metal. 

Now inside STM32CubeProg, you should be able to load your bin file (click Open file, see red box in screenshot), then on the right side select 'USB' (red box around USB) as the connection type and hit the 'refresh' (small icon button on the far right) button to show the USB device. If it says 'No DFU' like in the screenshot, it means you didn't succesfully short the BOOT0 and 3V3 pins. Unplug plinky, short those pads, and plug it in again. Hit refresh and keep trying until it works. you can then click 'Connect' green button, and then there should be a 'DOWNLOAD' button (not shown in the screenshot) to flash the plinky. It's not a great UI, but hey, complain to ST! :) Anyway, your plinky is now flashed! Unplug it and replug it and it should go into calibration. See the bottom of this document for more information about that process.

### building the firmware from source

* check out the github repo https://github.com/plinkysynth/plinky_public 
* install a fresh install of STM32CubeIDE, available for free from [ST here](https://www.st.com/en/development-tools/stm32cubeide.html) for mac, windows and linux
    * at the time of writing, the latest version is 1.13
    * like STM32CubeProgrammer above, on a mac this will refuse to open the first time. see the discourse above for some tips. the good news is that once you click 'run anyway', it seems more stable than the programmer. 
* you need python 3.x to be installed and working (check if you can run python from the commandline)

On first boot, STM32CubeIDE, after choosing a workspace folder, should present you with a welcome screen that allows you to 'import a project'. Do so, giving this confusing dialog:

<img src="imgs/importdialog.png">

Click 'directory...' and select the directory that is the root of your git checkout. This should then show you 3 projects with checks - the first being the folder as a whole, the second being the 'sw' project (main plinky firmware), and the third being the 'bootloader' project. Make sure the latter two are checked; uncheck the root folder. click ok.

You should see something like this:

<img src="imgs/project.png">

Right click on each of the projects on the left panel, and in the context menu select 'Build Configurations' / 'Set Active' / 'Release'.

Now go to the app's 'Project' menu and select 'Build All'.

If all goes well, that should succeed and write files into the `bootloader/Release` and `sw/Release` folders.

the last step is to run the `binmaker.py` script.
open a terminal window (cmd on windows), in the root of the github repo. run `python binmaker.py` which should output something like:

```
# python binmaker.py
456950 app, 30968 bootloader
bootloader size 65536, app size 456950, version 09z
outputting plink09z.bin...
outputting plink09z.uf2...
Converting to uf2, output size: 873472, start address: 0x8010000
Wrote 873472 bytes to plink09z.uf2
```

CONGRATULATIONS! this generates two files - `plink09z.bin` and `plink09z.uf2` (or whatever version).
the .bin file can be flashed onto a raw plinky using the BOOT0 method outlined at the top of this document.
once a plinky has the bootloader flashed onto it, the uf2 file can be used to update just the app portion, using the method in the public docs.


## troubleshooting

TODO

## booting a raw plinky for the first time

analog voltage calibration process docs todo
