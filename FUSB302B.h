#ifndef FUSB302B_H
#define FUSB302B_H

#include <Arduino.h>
#include <stdint.h>

//=============================================================================
// FUSB302B Register Addresses
//=============================================================================

// Control and Configuration Registers
#define REG_CONTROL0        0x06
#define REG_CONTROL1        0x07
#define REG_CONTROL2        0x08
#define REG_CONTROL3        0x09
#define REG_MASK            0x0A
#define REG_POWER           0x0B
#define REG_RESET           0x0C

// Status and Interrupt Registers  
#define REG_STATUS0A        0x3C
#define REG_STATUS1A        0x3D
#define REG_INTERRUPTA      0x3E
#define REG_INTERRUPTB      0x3F
#define REG_STATUS0         0x40
#define REG_STATUS1         0x41
#define REG_INTERRUPT       0x42
#define REG_FIFOS           0x43

//=============================================================================
// USB-PD Protocol Constants
//=============================================================================

// FUSB302B I2C Address
#define PD_ADDR             0x22

// USB-PD Message Types (Control Messages)
#define MSG_TYPE_GOODCRC            0x1
#define MSG_TYPE_GOTOMIN            0x2
#define MSG_TYPE_ACCEPT             0x3
#define MSG_TYPE_REJECT             0x4
#define MSG_TYPE_PING               0x5
#define MSG_TYPE_PS_READY           0x6
#define MSG_TYPE_GET_SOURCE_CAP     0x7
#define MSG_TYPE_GET_SINK_CAP       0x8
#define MSG_TYPE_DR_SWAP            0x9
#define MSG_TYPE_PR_SWAP            0xA
#define MSG_TYPE_VCONN_SWAP         0xB
#define MSG_TYPE_WAIT               0xC
#define MSG_TYPE_SOFT_RESET         0xD
#define MSG_TYPE_NOT_SUPPORTED      0x10
#define MSG_TYPE_GET_SOURCE_CAP_EXT 0x11
#define MSG_TYPE_GET_STATUS         0x12
#define MSG_TYPE_FR_SWAP            0x13
#define MSG_TYPE_GET_PPS_STATUS     0x14
#define MSG_TYPE_GET_COUNTRY_CODES  0x15

// USB-PD Message Types (Data Messages)
#define MSG_TYPE_SOURCE_CAPABILITIES    0x1
#define MSG_TYPE_REQUEST                0x2
#define MSG_TYPE_BIST                   0x3
#define MSG_TYPE_SINK_CAPABILITIES      0x4
#define MSG_TYPE_BATTERY_STATUS         0x5
#define MSG_TYPE_ALERT                  0x6
#define MSG_TYPE_GET_COUNTRY_INFO       0x7
#define MSG_TYPE_VDM                    0xF

// Protocol Sequence Constants
#define SOP_SEQUENCE_0      0x12
#define SOP_SEQUENCE_1      0x12
#define SOP_SEQUENCE_2      0x12
#define SOP_SEQUENCE_3      0x13
#define EOP_SEQUENCE        0x14
#define TXOFF_SEQUENCE      0xFE
#define CRC_PLACEHOLDER     0xFF

//=============================================================================
// Device Type Enumerations
//=============================================================================

/**
 * @brief USB-C Device Types for automatic recognition
 */
typedef enum {
    DEVICE_TYPE_CHARGER = 0,    ///< Power supply/charger
    DEVICE_TYPE_MONITOR = 1,    ///< Display/monitor
    DEVICE_TYPE_TABLET = 2,     ///< Tablet or mobile device
    DEVICE_TYPE_LAPTOP = 3,     ///< Laptop or computer
    DEVICE_TYPE_UNKNOWN = 4     ///< Unknown device type
} pd_device_type_t;

/**
 * @brief Power Data Object (PDO) Types
 */
typedef enum {
    PDO_TYPE_FIXED_SUPPLY = 0,      ///< Fixed voltage supply
    PDO_TYPE_BATTERY = 1,           ///< Battery supply
    PDO_TYPE_VARIABLE_SUPPLY = 2,   ///< Variable voltage supply  
    PDO_TYPE_AUGMENTED = 3          ///< Augmented PDO (PPS, etc.)
} pdo_type_t;

/**
 * @brief VDM Command Types
 */
typedef enum {
    VDM_CMD_DISCOVER_IDENTITY = 1,  ///< Discover Identity command
    VDM_CMD_DISCOVER_SVID = 2,      ///< Discover SVID command
    VDM_CMD_DISCOVER_MODES = 3,     ///< Discover Modes command
    VDM_CMD_ENTER_MODE = 4,         ///< Enter Mode command
    VDM_CMD_EXIT_MODE = 5,          ///< Exit Mode command
    VDM_CMD_ATTENTION = 6           ///< Attention command
} vdm_command_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Device recognition database entry
 */
typedef struct {
    uint16_t vid;           ///< Vendor ID
    uint16_t pid;           ///< Product ID  
    uint8_t device_type;    ///< Device type (pd_device_type_t)
} device_db_entry_t;

/**
 * @brief Power capability option
 */
typedef struct {
    int voltage;            ///< Voltage in volts
    int current;            ///< Current in 10mA units
    int position;           ///< PDO position (1-7)
} power_option_t;

/**
 * @brief USB-PD specification revision info
 */
typedef struct {
    uint8_t rev_major;      ///< Revision major version
    uint8_t rev_minor;      ///< Revision minor version  
    uint8_t ver_major;      ///< Version major
    uint8_t ver_minor;      ///< Version minor
} pd_spec_rev_t;

//=============================================================================
// Global State Variables (External References)
//=============================================================================

// Power delivery state
extern int volt_options[5];        ///< Available voltage options
extern int amp_options[5];         ///< Available current options  
extern int options_pos[5];         ///< PDO position mapping
extern int spec_revs[4];           ///< Specification revision info
extern int msg_id;                 ///< Current message ID
extern int dev_type;               ///< Detected device type

// Attachment and CC line state
extern bool int_flag;              ///< Interrupt flag
extern bool attached;              ///< Device attachment status
extern bool new_attach;            ///< New attachment flag
extern int meas_cc1;               ///< CC1 measurement result
extern int meas_cc2;               ///< CC2 measurement result
extern int cc_line;                ///< Active CC line (1 or 2)
extern int vconn_line;             ///< VCONN line (1 or 2)

// Device recognition database
extern uint16_t dev_library[10][3]; ///< Device VID/PID database

//=============================================================================
// Core Hardware Interface Functions
//=============================================================================

/**
 * @brief Set a register value on the FUSB302B
 * @param addr Register address
 * @param value Value to write
 */
void setReg(uint8_t addr, uint8_t value);

/**
 * @brief Read a register value from the FUSB302B
 * @param addr Register address
 * @return Register value
 */
uint8_t getReg(uint8_t addr);

/**
 * @brief Send data bytes to FUSB302B FIFO
 * @param data Pointer to data buffer
 * @param length Number of bytes to send
 */
void sendBytes(uint8_t *data, uint16_t length);

/**
 * @brief Receive data bytes from FUSB302B FIFO
 * @param data Pointer to receive buffer
 * @param length Number of bytes to receive
 */
void receiveBytes(uint8_t *data, uint16_t length);

//=============================================================================
// USB-PD Protocol Functions
//=============================================================================

/**
 * @brief Send a USB-PD packet
 * @param extended Extended message flag
 * @param num_data_objects Number of 32-bit data objects
 * @param message_id Message ID (0-7)
 * @param port_power_role Power role (0=sink, 1=source)
 * @param spec_rev Specification revision (0-3)
 * @param port_data_role Data role (0=UFP, 1=DFP)
 * @param message_type Message type constant
 * @param data_objects Pointer to data objects array
 */
void sendPacket(bool extended, uint8_t num_data_objects, uint8_t message_id,
                uint8_t port_power_role, uint8_t spec_rev, uint8_t port_data_role,
                uint8_t message_type, uint8_t *data_objects);

/**
 * @brief Receive and parse a PD packet
 * @return true if packet received successfully
 */
bool receivePacket();

/**
 * @brief Read all FUSB302B registers for debugging
 */
void readAllRegs();

//=============================================================================
// Power Delivery Negotiation Functions  
//=============================================================================

/**
 * @brief Initialize power delivery negotiation
 * @param volts Requested voltage
 * @param amps Requested current in amps
 * @return true if initialization successful
 */
bool pd_init(int volts, int amps);

/**
 * @brief Renegotiate power delivery after initialization
 * @param volts New requested voltage
 * @param amps New requested current in amps
 * @return true if renegotiation successful
 */
bool reneg_pd(int volts, int amps);

/**
 * @brief Get source capabilities from connected device
 */
void get_src_cap();

/**
 * @brief Select and request specific source capability
 * @param volts Desired voltage
 * @param amps Desired current in amps
 * @return true if request accepted
 */
bool sel_src_cap(int volts, int amps);

/**
 * @brief Send sink capabilities to source
 * @param volts Maximum supported voltage
 * @param amps Maximum supported current in amps
 */
void send_snk_cap(int volts, int amps);

/**
 * @brief Get request outcome (accept/reject)
 * @return true if request was accepted
 */
bool get_req_outcome();

//=============================================================================
// Device Recognition Functions
//=============================================================================

/**
 * @brief Recognize connected device type using VID/PID
 * @param volts Initial negotiation voltage
 * @param amps Initial negotiation current  
 */
void recog_dev(int volts, int amps);

/**
 * @brief Read extended source capabilities message
 * @return true if extended capabilities received
 */
bool read_ext_src_cap();

/**
 * @brief Send discover identity request (VDM)
 */
void send_dis_idt_request();

/**
 * @brief Send discover identity response (VDM)
 */
void send_dis_idt_response();

/**
 * @brief Read discover identity response
 * @return true if valid response received
 */
bool read_dis_idt_response();

/**
 * @brief Send discover SVID response (VDM)
 */
void send_dis_svid_response();

/**
 * @brief Send extended source capabilities
 */
void send_ext_src_cap();

//=============================================================================
// Power Data Object (PDO) Functions
//=============================================================================

/**
 * @brief Read and parse Power Data Objects from source capabilities
 * @return true if PDOs parsed successfully
 */
bool read_pdo();

/**
 * @brief Read Revision Message Data Object
 * @return RMDO value, 0 if failed
 */
uint32_t read_rmdo();

/**
 * @brief Get USB-PD specification revision information
 */
void get_spec_rev();

//=============================================================================
// CC Line and Attachment Functions
//=============================================================================

/**
 * @brief Reset FUSB302B to initial state
 */
void reset_fusb();

/**
 * @brief Check and process interrupts
 */
void check_interrupt();

/**
 * @brief Determine CC line orientation (must be called before init)
 */
void orient_cc();

/**
 * @brief Enable transmission on specified CC line
 * @param cc CC line number (1 or 2)
 * @param autocrc Enable automatic CRC generation
 */
void enable_tx_cc(int cc, bool autocrc);

/**
 * @brief Interrupt service routine flag setter
 * @param gpio GPIO number
 * @param events GPIO events
 */
void InterruptFlagger(uint gpio, uint32_t events);

//=============================================================================
// Debug and Utility Functions
//=============================================================================

/**
 * @brief Read RX FIFO for debugging purposes
 */
void read_rx_fifo();

/**
 * @brief Process remaining messages after initial negotiation
 * @param volts Negotiated voltage
 * @param amps Negotiated current
 * @return true if processing successful
 */
bool read_rest(int volts, int amps);

//=============================================================================
// Arduino Setup Functions
//=============================================================================

/**
 * @brief Arduino setup function (core 0)
 */
void setup();

/**
 * @brief Arduino setup function (core 1) - Main PD initialization
 */
void setup1();

/**
 * @brief Arduino main loop (core 0)
 */
void loop();

/**
 * @brief Arduino main loop (core 1) - Main PD processing
 */
void loop1();

//=============================================================================
// Helper Macros
//=============================================================================

/**
 * @brief Convert voltage to PDO voltage units (50mV units)
 */
#define VOLTAGE_TO_PDO(v) ((v) * 20)

/**
 * @brief Convert current to PDO current units (10mA units)  
 */
#define CURRENT_TO_PDO(a) ((a) * 100)

/**
 * @brief Convert PDO voltage to volts
 */
#define PDO_TO_VOLTAGE(pdo_v) ((pdo_v) / 20)

/**
 * @brief Convert PDO current to amps
 */
#define PDO_TO_CURRENT(pdo_a) ((pdo_a) / 100)

/**
 * @brief Extract PDO type from 32-bit PDO
 */
#define GET_PDO_TYPE(pdo) (((pdo) >> 30) & 0x3)

/**
 * @brief Check if device is attached
 */
#define IS_DEVICE_ATTACHED() (attached)

/**
 * @brief Check if new device just attached
 */
#define IS_NEW_ATTACHMENT() (attached && new_attach)

#endif // FUSB302B_H 