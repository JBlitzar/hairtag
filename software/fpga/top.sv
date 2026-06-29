module top (
    // Onboard FTDI USB-UART bridge
    input  wire usb_rx,     // FTDI TXD  : host -> device data
    output wire usb_tx,     // FTDI RXD  : device -> host data
    input  wire usb_rtsn,   // FTDI RTS# : drives CHIP_EN (reset)
    input  wire usb_dtrn,   // FTDI DTR# : drives GPIO9 (boot select)

    // Flashing header to hairtag (signal order: TXD RXD EN DTR)
    output wire flash_txd,  // -> ESP U0RXD
    input  wire flash_rxd,  // <- ESP U0TXD
    output wire flash_en,   // -> ESP CHIP_EN
    output wire flash_dtr   // -> ESP GPIO9 (boot)
);

    assign flash_txd = usb_rx;
    assign usb_tx    = flash_rxd;
    assign flash_en  = usb_rtsn;
    assign flash_dtr = usb_dtrn;

endmodule
