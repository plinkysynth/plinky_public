kicad hardware files for plinky
==
with the exception of [WIP_inputfix](/hw/WIP_inputfix/), these files were created using kicad 5 and 6.

[BOM.md](/hw/BOM.md) contains a list of parts you'll need to recreate the kit, apart from the actual PCBs.


------
## Folders:

[testjig](/hw/testjig/) contains files for the testjig used to test plinky and flash it, calibrate CVs and so on, used during manufacturing of the kits.

[plinkyblackfront](/hw/plinkyblackfront/) contains files for the front and back panels. The plinky production runs with LEDs premade used front panels mdae by pcbway, while the ones without LEDs were made by jlcpcb. The back panels were made by jlcpcb.

[xyab](/hw/xyab/) contains a snapshot of the expander, which is also available on [github](https://github.com/plinkysynth/plinky-expander)

[v2](/hw/v2/) is the hardware design used most recently for plinky kits. it was manufactured by both pcbway and jlcpcb. there are BOM and POS files in here for JLCPCB to do the SMT assembly, and this was recently (jan-2023) tested in a small run.

[WIP_inputfix](/hw/WIP_inputfix/) has been updated to kicad 7 and has yet to be manufactured (as of june-2023); we are revising it to reduce input noise and improve the physical dimensions of some ports and the expander port hole.
