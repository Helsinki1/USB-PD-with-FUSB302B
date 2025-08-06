#include <Arduino.h>
#include <Wire.h>
#include "FUSB302B.h"

// Implementation file - constants now in FUSB302B.h

// Global variables
uint8_t tx_buf[80];
uint8_t rx_buf[80];
uint8_t temp_buf[80];

// Device library for VID/PID recognition
uint16_t dev_library[10][3] = { 
    // VID, PID, device type (0:charger, 1:monitor, 2:tablet, 3:laptop/computer)
    {0x2B01, 0xF663, 0}, // Xiaomi charger
    {0x05AC, 0x7109, 2}, // iPad
    {0x413C, 0xB057, 3}, // Dell XPS laptop
    {0x04E8, 0x0, 2},    // Samsung Fold
    {0x05C6, 0x0, 2},    // OnePlus Phone (Qualcomm processor)
    {0, 0, 0},
    {0, 0, 0}
};

// Power delivery state variables
int dev_type = 0; // 0:charger, 1:monitor, 2:tablet, 3:laptop/computer
int volt_options[5] = {-1, -1, -1, -1, -1};
int amp_options[5] = {-1, -1, -1, -1, -1};
int options_pos[5] = {-1, -1, -1, -1, -1};
int spec_revs[4] = {2, 0, 0, 0}; // revision major, revision minor, version major, version minor
int msg_id = 0;
bool int_flag = false;
bool attached = false;
bool new_attach = false;
int meas_cc1 = 0; // BC level after measuring CC1
int meas_cc2 = 0; // BC level after measuring CC2
int cc_line = 1;
int vconn_line = 2;

// Function declarations
void sendPacket(bool extended, uint8_t num_data_objects, uint8_t message_id, 
                uint8_t port_power_role, uint8_t spec_rev, uint8_t port_data_role, 
                uint8_t message_type, uint8_t *data_objects);

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
 * Receive and parse a PD packet
 */
bool receivePacket() {
    uint8_t num_data_objects;
    uint8_t message_id;
    uint8_t port_power_role;
    uint8_t spec_rev;
    uint8_t port_data_role;
    uint8_t message_type;
    
    receiveBytes(rx_buf, 1);
    if (getReg(REG_STATUS1) & 0x20) {
        Serial1.println("RX empty - receive packet");
        return false;
    }
    
    receiveBytes(rx_buf, 2);
    num_data_objects = ((rx_buf[1] & 0x70) >> 4);
    message_id = ((rx_buf[1] & 0x0E) >> 1);
    port_power_role = (rx_buf[1] & 0x01);
    spec_rev = ((rx_buf[0] & 0xC0) >> 6);
    port_data_role = ((rx_buf[0] & 0x10) >> 5);
    
    // Fixed assignment bug (was = instead of ==)
    if (spec_revs[0] == 3) {
        message_type = (rx_buf[0] & 0x1F);
    } else {
        message_type = (rx_buf[0] & 0x0F);
    }
    
    if (num_data_objects) {
        Serial1.println("Data message received");
    } else if (message_type == MSG_TYPE_GOODCRC) {
        Serial1.println("GoodCRC message received");
        receiveBytes(rx_buf, 4); // CRC-32
        return true;
    } else {
        Serial1.print("Control message received, type: ");
        Serial1.println(message_type, BIN);
    }
    
    receiveBytes(rx_buf, (num_data_objects * 4));
    // Parse data objects if needed
    for (uint8_t i = 0; i < num_data_objects; i++) {
        // Data object parsing logic here
    }
    
    // Read CRC-32
    receiveBytes(rx_buf, 4);
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
 * Send a USB-PD packet
 */
void sendPacket(bool extended, uint8_t num_data_objects, uint8_t message_id, 
                uint8_t port_power_role, uint8_t spec_rev, uint8_t port_data_role, 
                uint8_t message_type, uint8_t *data_objects) {
    
    uint8_t temp;
    
    // SOP sequence - see USB-PD 2.0 page 108
    tx_buf[0] = SOP_SEQUENCE_0;
    tx_buf[1] = SOP_SEQUENCE_1;
    tx_buf[2] = SOP_SEQUENCE_2;
    tx_buf[3] = SOP_SEQUENCE_3;
    
    // Packet length
    tx_buf[4] = (0x80 | (2 + (4 * (num_data_objects & 0x07))));
    
    // Header byte 1
    tx_buf[5] = (message_type & 0x1F); // Extended message support
    tx_buf[5] |= ((port_data_role & 0x01) << 5);
    tx_buf[5] |= ((spec_rev & 0x03) << 6);
    
    // Header byte 2
    tx_buf[6] = (port_power_role & 0x01);
    tx_buf[6] |= ((message_id & 0x07) << 1);
    tx_buf[6] |= ((num_data_objects & 0x07) << 4);
    tx_buf[6] |= (extended << 7);
    
    // Data objects
    temp = 7;
    for (uint8_t i = 0; i < num_data_objects; i++) {
        tx_buf[temp] = data_objects[(4 * i)];
        tx_buf[temp + 1] = data_objects[(4 * i) + 1];
        tx_buf[temp + 2] = data_objects[(4 * i) + 2];
        tx_buf[temp + 3] = data_objects[(4 * i) + 3];
        temp += 4;
    }
    
    // Packet termination
    tx_buf[temp] = CRC_PLACEHOLDER;
    tx_buf[temp + 1] = EOP_SEQUENCE;
    tx_buf[temp + 2] = TXOFF_SEQUENCE;
    
    // Send packet
    uint8_t control_reg = getReg(REG_CONTROL0);
    sendBytes(tx_buf, (10 + (4 * (num_data_objects & 0x07))));
    setReg(REG_CONTROL0, (control_reg | 0x01)); // Start transmission
    msg_id++;
}

// Higher level abstractions

/**
 * Read RX FIFO for debugging purposes
 */
void read_rx_fifo() {
    int i = 1;
    while (!(getReg(REG_STATUS1) & 0x20)) {
        Serial1.print("Byte number ");
        Serial1.print(i);
        Serial1.print(": 0x");
        receiveBytes(rx_buf, 1);
        Serial1.println(rx_buf[0], HEX);
        i++;
    }
}

/**
 * Read Power Data Objects from source capabilities message
 */
bool read_pdo() {
    uint8_t num_data_objects;
    uint8_t message_type;
    uint8_t spec_rev;
    
    // Clear previous options
    for (int i = 0; i < 5; i++) {
        volt_options[i] = -1;
        amp_options[i] = -1;
        options_pos[i] = -1;
    }
    
    receiveBytes(rx_buf, 1);
    if (rx_buf[0] != 0xE0) {
        Serial1.println("No response received - read PDO");
        return false;
    }
    
    receiveBytes(rx_buf, 2);
    num_data_objects = ((rx_buf[1] & 0x70) >> 4);
    spec_rev = ((rx_buf[0] & 0xC0) >> 6);
    message_type = (rx_buf[0] & 0x0F);
    
    if ((message_type == MSG_TYPE_GOODCRC) && (num_data_objects > 0)) {
        Serial1.println("Source capabilities message received");
    } else {
        Serial1.println("Message received, but not source capabilities");
        return false;
    }
    
    receiveBytes(rx_buf, (num_data_objects * 4));
    int index = 0;
    
    for (uint8_t i = 0; i < num_data_objects; i++) {
        uint32_t byte1 = rx_buf[0 + i * 4];
        uint32_t byte2 = rx_buf[1 + i * 4] << 8;
        uint32_t byte3 = rx_buf[2 + i * 4] << 16;
        uint32_t byte4 = rx_buf[3 + i * 4] << 24;
        int32_t pdo = byte1 | byte2 | byte3 | byte4;
        
        switch (pdo >> 30) {
            case 0x0: // Fixed supply
                volt_options[index] = ((pdo >> 10) & 0x3FF) / 20; // Convert to 1V units
                amp_options[index] = (pdo & 0x3FF); // Keep in 10mA units
                options_pos[index] = i + 1; // Position 0001 is always safe 5V
                index++;
                break;
            case 0x1: // Battery supply
                Serial1.print("Battery supply PDO: ");
                Serial1.println(pdo, HEX);
                break;
            case 0x2: // Variable supply
                Serial1.print("Variable supply PDO: ");
                Serial1.println(pdo, HEX);
                break;
            case 0x3: // Augmented PDO
                Serial1.print("Augmented PDO: ");
                Serial1.println(pdo, HEX);
                break;
        }
    }
    
    // Read CRC-32
    receiveBytes(rx_buf, 4);
    return true;
}

/**
 * Get request outcome (accept/reject)
 */
bool get_req_outcome() {
    uint8_t message_type;
    uint8_t num_data_objects;
    
    receiveBytes(rx_buf, 1);
    if (getReg(REG_STATUS1) & 0x20) {
        Serial1.println("No response received - get request outcome");
        return false;
    }
    
    receiveBytes(rx_buf, 2);
    message_type = (rx_buf[0] & 0x0F);
    num_data_objects = ((rx_buf[1] & 0x70) >> 4);
    receiveBytes(rx_buf, 4); // CRC-32
    
    if (message_type == MSG_TYPE_ACCEPT) {
        Serial1.println("Request accepted");
        return true;
    } else if (message_type == MSG_TYPE_PS_READY) {
        Serial1.println("Power supply ready");
        return true;
    } else {
        Serial1.print("Error, message type: ");
        Serial1.println(message_type, DEC);
        Serial1.print("Number of data objects: ");
        Serial1.println(num_data_objects, DEC);
        return false;
    }
}

/**
 * Read Revision Message Data Object
 */
uint32_t read_rmdo() {
    uint8_t num_data_objects;
    uint8_t spec_rev;
    uint8_t message_type;
    
    receiveBytes(rx_buf, 1);
    if (rx_buf[0] != 0xE0) {
        Serial1.println("No response received - read RMDO");
        return 0;
    }
    
    receiveBytes(rx_buf, 2);
    num_data_objects = ((rx_buf[1] & 0x70) >> 4);
    spec_rev = ((rx_buf[0] & 0xC0) >> 6);
    message_type = (rx_buf[0] & 0x0F);
    
    if ((message_type == 0xC) && (num_data_objects == 1)) {
        Serial1.println("RMDO message received");
    } else {
        Serial1.print("Expected RMDO, incorrect packet type received. Type: ");
        Serial1.println(message_type, BIN);
        Serial1.print("Number of data objects: ");
        Serial1.println(num_data_objects);
        return 0;
    }
    
    receiveBytes(rx_buf, (num_data_objects * 4));
    Serial1.print("RMDO: 0x");
    
    uint32_t byte1 = rx_buf[0];
    uint32_t byte2 = rx_buf[1] << 8;
    uint32_t byte3 = rx_buf[2] << 16;
    uint32_t byte4 = rx_buf[3] << 24;
    uint32_t rmdo = byte1 | byte2 | byte3 | byte4;
    
    Serial1.println(rmdo, HEX);
    
    // Read CRC-32
    receiveBytes(rx_buf, 4);
    Serial1.println();
    
    return rmdo;
}

/**
 * Read extended source capabilities message
 */
bool read_ext_src_cap() {
    uint8_t num_data_objects;
    uint16_t ext_data_size;
    uint8_t message_type;
    bool extended_msg;
    uint16_t VID, PID;
    
    receiveBytes(rx_buf, 1); // Preamble
    if (getReg(REG_STATUS1) & 0x20) {
        Serial1.println("Empty RX FIFO - read extended source cap");
        return false;
    }
    
    receiveBytes(rx_buf, 2); // Header
    num_data_objects = ((rx_buf[1] & 0x70) >> 4);
    
    if (spec_revs[0] == 3) {
        message_type = (rx_buf[0] & 0x1F);
    } else {
        message_type = (rx_buf[0] & 0x0F);
    }
    
    extended_msg = (rx_buf[1] >> 7);
    
    if (extended_msg) {
        Serial1.println("Extended message received");
        receiveBytes(rx_buf, 2); // Extended header
        ext_data_size = (((rx_buf[1] & 0x1) << 8) | rx_buf[0]);
        
        if ((ext_data_size >= 24) && (message_type == 1)) {
            receiveBytes(rx_buf, ext_data_size); // Extended source cap content
            VID = (rx_buf[1] << 8) | rx_buf[0];
            PID = (rx_buf[3] << 8) | rx_buf[2];
            
            Serial1.print("Device VID: ");
            Serial1.println(VID, HEX);
            Serial1.print("Device PID: ");
            Serial1.println(PID, HEX);
            
            // Check device library for recognition
            for (int i = 0; i < 10; i++) {
                if ((dev_library[i][0] == VID) && (dev_library[i][1] == PID)) {
                    dev_type = dev_library[i][2];
                    break;
                }
            }
            
            setReg(REG_CONTROL1, 0x04); // Flush RX
            return true;
        } else {
            Serial1.print("Data size: ");
            Serial1.println(ext_data_size, DEC);
            Serial1.print("Message type: ");
            Serial1.println(message_type, BIN);
            setReg(REG_CONTROL1, 0x04); // Flush RX
            return false;
        }
    } else {
        if ((message_type == 16) && (num_data_objects == 0)) {
            Serial1.println("Extended source cap not supported");
            setReg(REG_CONTROL1, 0x04); // Flush RX
            return false;
        }
        Serial1.println("Wrong type of message received - read ext source cap");
        setReg(REG_CONTROL1, 0x04); // Flush RX
        return false;
    }
    return false;
}

/**
 * Read discover identity response
 */
bool read_dis_idt_response() {
    uint8_t num_data_objects;
    uint8_t message_type;
    uint8_t command;
    uint8_t cmd_type;
    uint16_t VID, PID;
    
    receiveBytes(rx_buf, 1); // Preamble
    if (getReg(REG_STATUS1) & 0x20) {
        Serial1.println("Empty RX FIFO - read discover identity response");
        return false;
    }
    
    receiveBytes(rx_buf, 2); // Header
    num_data_objects = ((rx_buf[1] & 0x70) >> 4);
    message_type = (rx_buf[0] & 0x1F);
    
    if (message_type == MSG_TYPE_VDM) {
        Serial1.println("VDM received");
        receiveBytes(rx_buf, 2); // VDM header
        command = rx_buf[0] & 0x1F; // Discover identity
        cmd_type = (rx_buf[0] & 0xC0) >> 6; // REQ/ACK/NACK/BUSY
        
        if ((command == 1) && (cmd_type == 1)) {
            receiveBytes(rx_buf, num_data_objects * 4); // VDM content
            VID = (rx_buf[1] << 8) | rx_buf[0];
            PID = (rx_buf[11] << 8) | rx_buf[10];
            
            Serial1.print("Device VID: ");
            Serial1.println(VID, HEX);
            Serial1.print("Device PID: ");
            Serial1.println(PID, HEX);
        } else {
            if ((command == 1) && (message_type == 0xF)) {
                Serial1.println("VDM request NACK");
                return false;
            }
            Serial1.print("Command: ");
            Serial1.println(command, DEC);
            Serial1.print("Message type: ");
            Serial1.println(message_type, BIN);
            return false;
        }
    } else {
        Serial1.println("Wrong type of message received - read discover identity response");
        return false;
    }
    
    receiveBytes(rx_buf, 4); // CRC-32
    return true;
}

/**
 * Get source capabilities
 */
void get_src_cap() {
    Serial1.println("Fetching source capabilities info...");
    sendPacket(false, 0, msg_id, 0, spec_revs[0] - 1, 0, MSG_TYPE_GET_SOURCE_CAP, NULL);
    
    while (getReg(REG_STATUS1) & 0x20) {} // Wait while RX empty
    receivePacket();
    
    while (getReg(REG_STATUS1) & 0x20) {} // Wait while RX empty  
    read_pdo();
}

/**
 * Select source capability (request specific voltage/current)
 */
bool sel_src_cap(int volts, int amps) {
    uint8_t temp_buf[80] = {};
    bool possible_v = false;
    bool possible_a = false;
    int idx = 0;
    
    // Check if requested voltage and current are available
    for (int i = 0; i < 5; i++) {
        if (volt_options[i] == volts) {
            possible_v = true;
            if (amp_options[i] >= amps) {
                possible_a = true;
                idx = i;
                break;
            }
        }
    }
    
    if (possible_v && possible_a) {
        uint32_t request_msg = (options_pos[idx] << 28) | 
                              ((amps * 100) << 10) | 
                              (amp_options[idx]);
        
        temp_buf[0] = request_msg & 0xFF;
        temp_buf[1] = (request_msg >> 8) & 0xFF;
        temp_buf[2] = (request_msg >> 16) & 0xFF;
        temp_buf[3] = (request_msg >> 24) & 0xFF;
        
        sendPacket(false, 1, msg_id, 0, spec_revs[0] - 1, 0, 0x2, temp_buf);
        Serial1.println("Voltage and current requested from source");
        
        unsigned long time = millis();
        while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 500))) {} // Wait for GoodCRC
        receivePacket();
        
        while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 500))) {} // Wait for Accept
        get_req_outcome();
        
        while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 500))) {} // Wait for PS_Ready
        if (get_req_outcome()) {
            return true;
        }
    } else if (!possible_v) {
        Serial1.println("Voltage not available");
        return false;
    } else {
        Serial1.println("Current request too high for selected voltage");
        return false;
    }
    return false;
}

/**
 * Send sink capabilities
 */
void send_snk_cap(int volts, int amps) {
    if (volts == 5) {
        // Single 5V PDO
        uint32_t sink_cap_msg = (0 << 29) | (1 << 26) | (0 << 25) | ((5 * 20) << 10) | (amps * 100);
        temp_buf[0] = sink_cap_msg & 0xFF;
        temp_buf[1] = (sink_cap_msg >> 8) & 0xFF;
        temp_buf[2] = (sink_cap_msg >> 16) & 0xFF;
        temp_buf[3] = (sink_cap_msg >> 24) & 0xFF;
        
        sendPacket(false, 1, msg_id, 0, spec_revs[0] - 1, 0, 0x4, temp_buf);
        Serial1.println("Sink capabilities sent (5V only)");
    } else if (volts > 5) {
        // Dual PDO: 5V + higher voltage
        uint32_t sink_cap_msg_p1 = (0 << 29) | (1 << 28) | (1 << 26) | (0 << 25) | ((5 * 20) << 10) | (3 * 100);
        uint32_t sink_cap_msg_p2 = ((volts * 20) << 10) | (amps * 100);
        
        temp_buf[0] = sink_cap_msg_p1 & 0xFF;
        temp_buf[1] = (sink_cap_msg_p1 >> 8) & 0xFF;
        temp_buf[2] = (sink_cap_msg_p1 >> 16) & 0xFF;
        temp_buf[3] = (sink_cap_msg_p1 >> 24) & 0xFF;
        temp_buf[4] = sink_cap_msg_p2 & 0xFF;
        temp_buf[5] = (sink_cap_msg_p2 >> 8) & 0xFF;
        temp_buf[6] = (sink_cap_msg_p2 >> 16) & 0xFF;
        temp_buf[7] = (sink_cap_msg_p2 >> 24) & 0xFF;
        
        sendPacket(false, 2, msg_id, 0, spec_revs[0] - 1, 0, 0x4, temp_buf);
        Serial1.println("Sink capabilities sent (higher capacity than 5V)");
    }
}

/**
 * Send discover identity request
 */
void send_dis_idt_request() {
    uint8_t temp_buf[80];
    uint16_t dev_PD_SID = 0xFF00; // SVID field must be set to this for discover identity command
    
    uint32_t vdm_header = (dev_PD_SID << 16) | (1 << 15) | (0 << 13) | (0 << 6) | (1); // REQ for Discover Identity
    temp_buf[0] = vdm_header & 0xFF;
    temp_buf[1] = (vdm_header >> 8) & 0xFF;
    temp_buf[2] = (vdm_header >> 16) & 0xFF;
    temp_buf[3] = (vdm_header >> 24) & 0xFF;
    
    sendPacket(false, 1, msg_id, 0, spec_revs[0] - 1, 0, MSG_TYPE_VDM, temp_buf);
    Serial1.println("Fetching discovery identity info...");
}

/**
 * Send discover identity response
 */
void send_dis_idt_response() {
    uint16_t dev_VID = 0x0483; // VID from Intel Corp
    uint16_t dev_PID = 0x1307; // PID from Intel Corp  
    uint16_t dev_PD_SID = 0xFF00; // SVID field for discover identity command
    
    uint32_t vdm_header = (dev_PD_SID << 16) | (1 << 15) | (1 << 13) | (1 << 6) | (1);
    uint32_t id_header = (1 << 30) | (2 << 27) | (2 << 21) | (dev_VID);
    uint32_t cert_stat = 0; // Empty XID
    uint32_t product_vdo = (dev_PID << 16) | (0); // Empty BCD device code
    uint32_t UFP_type_vdo = (3 << 29) | (4 << 24) | (1); // USB 3.2 capable with Gen 1 SuperSpeed
    
    temp_buf[0] = vdm_header & 0xFF;
    temp_buf[1] = (vdm_header >> 8) & 0xFF;
    temp_buf[2] = (vdm_header >> 16) & 0xFF;
    temp_buf[3] = (vdm_header >> 24) & 0xFF;
    
    temp_buf[4] = id_header & 0xFF;
    temp_buf[5] = (id_header >> 8) & 0xFF;
    temp_buf[6] = (id_header >> 16) & 0xFF;
    temp_buf[7] = (id_header >> 24) & 0xFF;
    
    temp_buf[8] = cert_stat & 0xFF;
    temp_buf[9] = (cert_stat >> 8) & 0xFF;
    temp_buf[10] = (cert_stat >> 16) & 0xFF;
    temp_buf[11] = (cert_stat >> 24) & 0xFF;
    
    temp_buf[12] = product_vdo & 0xFF;
    temp_buf[13] = (product_vdo >> 8) & 0xFF;
    temp_buf[14] = (product_vdo >> 16) & 0xFF;
    temp_buf[15] = (product_vdo >> 24) & 0xFF;
    
    temp_buf[16] = UFP_type_vdo & 0xFF;
    temp_buf[17] = (UFP_type_vdo >> 8) & 0xFF;
    temp_buf[18] = (UFP_type_vdo >> 16) & 0xFF;
    temp_buf[19] = (UFP_type_vdo >> 24) & 0xFF;
    
    sendPacket(false, 5, msg_id, 0, spec_revs[0] - 1, 0, MSG_TYPE_VDM, temp_buf);
    Serial1.println("Discovery identity response sent");
}

/**
 * Send discover SVID response
 */
void send_dis_svid_response() {
    uint16_t dev_VID = 0x0483; // VID from Intel Corp
    uint16_t dev_PID = 0x1307; // PID from Intel Corp
    uint16_t dev_PD_SID = 0xFF00; // SVID field for discover identity command
    
    uint32_t vdm_header = (dev_PD_SID << 16) | (1 << 15) | (1 << 13) | (2 << 6) | (2);
    
    temp_buf[0] = vdm_header & 0xFF;
    temp_buf[1] = (vdm_header >> 8) & 0xFF;
    temp_buf[2] = (vdm_header >> 16) & 0xFF;
    temp_buf[3] = (vdm_header >> 24) & 0xFF;
    
    sendPacket(false, 1, msg_id, 0, spec_revs[0] - 1, 0, MSG_TYPE_VDM, temp_buf);
    Serial1.println("Discovery SVID response sent");
}

/**
 * Send extended source capabilities
 */
void send_ext_src_cap() {
    uint16_t dev_VID = 0x0483; // VID from Intel Corp
    uint16_t dev_PID = 0x1307; // PID from Intel Corp
    
    temp_buf[0] = 0; // Extended message header
    temp_buf[1] = 0x19; // Data size (25 bytes in SCEDB)
    
    temp_buf[2] = dev_VID & 0xFF;
    temp_buf[3] = (dev_VID >> 8) & 0xFF;
    temp_buf[4] = dev_PID & 0xFF;
    temp_buf[5] = (dev_PID >> 8) & 0xFF;
    // Rest of SCEDB left blank
    
    temp_buf[10] = 0xFF; // Firmware version number
    temp_buf[11] = 0xFF; // Hardware version number  
    temp_buf[13] = 0x3; // 3ms holdup time
    temp_buf[15] = 0x7; // Minimal leakage, ground pin exists and connected to protective earth
    
    temp_buf[25] = 0x2E; // PDP rating = 46W
    
    sendPacket(true, 0, msg_id, 0, spec_revs[0] - 1, 0, 0x1, temp_buf);
    Serial1.println("Extended source capabilities response sent");
}

/**
 * Get specification revision information
 */
void get_spec_rev() {
    sendPacket(false, 0, msg_id, 0, spec_revs[0] - 1, 0, 0x18, NULL);
    Serial1.println("Fetching revision and version specifications...");
    delay(1000);
    
    while (getReg(REG_STATUS1) & 0x20) {
        delay(1);
    }
    receivePacket();
    
    while (getReg(REG_STATUS1) & 0x20) {
        delay(1);
    }
    
    uint32_t rmdo = read_rmdo();
    if (rmdo) {
        spec_revs[0] = ((rmdo & (0xF << 28)) >> 28); // Revision major
        spec_revs[1] = ((rmdo & (0xF << 24)) >> 24); // Revision minor
        spec_revs[2] = ((rmdo & (0xF << 20)) >> 20); // Version major
        spec_revs[3] = ((rmdo & (0xF << 16)) >> 16); // Version minor
    } else {
        Serial1.println("Invalid RMDO packet");
    }
}

/**
 * Process remaining messages after initial negotiation
 */
bool read_rest(int volts, int amps) {
    temp_buf[80] = {};
    rx_buf[80] = {};
    
    uint8_t message_type;
    uint8_t num_data_objects;
    bool extended;
    
    receiveBytes(rx_buf, 1);
    if (getReg(REG_STATUS1) & 0x20) { // RX FIFO empty
        Serial1.println("No more trailing messages");
        return true;
    }
    
    receiveBytes(rx_buf, 2);
    num_data_objects = ((rx_buf[1] & 0x70) >> 4);
    
    if (spec_revs[0] == 3) {
        message_type = (rx_buf[0] & 0x1F);
    } else {
        message_type = (rx_buf[0] & 0x0F);
    }
    extended = (rx_buf[1] >> 7);
    
    if ((num_data_objects == 0) && (message_type == MSG_TYPE_GET_SINK_CAP)) {
        Serial1.println("Sink capabilities requested");
        receiveBytes(rx_buf, 4); // Clear CRC-32
        send_snk_cap(volts, amps);
        
        unsigned long time = millis();
        while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 300))) {}
        receivePacket();
        
        while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 3300))) {}
        read_rest(volts, amps);
        return true;
        
    } else if ((num_data_objects > 0) && (message_type == 0x1)) {
        Serial1.println("Source capabilities message received");
        receiveBytes(rx_buf, (num_data_objects * 4));
        
        int index = 0;
        while (!int_flag) {}
        
        for (uint8_t i = 0; i < num_data_objects; i++) {
            uint32_t byte1 = rx_buf[0 + i * 4];
            uint32_t byte2 = rx_buf[1 + i * 4] << 8;
            uint32_t byte3 = rx_buf[2 + i * 4] << 16;
            uint32_t byte4 = rx_buf[3 + i * 4] << 24;
            int32_t pdo = byte1 | byte2 | byte3 | byte4;
            
            if ((pdo >> 30) == 0) { // Fixed supply
                volt_options[index] = ((pdo >> 10) & 0x3FF) / 20; // Convert to 1V units
                amp_options[index] = (pdo & 0x3FF); // Keep in 10mA units
                options_pos[index] = i + 1; // Position 0001 is always safe 5V
                index++;
                break;
            }
        }
        
        receiveBytes(rx_buf, 4); // CRC-32
        sel_src_cap(volts, amps);
        
        unsigned long time = millis();
        while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 3300))) {}
        read_rest(volts, amps);
        return true;
        
    } else if ((num_data_objects > 0) && (message_type == MSG_TYPE_VDM)) {
        receiveBytes(rx_buf, (num_data_objects * 4));
        uint8_t command = rx_buf[0] & 0x1F;
        uint8_t cmd_type = rx_buf[0] >> 6;
        receiveBytes(rx_buf, 4); // CRC-32
        
        if ((command == 1) && (cmd_type == 0)) {
            Serial1.println("Discovery identity request VDM");
            send_dis_idt_response();
            
            unsigned long time = millis();
            while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 300))) {}
            receivePacket();
            
            while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 500))) {}
            read_rest(volts, amps);
            return true;
            
        } else if ((command == 2) && (cmd_type == 0)) {
            Serial1.println("Discovery SVID request VDM");
            send_dis_svid_response();
            
            unsigned long time = millis();
            while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 300))) {}
            receivePacket();
            
            while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 500))) {}
            read_rest(volts, amps);
            return true;
        }
        
    } else if ((num_data_objects == 0) && (message_type == MSG_TYPE_GET_SOURCE_CAP)) {
        receiveBytes(rx_buf, 4); // CRC-32
        Serial1.println("Source capabilities requested from source");
        sendPacket(false, 0, msg_id, 0, spec_revs[0] - 1, 0, MSG_TYPE_NOT_SUPPORTED, NULL);
        Serial1.println("Replied with 'not supported'");
        
        unsigned long time = millis();
        while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 300))) {}
        receivePacket();
        read_rest(volts, amps);
        return true;
        
    } else if ((num_data_objects == 0) && (message_type == MSG_TYPE_GET_SOURCE_CAP_EXT)) {
        receiveBytes(rx_buf, 4); // CRC-32
        // Extended source cap request handling (commented for speed optimization)
        read_rest(volts, amps);
        return true;
        
    } else {
        Serial1.println("Miscellaneous message detected");
        Serial1.print("Extended: ");
        Serial1.println(extended);
        Serial1.print("Number of data objects: ");
        Serial1.println(num_data_objects);
        Serial1.print("Message type: ");
        Serial1.println(message_type);
        
        receiveBytes(rx_buf, 4); // CRC-32
        unsigned long time = millis();
        while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 3300))) {}
        read_rest(volts, amps);
        return true;
    }
    return true;
}

/**
 * Reset FUSB302B to initial state
 */
void reset_fusb() {
    setReg(REG_RESET, 0x01); // Reset FUSB302
    setReg(REG_POWER, 0x0F); // Full power
    setReg(REG_CONTROL1, 0x04); // Flush RX
    setReg(REG_CONTROL0, 0x00); // Disable all interrupt masks
    setReg(REG_MASK, 0x77); // Mask all interrupts except VBUSOK
    setReg(0x02, 0x03); // Enable both pull-downs (enables attach detection)
    setReg(REG_CONTROL2, 0x20); // Turn off auto GoodCRC and set power/data roles to SNK
}

/**
 * Check and process interrupts
 */
void check_interrupt() {
    uint8_t i_reg = getReg(REG_INTERRUPT);
    bool i_alert = i_reg & 0x8;
    bool i_vbusok = i_reg >> 7;
    bool vbusok = getReg(REG_STATUS0) >> 7;
    
    if (i_vbusok) {
        if (vbusok) {
            if (!attached) {
                new_attach = true;
                Serial1.println("NEW ATTACH");
            } else {
                new_attach = false;
            }
            attached = true;
        } else {
            attached = false;
            Serial1.println("DETACHED");
        }
    } else if (i_alert) {
        Serial1.println("TX_FULL or RX_FULL, alert interrupt");
    } else {
        Serial1.println("Interrupt triggered but VBUSOK=0 and alert=0");
        if (attached) {
            new_attach = false;
        }
    }
}

/**
 * Determine CC line orientation
 */
void orient_cc() {
    setReg(0x02, 0x07); // Measure CC1
    unsigned long time = millis();
    while (millis() < (time + 150)) {}
    
    meas_cc1 = getReg(REG_STATUS0) & 3;
    Serial1.print("BC level after measuring CC1: ");
    Serial1.println(meas_cc1, BIN);
    
    setReg(0x02, 0x0B); // Switch to measuring CC2
    time = millis();
    while (millis() < (time + 150)) {}
    
    meas_cc2 = getReg(REG_STATUS0) & 3;
    Serial1.print("BC level after measuring CC2: ");
    Serial1.println(meas_cc2, BIN);
    
    if (meas_cc1 > meas_cc2) {
        cc_line = 1;
        vconn_line = 2;
    } else {
        cc_line = 2;
        vconn_line = 1;
    }
}

/**
 * Enable transmission on specified CC line
 */
void enable_tx_cc(int cc, bool autocrc) {
    if (cc == 1) {
        setReg(0x02, 0x07); // Switch on MEAS_CC1
        setReg(REG_CONTROL1, 0x04); // Flush RX
        if (autocrc) {
            setReg(REG_CONTROL2, 0x25); // Enable BMC TX on CC1, auto CRC ON
        } else {
            setReg(REG_CONTROL2, 0x21); // Enable BMC TX on CC1, auto CRC OFF
        }
    } else if (cc == 2) {
        setReg(0x02, 0x0B); // Switch on MEAS_CC2
        setReg(REG_CONTROL1, 0x04); // Flush RX
        if (autocrc) {
            setReg(REG_CONTROL2, 0x26); // Enable BMC TX on CC2, auto CRC ON
        } else {
            setReg(REG_CONTROL2, 0x22); // Enable BMC TX on CC2, auto CRC OFF
        }
    }
}

/**
 * Initialize power delivery negotiation
 */
bool pd_init(int volts, int amps) {
    while (getReg(REG_STATUS1) & 0x20) {} // Wait while FIFO RX is empty
    read_pdo();
    
    unsigned long time = millis();
    if (sel_src_cap(volts, amps)) {
        while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 3300))) {}
        read_rest(volts, amps);
    }
    
    return true;
}

/**
 * Renegotiate power delivery after initialization
 */
bool reneg_pd(int volts, int amps) {
    setReg(REG_CONTROL1, 0x04); // Flush RX
    get_src_cap();
    sel_src_cap(volts, amps);
    return true;
}

/**
 * Recognize connected device type
 */
void recog_dev(int volts, int amps) {
    Serial1.println("(Spec Rev 3)");
    spec_revs[0] = 3; // Temporarily switch to spec rev 3
    pd_init(volts, amps);
    
    sendPacket(false, 0, msg_id, 0, spec_revs[0] - 1, 0, MSG_TYPE_GET_SOURCE_CAP_EXT, NULL);
    Serial1.println("Requested extended source capabilities");
    
    unsigned long time = millis();
    while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 500))) {}
    receivePacket(); // For GoodCRC
    
    while ((getReg(REG_STATUS1) & 0x20) && (millis() < (time + 500))) {}
    if (read_ext_src_cap()) {
        Serial1.print("VID & PID registered successfully ---> ");
        switch (dev_type) {
            case 0:
                Serial1.println("charger");
                break;
            case 1:
                Serial1.println("monitor");
                break;
            case 2:
                Serial1.println("tablet");
                break;
            case 3:
                Serial1.println("laptop/computer");
                break;
        }
    } else {
        Serial1.println("VID & PID detection failed, defaulting device type to --> charger");
        dev_type = 0;
    }
    
    setReg(REG_CONTROL3, 0x40);
    Serial1.println("Hard reset sent");
    
    spec_revs[0] = 2;
    Serial1.println("(Spec Rev 2)");
}

/**
 * Interrupt service routine flag setter
 */
void InterruptFlagger(uint gpio, uint32_t events) {
    int_flag = true;
}

/**
 * Arduino setup function (core 0)
 */
void setup() {
    // Core 0 setup if needed
}

/**
 * Arduino setup function (core 1)
 */
void setup1() {
    Serial1.begin(115200);
    Wire.begin();
    pinMode(6, INPUT_PULLUP); // Interrupt pin from FUSB302B
    gpio_set_irq_enabled_with_callback(6, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, 
                                       true, &InterruptFlagger);
    
    delay(150);
    reset_fusb();
}

/**
 * Arduino main loop (core 0)
 */
void loop() {
    // Core 0 main loop if needed
}

/**
 * Arduino main loop (core 1)
 */
void loop1() {
    if (int_flag) {
        check_interrupt(); // Process attach/detach events
        
        if (attached && new_attach) {
            orient_cc();
            enable_tx_cc(cc_line, true);
            recog_dev(5, 0.5);
            
            reset_fusb();
            enable_tx_cc(cc_line, true);
            pd_init(5, 0.5);
            
            Serial1.println("-------------------------------------");
            
            // Optional: Renegotiate to higher power after delay
            // delay(5000);
            // reneg_pd(20, 1);
        } else if (!attached) {
            reset_fusb();
        }
        
        int_flag = false;
    }
}
