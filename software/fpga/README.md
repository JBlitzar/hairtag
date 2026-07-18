# Icepi Zero FPGA flasher

This is how I'm going to personally flash since I want to avoid spending 16 dollars. 

You only need this if you happen to have an Icepi Zero lying around. For
everyone else, the CP2102 dongle in the main
[wiring diagram](/README.md#wiring-diagram-for-flashing) is simpler.

## How it works

[`top.sv`](top.sv) is a pure combinational pass-through. esptool talks to the
Icepi's onboard FTDI exactly as it would to a CP2102; the FPGA just forwards
the data and modem-control lines (RTS->EN, DTR->boot) to four GPIOs:

| FTDI signal | FPGA pass-through | Hairtag pin |
|-------------|-------------------|-------------|
| TXD (`usb_rx`)   | `flash_txd` | TXD (ESP U0RXD) |
| RXD (`usb_tx`)   | `flash_rxd` | RXD (ESP U0TXD) |
| RTS# (`usb_rtsn`)| `flash_en`  | EN (CHIP_EN)    |
| DTR# (`usb_dtrn`)| `flash_dtr` | DTR (GPIO9 boot)|

## Wiring

The four signal GPIOs sit on the odd-numbered outer row, so VCC/GND/signals
are all reachable along one side of the 40-pin header:

| Hairtag pin | Icepi 40-pin header | ECP5 site |
|-------------|---------------------|-----------|
| VCC | pin 1 (3V3) | — (rail) |
| GND | pin 9 (GND) | — (rail) |
| TXD | pin 3  | T2 |
| RXD | pin 5  | R2 |
| EN  | pin 7  | R1 |
| DTR | pin 11 | R3 |


![](https://i.sstatic.net/yHddo.png)


## Build and load

Requires the open-source ECP5 toolchain (`yosys`, `nextpnr-ecp5`, `ecppack`,
`openFPGALoader`).

```bash
make debug    # load to SRAM (lost on power cycle) - good for flashing sessions
make install  # write to flash (persists)
make clean
```

Once the bitstream is loaded, flash the hairtag the normal way (`pio run -t
upload`, `keywriter.py`, etc.) pointing esptool at the Icepi's serial port.
