kicad hardware files for plinky
==

## OPENSOURCE HARDWARE CREDITS
Many thanks to the people who worked so hard to make opensource plinky a reality, far better than the original version.

* **Stijn** from [This Is Not Rocket Science](https://www.thisisnotrocketscience.nl/) routed the original Version 3.
* **Steve / Okyreon** from [denki oto](https://www.okyeron.com/denki_oto/about.php) added the LPZW midi circuit to allow plinky to have on-board midi without the expander.
* **Kay** from [LPZW Modules](https://leipzigwest.org/)  then contributed a circuit to fix the input noise, and then polished everything into a 'Version 3.1', which is the current version linked here. Kay also was one of the original people who helped me with the hardware design of V1 and V2.

If you contributed to the open source hardware and I forgot you, please shout at me @mmalex on the Plinky Discord.
Thanks to all the Plinky Discord members for making Plinky ever better!


------
## The Main Folders:

[BOM.md](/hw/BOM.md) contains a list of parts you'll need to recreate the kit, apart from the actual PCBs.

[plinkyblack](/hw/plinkyblack/) contains the files for the main plinky pcb, OSS edition. this was originally called v3.1 LPZW, and represents the latest and greatest OSS hardware release, thanks to LPZWModules for the most recent revision. See the credits paragraph above for more information on all the great people who helped on the hardware side to make OSS plinky a reality (and much better than the original plinky!)

[plinkyblackfront](/hw/plinkyblackfront/) contains files for the front and back panels. The plinky production runs with LEDs premade used front panels mdae by pcbway, while the ones without LEDs were made by jlcpcb. The back panels were made by jlcpcb.

[plinkyblackfront_osh](/hw/plinkyblackfront_osh/) is a variation of the front panel with the OSH logo replacing the plinky logo's dot on the i.

## Additional Folders:

[testjig](/hw/testjig/) contains files for the testjig used to test plinky and flash it, calibrate CVs and so on, used during manufacturing of the kits.

[expander](/hw/expander/) contains a snapshot of the expander, which is also available on [github](https://github.com/plinkysynth/plinky-expander)

## Older stuff:

There is also a big folder of archived old designs. this is now in [archived](/hw/archived/).

[archived/v2](/hw/archived/v2/) is the hardware design used most recently for plinky kits. it was manufactured by both pcbway and jlcpcb. there are BOM and POS files in here for JLCPCB to do the SMT assembly, and this was recently (jan-2023) tested in a small run.

[archived/WIP_inputfix](/hw/archived/WIP_inputfix/) has been updated to kicad 7 and has yet to be manufactured (as of june-2023); we are revising it to reduce input noise and improve the physical dimensions of some ports and the expander port hole.

[archived/plinkyblack_hwmidi](/hw/archived/plinkyblack_hwmidi/) Kicad 7 version modified by @okyeron which adds hardware MIDI in/out on the front of the device. Includes modifications from WIP_inputfix
