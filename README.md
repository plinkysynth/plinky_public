# plinky

an 8 voice touch synth - https://plinkysynth.com

I apologise, its been so long since I looked at this, I am releasing a snapshot of what I had on my harddrive after a couple of years of barely being touched. while I am not actively developing plinky, I hope that by open sourcing it I can stimulate the wonderful community who collaborate on the plinky discord. I also hope to help iterate on some of the more obvious bugs, both hardware and software. 

If you are interested in plinky, please feel free to join our discord community. More information at https://plinkysynth.com


License stuff
--
Please refer to the [license](LICENSE.md) for information including stuff about derived works.

Contributing stuff
--
please see [CONTRIBUTING.md](CONTRIBUTING.md) for information on contributing to this project.

Building and stuff
--
you need [STM32 cube ide](https://www.st.com/en/development-tools/stm32cubeide.html), its basically the default project so most of the code is in [/sw/Core/Src](/sw/Core/Src), most of the rest of the [/sw](/sw) folder is just the usual auto generated boilerplate mess.

It would be a useful project to update the firmware to build with a more minimal setup; the only significant middleware used relates to USB; while I lean on the STM default project a little, I imagine that the peripheral code could be modified fairly easily.

it should also compile as an emulator for mac and windows, using portaudio and dear imgui. it also compiles for wasm, somehow... I imagine that following the path of the WASM emulator, it would be relatively easy to port plinky to software plugin type platforms.

there is also a [bootloader](/bootloader/) project... I forget exactly how it relates to the final build process. Consider that a TODO to document :)

the hardware is mostly in the [hw/](/hw/) folder, but I was new to kicad so I am not sure if all the library stuff is in the right place.

if there is something missing or something in there that you think shouldn't be in there, let me know and I can sort it out


Will there still be kit plinkys? eg from Thonk?
--
yes! thanks to the wonderful makingsonudmachines, v3 kits are starting to appear at thonk! thanks roland, enrica and steve!  https://www.thonk.co.uk/shop/plinky-kit-001/

Thanks!
--
Many thanks to the plinky community, who have helped plinky evolve and kept the dream alive over a few years. 

Thanks to Steve from https://thonk.co.uk who encouraged plinky into existence and helped produce the kits; to Kay from https://leipzigwest.org/ for tireless hardware help, encouragment and pre-made plinkys! To Stijn from http://www2.thisisnotrocketscience.nl/ for support, hardware help, and the impetus to opensource this.

Cheers all! 

-- [mmalex](https://twitter.com/mmalex)

