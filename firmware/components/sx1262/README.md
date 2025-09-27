# SX1262 Radio Driver

This component implements the SX1262 LoRa radio driver for Parallel Meshtastic. It provides SPI communication, IRQ handling, and basic TX/RX functions.

## Hardware Pinout
Refer to the main reference document for pin mapping.

## Initialization Steps
- Configure SPI bus
- Set up GPIOs for CS, RESET, DIO1, BUSY, DIO2
- Verify radio communication

## TODO
- Implement full register set
- Add interrupt handling
- Integrate with link layer
