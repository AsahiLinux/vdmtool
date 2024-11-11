# Introduction

This is a simple tool for sending and receiving USB-PD VDM messages using a FUSB302, based on [usb-c-arduino](https://github.com/graycatlabs/usb-c-arduino).

USB-PD VDM documentation is [here](https://github.com/AsahiLinux/docs/wiki/Hardware:USB-PD)

# Wiring
#### Bare FUSB
You can use a breakout board or a bare [FUSB302](https://www.onsemi.com/pub/Collateral/FUSB302-D.PDF). You need to wire it as follows:

* 1 CC2 → Type-C CC2 (B5) - ONLY wire this to a Type-C *socket* used with a cable. Otherwise leave open.
* 2 VBUS → Arduino D4, Type-C VBUS (A4/A9/B4/B9)
* 3 VDD → Arduino 3V3
* 4 VDD ↑
* 5 INT_N → Arduino D12
* 6 SCL → Arduino A5
* 7 SDA → Arduino A4
* 8 GND → Arduino GND, Type-C GND (A1/A12/B1/B12)
* 9 GND ↑
* 10 CC1 → Type-C CC1 (A5)
* 11 CC1 ↑
* 12 VCONN output - leave open
* 13 VCONN output - leave open
* 14 CC2 - See pin 1 ↑

You can get away without decoupling and with only connecting one of the duplicated pins. The exposed pad is NC.

#### Reclaimer Labs Board
Alternatively, you can also use the [Relaimer Labs](https://www.tindie.com/products/reclaimerlabs/usb-c-power-delivery-phy-breakout-board/) board.

**NOTE: SBU1 and SBU2 aren't connected on the USB-C connector**

* 1 VBUS → Arduino D4 (**not VBUS_A**)
* 2 VPU → Arduino 3V3
* 3 VDD → Arduino 3V3
* 4 GND → Arduino GND
* 5 INT_N → Arduino D12
* 6 SCL → Arduino A5
* 7 SDA → Arduino A4
* 8 GND → Arduino GND

# Cabling 
Not all USB-C Cables are alike. For this you will need a SuperSpeed or Thunderbolt cable. These may or may not be marked as such. (White Apple cables don't work for example). If you get no output from a vdm command but output otherwise looks ok: try another cable.

# Interface

By default the code will perform trivial (mostly hardcoded) USB-PD negotiation, then request to mux the debug UART over SBU1/SBU2. You can send custom VDMs using the Arduino serial terminal at 500000 baud.

Sample DFP mode session:

```
VBUS OFF
Device ID: 0x91
Init
STATUS0: 0x0
VBUS OFF
Disconnected
S: DISCONNECTED
Poll: cc1=0 cc2=0
Poll: cc1=2 cc2=0
Connected: cc1=2 cc2=0
Polarity: CC1 (normal)
VBUS ON
S: DFP_VBUS_ON
IRQ: VBUSOK (VBUS=ON)
>SOURCE_CAP
S: DFP_CONNECTED
IRQ: HARDRESET
VBUS OFF
Disconnected
S: DISCONNECTED
Poll: cc1=2 cc2=0
Connected: cc1=2 cc2=0
Polarity: CC1 (normal)
VBUS ON
S: DFP_VBUS_ON
IRQ: VBUSOK (VBUS=ON)
>SOURCE_CAP
S: DFP_CONNECTED
<REQUEST: 12000000
>ACCEPT
>PS_RDY
S: IDLE
<GET_SINK_CAP
>SINK_CAP
S: READY
>VDM SET ACTION serial -> SBU1/2
S: IDLE
<VDM RX SOP"DEBUG (5) [504F] 5AC8052 44740000 306 0 0
```

Sample UFP mode session:

```
VBUS OFF
Device ID: 0x91
Init
STATUS0: 0x0
Disconnected
IRQ: VBUSOK (VBUS=OFF)
S: DISCONNECTED
S: DISCONNECTED
IRQ: VBUSOK (VBUS=ON)
Connected: cc1=7 cc2=0
Polarity: CC1 (normal)
S: CONNECTED
<SOURCE_CAP: 3F01912C
>REQUEST
<ACCEPT
<PS_RDY
<SOURCE_CAP: 3E019096
>REQUEST
<ACCEPT
<PS_RDY
<GET_SINK_CAP
>SINK_CAP
<VDM DISCOVER_IDENTITY
>VDM DISCOVER_IDENTITY
S: READY
>VDM SET ACTION serial -> SBU1/2
S: IDLE
  input: "5AC8010\n"
>VDM 5AC8010
<VDM RX SOP'DEBUG (7) [724F] 5AC8050 46060606 2060301 3060106 1050303 8030809 1030000
  input: "5AC8011,306\n"
>VDM 5AC8011 306
<VDM RX SOP'DEBUG (3) [344F] 5AC8051 187020C 800C0000
  input: "5AC8012,1840306\n"
>VDM 5AC8012 1840306
<VDM RX SOP'DEBUG (5) [504F] 5AC8052 44740000 306 0 0
```
