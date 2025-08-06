#include <Arduino.h>
#include <Wire.h>
#include "FUSB302B.h"

// Implementation file - constants now in FUSB302B.h

// Global variables
uint8_t tx_buf[80];
uint8_t rx_buf[80];
uint8_t temp_buf[80];

// Power delivery state variables
int volt_options[5] = {-1, -1, -1, -1, -1};
int amp_options[5] = {-1, -1, -1, -1, -1};
int options_pos[5] = {-1, -1, -1, -1, -1};
int spec_revs[4] = {2, 0, 0, 0}; // revision major, revision minor, version major, version minor
int msg_id = 2;

// Function declarations
void sendPacket(uint8_t num_data_objects, uint8_t message_id, uint8_t port_power_role, 
                uint8_t spec_rev, uint8_t port_data_role, uint8_t message_type, 
                uint8_t *data_objects);

/**
 * Set a register value on the FUSB302B
 */
void setReg(uint8_t addr, uint8_t value) {
    Wire.beginTransmission(PD_ADDR);
    Wire.write(addr);
    Wire.write(value);
    Wire.endTransmission(true);
}

/**
 * Read a register value from the FUSB302B
 */
uint8_t getReg(uint8_t addr) {
    Wire.beginTransmission(PD_ADDR);
    Wire.write(addr);
    Wire.endTransmission(false);
    Wire.requestFrom((int)PD_ADDR, 1, true);
    return Wire.read();
}

/**
 * Send data bytes to FUSB302B FIFO
 */
void sendBytes(uint8_t *data, uint16_t length) {
    if (length > 0) {
        Wire.beginTransmission(PD_ADDR);
        Wire.write(REG_FIFOS);
        for (uint16_t i = 0; i < length; i++) {
            Wire.write(data[i]);
        }
        Wire.endTransmission(true);
    }
}

/**
 * Receive data bytes from FUSB302B FIFO
 */
void receiveBytes(uint8_t *data, uint16_t length) {
    if (length > 0) {
        Wire.beginTransmission(PD_ADDR);
        Wire.write(REG_FIFOS);
        Wire.endTransmission(false);
        Wire.requestFrom((int)PD_ADDR, (int)length, true);
        for (uint16_t i = 0; i < length; i++) {
            data[i] = Wire.read();
        }
    }
}

/**
 * Receive and parse a PD packet (debug version with full logging)
 */
bool receivePacket() {
    uint8_t num_data_objects;
    uint8_t message_id;
    uint8_t port_power_role;
    uint8_t spec_rev;
    uint8_t port_data_role;
    uint8_t message_type;
    
    receiveBytes(rx_buf, 1);
    if (rx_buf[0] != 0xE0) {
        Serial1.println("FAIL - receive packet");
        return false;
    }
    
    receiveBytes(rx_buf, 2);
    num_data_objects = ((rx_buf[1] & 0x70) >> 4);
    message_id = ((rx_buf[1] & 0x0E) >> 1);
    port_power_role = (rx_buf[1] & 0x01);
    spec_rev = ((rx_buf[0] & 0xC0) >> 6);
    port_data_role = ((rx_buf[0] & 0x10) >> 5);
    message_type = (rx_buf[0] & 0x0F);
    
    if (num_data_objects) {
        Serial1.println("Data message received");
        spec_revs[0] = spec_rev + 1;
    } else if (message_type == MSG_TYPE_GOODCRC) {
        Serial1.println("GoodCRC message received");
    } else {
        Serial1.println("Control message received");
    }
    
    Serial1.println("Received SOP Packet");
    Serial1.print("Header: 0x");
    Serial1.println(*(int *)rx_buf, HEX);
    Serial1.print("Number of data objects = ");
    Serial1.println(num_data_objects, DEC);
    Serial1.print("Message ID = ");
    Serial1.println(message_id, DEC);
    Serial1.print("Port power role = ");
    Serial1.println(port_power_role, DEC);
    Serial1.print("Spec revision = ");
    Serial1.println(spec_rev, DEC);
    Serial1.print("Port data role = ");
    Serial1.println(port_data_role, DEC);
    Serial1.print("Message type = ");
    Serial1.println(message_type, DEC);
    
    receiveBytes(rx_buf, (num_data_objects * 4));
    // Parse each data object (32 bits each)
    for (uint8_t i = 0; i < num_data_objects; i++) {
        Serial1.print("Object: 0x");
        uint32_t byte1 = rx_buf[0 + i * 4];
        uint32_t byte2 = rx_buf[1 + i * 4] << 8;
        uint32_t byte3 = rx_buf[2 + i * 4] << 16;
        uint32_t byte4 = rx_buf[3 + i * 4] << 24;
        Serial1.println(byte1 | byte2 | byte3 | byte4, HEX);
    }
    
    // Read CRC-32
    receiveBytes(rx_buf, 4);
    Serial1.print("CRC-32: 0x");
    Serial1.println(*(long *)rx_buf, HEX);
    Serial1.println();
    
    return true;
}

/**
 * Read all FUSB302B registers for debugging
 */
void readAllRegs() {
    Wire.beginTransmission(PD_ADDR);
    Wire.write(0x01);
    Wire.endTransmission(false);
    Wire.requestFrom((int)PD_ADDR, 16, 1);
    
    for (int i = 1; i <= 16; i++) {
        uint8_t c = Wire.read();
        Serial1.print("Address: 0x");
        Serial1.print(i, HEX);
        Serial1.print(", Value: 0x");
        Serial1.println(c, HEX);
    }
    
    Wire.beginTransmission(PD_ADDR);
    Wire.write(0x3C);
    Wire.endTransmission(false);
    Wire.requestFrom((int)PD_ADDR, 7, true);
    
    for (int i = 0x3C; i <= 0x42; i++) {
        uint8_t c = Wire.read();
        Serial1.print("Address: 0x");
        Serial1.print(i, HEX);
        Serial1.print(", Value: 0x");
        Serial1.println(c, HEX);
    }
    Serial1.println();
}

/**
 * Send a USB-PD packet (debug version with logging)
 */
void sendPacket(uint8_t num_data_objects, uint8_t message_id, uint8_t port_power_role, 
                uint8_t spec_rev, uint8_t port_data_role, uint8_t message_type, 
                uint8_t *data_objects) {
    
    uint8_t temp;
    
    // SOP sequence - see USB-PD 2.0 page 108
    tx_buf[0] = SOP_SEQUENCE_0;
    tx_buf[1] = SOP_SEQUENCE_1;
    tx_buf[2] = SOP_SEQUENCE_2;
    tx_buf[3] = SOP_SEQUENCE_3;
    
    // Packet length
    tx_buf[4] = (0x80 | (2 + (4 * (num_data_objects & 0x1F))));
    
    // Header byte 1
    tx_buf[5] = (message_type & 0x0F);
    tx_buf[5] |= ((port_data_role & 0x01) << 5);
    tx_buf[5] |= ((spec_rev & 0x03) << 6);
    
    // Header byte 2
    tx_buf[6] = (port_power_role & 0x01);
    tx_buf[6] |= ((message_id & 0x07) << 1);
    tx_buf[6] |= ((num_data_objects & 0x07) << 4);
    
    Serial1.print("Sending Header: 0x");
    Serial1.println((tx_buf[6] << 8) | (tx_buf[5]), HEX);
    Serial1.println();
    
    // Data objects
    temp = 7;
    for (uint8_t i = 0; i < num_data_objects; i++) {
        tx_buf[temp] = data_objects[(4 * i)];
        tx_buf[temp + 1] = data_objects[(4 * i) + 1];
        tx_buf[temp + 2] = data_objects[(4 * i) + 2];
        tx_buf[temp + 3] = data_objects[(4 * i) + 3];
        
        Serial1.print("Data object being sent out: 0x");
        Serial1.println((tx_buf[temp + 3] << 24) | (tx_buf[temp + 2] << 16) | 
                       (tx_buf[temp + 1] << 8) | (tx_buf[temp]), HEX);
        temp += 4;
    }
    
    Serial1.print("Message ID: ");
    Serial1.println(message_id, DEC);
    
    // Packet termination
    tx_buf[temp] = CRC_PLACEHOLDER;
    tx_buf[temp + 1] = EOP_SEQUENCE;
    tx_buf[temp + 2] = TXOFF_SEQUENCE;
    
    // Send packet
    uint8_t control_reg = getReg(REG_CONTROL0);
    sendBytes(tx_buf, (10 + (4 * (num_data_objects & 0x1F))));
    setReg(REG_CONTROL0, (control_reg | 0x01)); // Start transmission
    msg_id++;
}
