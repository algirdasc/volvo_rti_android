
# Android Auto project on Volvo XC70 (P3) RTI

This is still WIP. Unfinished version. Pre-ALPHA stage.

# RTI differences between Volvo platforms
My XC70 came with newer type of RTI screen. It had two FAKRA connectors from both sides. According to electrical schematics, I guess it's for DVD input and rear view camera video input. Also it had 6-pin connector (red, green, blue, v-sync, h-sync, and gnd) instead of 10-pin. I could not manage to control my screen via CANBUS, so the cheapest and fastest way was to retrofit RTI screen from P2 Volvo. I found it used for only 12€ and it can be changed without any modifications. **Just keep in mind, that blue connector for power is different. New RTI has two CANBUS wires, where old RTI has +12V, GND and +12 ignition**. Also, P3 steering wheel controls works on LINBUS, instead of CANBUS (as in P2 Volvo's). 

The video quality of original RTI matrix is usable, but poor for this century standards. Luckily there is Aliexpress with it's bottomless supply of electronics. I've ordered different screen and control board to easily connect RPi via HDMI instead of composite video. But more on that later. I'm still waiting for it to arrive.
# Credits

 - [laurynas](https://github.com/laurynas/volvo_linbus) - Volvo LINBUS / Crankshaft
 - [luuk](https://github.com/LuukEsselbrugge/Volve) - Volvo S60 project
