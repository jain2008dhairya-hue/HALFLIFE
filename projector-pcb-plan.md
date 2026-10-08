# DIY Projector LED Driver Board: Design Plan v0.1

A first-board plan for a USB-C powered, 20 W LED driver with fan control and PWM dimming. Values marked **(verify)** must be taken from the official datasheet before you order boards.

## 1. Goal and power budget

| Item | Value |
|---|---|
| LED | 20 W COB, about 30 to 34 V at roughly 0.6 A (pick one whose hot and cold forward voltage stays under about 35 V) |
| Input | USB-C PD, requested at **15 V** (12 V fallback). Never request 20 V: the MIC3223 tops out at 20 V input |
| LED power | about 20 W |
| Boost input power | about 22 W (assuming roughly 90 % efficiency) |
| Fan and control | about 2 to 3 W |
| **Total from charger** | about 25 W, or about 1.7 A at 15 V |
| Charger needed | PD charger of 45 W or more that offers a 15 V profile, plus a cable rated for 3 A |

## 2. Block diagram

```
USB-C (J1) -> CH224K (U1, PD sink, requests 15 V)
          -> TVS + PTC fuse + bulk capacitor -> 15 V rail
15 V rail -> MIC3223 boost LED driver (U2) -> LED (about 33 V, 0.6 A)
15 V rail -> 5 V buck (U3) -> Raspberry Pi Pico (U4) + 5 V fan
Pico GPIO -> MIC3223 DIM_IN (PWM dimming)
Pico GPIO -> fan MOSFET (Q1)
NTC thermistor on heatsink -> Pico ADC (over-temperature shutoff)
```

## 3. Connections, block by block

### 3.1 USB-C input (CH224K)
- J1 receptacle: VBUS and GND pins to the input rail, CC1 and CC2 to the CH224K CC pins as in the datasheet reference schematic. The 16-pin type has two CC pins and both must be connected.
- Set the requested voltage to **15 V** using the CFG pins. Read the datasheet's configuration table for the exact pin levels. Do not guess them. CFG pins must stay at or below 3.7 V.
- Add two solder jumpers so you can pick 12 V or 15 V. Do not provide a 20 V option.
- **(verify)** The VDD and VBUS pin wiring and VDD supply range. Secondary sources I found disagree, so use the official WCH datasheet.
- Protection: TVS diode on VBUS, PTC fuse in series, and 100 uF bulk plus 10 uF ceramic capacitors. One hobbyist review reported a CH224K trigger board dying when a 20 V load was switched off, so keep the input clean.

### 3.2 LED driver (MIC3223)
- Supply input comes from the 15 V rail. Keep the input capacitors right next to the VIN and GND pins.
- Inductor L1 goes from the 15 V rail to the SW pin, then a Schottky diode D2 to the output capacitors and the LED+ terminal. **(verify)** L1 value, saturation current (aim for 4 A or more) and capacitor values from the datasheet design equations.
- Current sense resistor from the LED return to ground: the feedback voltage is 200 mV, so R = 0.2 V / 0.6 A = about 0.33 ohm. Two 0.68 ohm 1 % resistors in parallel give about 0.34 ohm (about 590 mA).
- OVP pin: resistor divider from the output. Set the threshold above the LED's maximum voltage and below the IC's 37 V boost limit. **(verify)** with the datasheet formula.
- COMP pin: series R and C to ground, values from the datasheet. Soft-start: capacitor value from the datasheet.
- EN pin: resistor divider from the 15 V rail so the driver stays off during the initial 5 V phase, before the charger switches to 15 V. **(verify)** the EN threshold.
- DIM_IN: PWM signal from a Pico GPIO. **(verify)** that the datasheet's logic-high threshold is met by 3.3 V.
- The exposed pad connects to the ground plane with several vias.

### 3.3 5 V rail, fan and control
- U3: small 5 V buck converter, 1 A or more, input rating of 20 V or more. Pick one with a datasheet reference design you can copy.
- Raspberry Pi Pico plugged into pin headers or soldered by its castellated edges, powered from 5 V at VSYS.
- Fan: 5 V fan, low-side logic-level N-MOSFET Q1 (gate from a Pico PWM pin, pull-down resistor on the gate, flyback diode across the fan).
- Thermistor: 10 k NTC on wires attached to the heatsink, in a divider with a 10 k resistor, read by a Pico ADC pin. Firmware: fan speed rises with temperature, and the LED switches off above a limit.

### 3.4 Connectors
- LED output: 2-position screw terminal (5.08 mm), clearly marked LED+ and LED-.
- Fan: 2-pin header or JST. Thermistor: 2-pin header or JST.
- Four M3 mounting holes. Board size target: about 60 x 45 mm.

## 4. KiCad workflow
1. New project. Draw the schematic block by block (power input, driver, control).
2. Symbols and footprints: check the KiCad library first. For the CH224K and MIC3223, if no symbol exists, make a custom symbol from the datasheet pin table, or import the part from a library such as the one on JLCPCB/EasyEDA. Double check footprint dimensions against the datasheet's land pattern.
3. Run ERC (electrical rules check) and fix every error.
4. Assign footprints, then update the PCB from the schematic.
5. Layout: 2 layers, 1.6 mm FR4. Place the high-current boost parts first.
6. Run DRC, then export Gerbers and a drill file (and BOM and placement files if you use JLCPCB assembly).

## 5. Layout rules for the power section
- Keep the **switch loop** small: the MIC3223 SW pin, inductor, Schottky diode and output capacitor should sit close together.
- Use wide traces or copper pours for high-current paths (15 V input, SW node, LED output): at least 1.5 mm wide, and wider where space allows.
- Use a solid ground plane on the bottom layer. Put the current sense resistor's return and the feedback trace away from the SW node.
- Keep the USB-C and CH224K area away from the switching node.
- Add test points for 15 V, 5 V, LED+ and GND.
- Leave copper area and clearance around the diode and inductor for heat.

## 6. Bring-up and testing (in this order)
1. Power the board from the USB-C charger with nothing else connected. Measure VBUS: it should read about 15 V.
2. Check the 5 V rail and that the Pico boots.
3. With the LED wired to the heatsink, fan running and PWM at the lowest duty cycle, turn on the driver and measure LED current. Raise brightness gradually.
4. Check temperatures after 10 minutes at full power, with the thermistor feeding the fan control.
5. Never look into the LED beam, and never run it without the heatsink.

## 7. Open items to confirm in datasheets
- CH224K: exact CFG pin levels for 15 V, VDD/VBUS wiring, and CC pin handling.
- MIC3223: inductor and capacitor values, OVP and EN divider formulas, DIM_IN logic levels, and maximum input voltage and absolute ratings against the TVS clamp voltage.
- LED: its exact forward voltage and current at your operating point.
- 5 V buck IC and TVS diode part numbers.
