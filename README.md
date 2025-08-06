# USB-C Power Delivery with FUSB302B

A comprehensive library of functions to facilitate USB-C power negotiation using the Serial protocol & the 
specified packet parameters outlined in the MicroChip Datasheet

## Features

- **Complete PD Protocol Implementation**: Full USB-PD 2.0 and 3.0 specification support
- **Power Negotiation**: Automatic voltage and current negotiation with connected devices  
- **Device Recognition**: Automatic detection of device types (charger, monitor, tablet, laptop)
- **Vendor Defined Messages**: Support for VDM discovery identity and SVID requests
- **Extended Source Capabilities**: PD 3.0 extended message support
- **Multi-Voltage Support**: Handles 5V, 9V, 12V, 15V, 20V power profiles
- **Real-time Monitoring**: Interrupt-driven attach/detach detection

## Hardware Requirements

- FUSB302B USB-C Power Delivery controller
- Arduino-compatible microcontroller (tested with Pi Pico)
- USB-C connector with CC1/CC2 pins connected to FUSB302B
- I2C communication interface

## Protocol Support

- USB Power Delivery 2.0 and 3.0 specifications
- Source Capabilities discovery and negotiation  
- Sink Capabilities advertisement
- Power role swap and data role swap
- Cable identity discovery
- Extended source capabilities (PD 3.0)

## Files

- **PD_Negotiation.cpp**: Complete power delivery negotiation implementation with device recognition
- **Protocol_Engine.cpp**: Core protocol engine for basic packet handling and communication

## Device Recognition

The library includes a device database that automatically identifies connected devices based on their VID/PID:

- Chargers (Xiaomi, etc.)
- Tablets (iPad, Samsung, OnePlus)
- Laptops (Dell XPS)
- Monitors and other USB-C devices

## Technicals

Built on the MicroChip FUSB302B datasheet specifications with serial protocol communication over I2C. Implements the complete USB-PD state machine including:

- CC line orientation detection
- Power contract establishment  
- Message retry and timeout handling
- CRC validation and Good CRC responses
- Interrupt-driven event processing
