# UART-eTPU
This UART eTPU driver includes many enhancements beyond the original NXP UART eTPU drivers, including:
- size-configurable receive/transmit FIFOs (circular buffers) on the eTPU.
- programmable thresholds for FIFO full/empty host interrupts.
- even/odd/no parity options, 1-23 bit data word size, programmable stop length.
- optional and independent hardware flow control support (CTS/RTS).
- RS-485 mode with drive enable output and programmable turn-off delay.
- buffer overrun detect, per-word framing and parity error detect/report.

This software is built and simulated/tested by the following tools:
- ETEC C Compiler for eTPU/eTPU2/eTPU2+, version 2.62E, ASH WARE Inc.
- System Development Tool, version 2.72E, ASH WARE Inc.

Possible future enhancements include:
- software flow control
- better DMA support
- LSB or MSB first select
- initial receive idle detect

Use of or collaboration on this project is welcomed. For any questions please contact:

ASH WARE Inc. John Diener john.diener@ashware.com
