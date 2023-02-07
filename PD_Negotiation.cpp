#include <Arduino.h>
#include <Wire.h>

const uint8_t PD_ADDR = 0x22;
uint8_t tx_buf[80];
uint8_t rx_buf[80];
uint8_t temp_buf[80];
int volt_options[5] = {-1,-1,-1,-1,-1};
int amp_options[5] = {-1,-1,-1,-1,-1};
int options_pos[5] = {-1,-1,-1,-1,-1};
int spec_revs[4] = {2,0, 0,0}; // true revision major, true revision minor, true version major, true version minor
int msg_id = 2;

void sendPacket( \
      uint8_t num_data_objects, \
      uint8_t message_id, \
      uint8_t port_power_role, \
      uint8_t spec_rev, \
      uint8_t port_data_role, \
      uint8_t message_type, \
      uint8_t *data_objects );

void setReg(uint8_t addr, uint8_t value) {
  Wire.beginTransmission(PD_ADDR);
  Wire.write(addr);
  Wire.write(value);
  Wire.endTransmission(true);
}

uint8_t getReg(uint8_t addr) {
  Wire.beginTransmission(PD_ADDR);
  Wire.write(addr);
  Wire.endTransmission(false);
  Wire.requestFrom((int)PD_ADDR, 1, true);
  return Wire.read();
}

void sendBytes(uint8_t *data, uint16_t length) {
  if (length > 0) {
    Wire.beginTransmission(PD_ADDR);
    Wire.write(0x43);
    for (uint16_t i=0; i<length; i++) {
      Wire.write(data[i]);
    }
    Wire.endTransmission(true);
  }
}

void receiveBytes(uint8_t *data, uint16_t length) {
  if (length > 0) {
    Wire.beginTransmission(PD_ADDR);
    Wire.write(0x43);
    Wire.endTransmission(false);
    Wire.requestFrom((int)PD_ADDR, (int)length, true);
    for (uint16_t i=0; i<length; i++) {
      data[i] = Wire.read();
    }
  }
}

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
  message_id       = ((rx_buf[1] & 0x0E) >> 1);
  port_power_role  = (rx_buf[1] & 0x01);
  spec_rev         = ((rx_buf[0] & 0xC0) >> 6);
  port_data_role   = ((rx_buf[0] & 0x10) >> 5);
  message_type     = (rx_buf[0] & 0x0F);
  if(num_data_objects){
    Serial1.println("Data Msg received");
    spec_revs[0] = spec_rev + 1;
  }else if(message_type == 0x1){
    Serial1.println("GoodCRC Msg received");
    receiveBytes(rx_buf, 4); // crc-32
    Serial1.println();
    return true;
  }else{
    Serial1.println("Control Msg received");
  }
  Serial1.println("Received SOP Packet");
  Serial1.print("Header: 0x");
  Serial1.println(*(int *)rx_buf, HEX);
  Serial1.print("num_data_objects = ");
  Serial1.println(num_data_objects, DEC);
  Serial1.print("message_id       = ");
  Serial1.println(message_id, DEC);
  Serial1.print("port_power_role  = ");
  Serial1.println(port_power_role, DEC);
  Serial1.print("spec_rev         = ");
  Serial1.println(spec_rev, DEC);
  Serial1.print("port_data_role   = ");
  Serial1.println(port_data_role, DEC);
  Serial1.print("message_type     = ");
  Serial1.println(message_type, DEC);

  receiveBytes(rx_buf, (num_data_objects*4));
  // each data object is 32 bits
  for (uint8_t i=0; i<num_data_objects; i++) {
    Serial1.print("Object: 0x");
    uint32_t byte1 = rx_buf[0 + i*4];
    uint32_t byte2 = rx_buf[1 + i*4]<<8;
    uint32_t byte3 = rx_buf[2 + i*4]<<16;
    uint32_t byte4 = rx_buf[3 + i*4]<<24;
    Serial1.println(byte1|byte2|byte3|byte4, HEX);
  }  
  // CRC-32
  receiveBytes(rx_buf, 4);
  Serial1.print("CRC-32: 0x");
  Serial1.println(*(long *)rx_buf, HEX);
  Serial1.println();
  return true;
}

void readAllRegs() {
  Wire.beginTransmission(PD_ADDR);
  Wire.write(0x01);
  Wire.endTransmission(false);
  Wire.requestFrom((int)PD_ADDR, 16, 1);
  for (int i=1; i<=16; i++) {
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
  for (int i=0x3C; i<=0x42; i++) {
    uint8_t c = Wire.read();
    Serial1.print("Address: 0x");
    Serial1.print(i, HEX);
    Serial1.print(", Value: 0x");
    Serial1.println(c, HEX);
  }
  Serial1.println();
  Serial1.println();
}

void sendPacket( \
      uint8_t num_data_objects, \
      uint8_t message_id, \
      uint8_t port_power_role, \
      uint8_t spec_rev, \
      uint8_t port_data_role, \
      uint8_t message_type, \
      uint8_t *data_objects ) {

  uint8_t temp;

  tx_buf[0]  = 0x12; // SOP, see USB-PD2.0 page 108
  tx_buf[1]  = 0x12;
  tx_buf[2]  = 0x12;
  tx_buf[3]  = 0x13;
  tx_buf[4]  = (0x80 | (2 + (4*(num_data_objects & 0x1F))));
  tx_buf[5]  = (message_type & 0x0F);
  tx_buf[5] |= ((port_data_role & 0x01) << 5);
  tx_buf[5] |= ((spec_rev & 0x03) << 6);
  tx_buf[6]  = (port_power_role & 0x01);
  tx_buf[6] |= ((message_id & 0x07) << 1);
  tx_buf[6] |= ((num_data_objects & 0x07) << 4);

  Serial1.print("Sending Header: 0x");
  Serial1.println((tx_buf[6]<<8)|(tx_buf[5]), HEX);
  Serial1.println();

  temp = 7;
  for(uint8_t i=0; i<num_data_objects; i++) {
    tx_buf[temp]   = data_objects[(4*i)];
    tx_buf[temp+1] = data_objects[(4*i)+1];
    tx_buf[temp+2] = data_objects[(4*i)+2];
    tx_buf[temp+3] = data_objects[(4*i)+3];
    Serial1.print("Data object being sent out: 0x");
    Serial1.println( (tx_buf[temp+3]<<24)|(tx_buf[temp+2]<<16)|(tx_buf[temp+1]<<8)|(tx_buf[temp]), HEX );
    temp += 4;
  }
  Serial1.print("msg_id: ");
  Serial1.println(message_id, DEC);

  tx_buf[temp] = 0xFF; // CRC
  tx_buf[temp+1] = 0x14; // EOP
  tx_buf[temp+2] = 0xFE; // TXOFF

  temp = getReg(0x06);
  sendBytes(tx_buf, (10+(4*(num_data_objects & 0x1F))) );
  setReg(0x06, (temp | (0x01))); // Flip on TX_START
  msg_id++;
}






/*  Higher Level Abstractions   */

bool read_pdo(){  // modified version of receive packet function that handles src cap pdos
  uint8_t num_data_objects;
  uint8_t message_type;
  uint8_t spec_rev;
  
  for(int i=0; i<5; i++){
    volt_options[i] = -1;
    amp_options[i] = -1;
    options_pos[i] = -1;
  }

  receiveBytes(rx_buf, 1);
  if (rx_buf[0] != 0xE0) {
    Serial1.println("NO RESPONSE RECEIVED - read pdo");
    return false;
  }

  receiveBytes(rx_buf, 2);
  num_data_objects = ((rx_buf[1] & 0x70) >> 4);
  spec_rev         = ((rx_buf[0] & 0xC0) >> 6);
  message_type     = (rx_buf[0] & 0x0F);
  if(message_type == 0x1){
    Serial1.println("src cap msg received");
    spec_revs[0] = spec_rev + 1;
    Serial1.print("header spec_rev was set to: ");
    Serial1.println(spec_rev + 1);
  }else{
    Serial1.println("msg received, but not src cap msg");
    return false;
  }
  Serial1.print("num_data_objects = ");
  Serial1.println(num_data_objects, DEC);

  receiveBytes(rx_buf, (num_data_objects*4));
  int index = 0;
  for (uint8_t i=0; i<num_data_objects; i++) {
    Serial1.print("PDO: ");
    uint32_t byte1 = rx_buf[0 + i*4];
    uint32_t byte2 = rx_buf[1 + i*4]<<8;
    uint32_t byte3 = rx_buf[2 + i*4]<<16;
    uint32_t byte4 = rx_buf[3 + i*4]<<24;
    int32_t pdo = byte1|byte2|byte3|byte4;
    switch(pdo>>30){
      case 0x0: // fixed supply
        Serial1.print("fixed supply pdo: ");
        volt_options[index] = ((pdo>>10) & 0x3FF)/20;   // Converted to 1 volt units
        amp_options[index] = ((pdo & 0x3FF));   // Left in 10mA units
        options_pos[index] = i+1; // 0001 is always safe5v
        Serial1.print(volt_options[index], DEC);
        Serial1.print("V, ");
        Serial1.print(amp_options[index]*10, DEC);
        Serial1.print("mA max, ");
        Serial1.println(options_pos[index]);
        index++;
        break;
      case 0x1: // battery supply
        Serial1.print("battery supply pdo: ");
        Serial1.println(pdo, HEX);
        break;
      case 0x2: // variable supply
        Serial1.print("variable supply pdo: ");
        Serial1.println(pdo, HEX);
        break;
      case 0x3: // augmented pdo
        Serial1.print("augmented pdo: ");
        Serial1.println(pdo, HEX);
        break;
    }
  } 
  // CRC-32
  receiveBytes(rx_buf, 4);
  return true;
}


bool get_req_outcome(){ // receive packet function that only analyzes message type for "accept" for control packets
  uint8_t message_type;

  receiveBytes(rx_buf, 1);
  if (rx_buf[0] != 0xE0) {
    Serial1.println("NO RESPONSE RECEIVED - get req outcome");
    return false;
  }
  receiveBytes(rx_buf, 2);
  message_type = (rx_buf[0] & 0x0F);
  if(message_type == 0x3){
    Serial1.println("request accepted");
    return true;
  }else{
    return false;
  }
}


void get_src_cap(){
  Serial1.println("fetching src cap info...");
  sendPacket( 0, msg_id, 0, spec_revs[0]-1, 0, 0x7, NULL );
  delay(1000);
  while ( getReg(0x41) & 0x20 ) { // while RX_empty, wait
    delay(1);}
  receivePacket();
  while ( getReg(0x41) & 0x20 ) { // while RX_empty, wait
    delay(1);}
  read_pdo();
}

/*  Must declare this after declaring get_src_cap   */
bool sel_src_cap(int volts, int amps){    // 1V and 1A units for input
  bool possible_v = 0;
  bool possible_a = 0;
  int idx = 0;
  for(int i=0; i<5; i++){
    if(volt_options[i] == volts){
      possible_v = 1;
      if(amp_options[i] >= amps){
        possible_a = 1;
        idx = i;
      }
    }
  }
  if(possible_v & possible_a){
    Serial1.println("requesting voltage & current...");
    uint32_t request_msg = (options_pos[idx]<<28) | ((amps*100)<<10) | (amp_options[idx]);
    temp_buf[0] = request_msg & 0xFF;
    temp_buf[1] = (request_msg>>8) & 0xFF;
    temp_buf[2] = (request_msg>>16) & 0xFF;
    temp_buf[3] = (request_msg>>24) & 0xFF;
    Serial1.print("requested obj pos: ");
    Serial1.println(options_pos[idx]);
    sendPacket( 1, msg_id, 0, spec_revs[0]-1, 0, 0x2, temp_buf );
    delay(1000);
    while ( getReg(0x41) & 0x20 ) { // while RX_empty, wait
      delay(1);}
    receivePacket();
    while ( getReg(0x41) & 0x20 ) {  // while RX_empty, wait
      delay(1);}
    get_req_outcome();
    return true;
  }else if(!possible_v){
    Serial1.println("voltage not available");
    return false;
  }else if(!possible_a){
    Serial1.println("amp request too high for your voltage");
    return false;
  }
}


uint32_t read_rmdo(){  // read revision msg data obj
  uint8_t num_data_objects;
  uint8_t spec_rev;
  uint8_t message_type;
  
  receiveBytes(rx_buf, 1);
  if (rx_buf[0] != 0xE0) {
    Serial1.println("NO RESPONSE RECEIVED - read rmdo");
    return 0;
  }
  receiveBytes(rx_buf, 2);
  num_data_objects = ((rx_buf[1] & 0x70) >> 4);
  spec_rev         = ((rx_buf[0] & 0xC0) >> 6);
  message_type     = (rx_buf[0] & 0x0F);
  if((message_type == 0xC) && (num_data_objects==1)){
    Serial1.println("RMDO Msg received");
  }else{
    Serial1.print("Expected RMDO, incorrect packet type received. Received type: ");
    Serial1.println(message_type, BIN);
    Serial1.print("num of data objs: ");
    Serial1.println(num_data_objects);
    return 0;
  }
  receiveBytes(rx_buf, (num_data_objects*4));
  Serial1.print("RMDO: 0x");
  uint32_t byte1 = rx_buf[0];
  uint32_t byte2 = rx_buf[1]<<8;
  uint32_t byte3 = rx_buf[2]<<16;
  uint32_t byte4 = rx_buf[3]<<24;
  uint32_t rmdo = byte1|byte2|byte3|byte4;
  Serial1.println(byte1|byte2|byte3|byte4, HEX);  
  // CRC-32
  receiveBytes(rx_buf, 4);
  // Serial1.print("CRC-32: 0x");
  // Serial1.println(*(long *)rx_buf, HEX);
  Serial1.println();
  return rmdo;
}


void get_spec_rev(){   // saves revision majors and minors
  sendPacket( 0, msg_id, 0, spec_revs[0]-1, 0, 0x18, NULL );
  Serial1.println("fetching revision and version specs...");
  delay(1000);
  while ( getReg(0x41) & 0x20 ) { // while RX_empty, wait
    delay(1);}
  receivePacket();
  while ( getReg(0x41) & 0x20 ) {  // while RX_empty, wait
    delay(1);}
  uint32_t rmdo = read_rmdo();
  if(rmdo){
    spec_revs[0] = ((rmdo & (0xF<<28)) >> 28);  // true rev major
    spec_revs[1] = ((rmdo & (0xF<<24)) >> 24);  // true rev minor
    spec_revs[2] = ((rmdo & (0xF<<20)) >> 20);  // true ver major
    spec_revs[3] = ((rmdo & (0xF<<16)) >> 16);  // true ver minor
  }else{
    Serial1.println("invalid rmdo packet");
  }
}


bool pd_init(int volts, int amps){  // figure out why src is rejecting / ignoring rev request
  setReg(0x0C, 0x01); // Reset FUSB302
  setReg(0x0B, 0x0F); // FULL POWER!  
  setReg(0x07, 0x04); // Flush RX
  setReg(0x02, 0x0B); // Switch on MEAS_CC2
  //setReg(0x02, 0x07); // Switch on MEAS_CC1
  setReg(0x03, 0x26); // Enable BMC Tx on CC2
  //setReg(0x03, 0x25); // Enable BMC Tx on CC1

  readAllRegs();
  delay(200);
  Serial1.println("------------------------------------------");

  readAllRegs();
  read_pdo();
  Serial1.println("------------------------------------------");

  readAllRegs();
  Serial1.println("------------------------------------------");
  
  delay(50);
  sel_src_cap(volts, amps);
  readAllRegs();
  Serial1.println("------------------------------------------");

  //get_spec_rev();

  return true;
}

// figure out detecting attach / detach


void setup() {
  // put your setup code here, to run once:
  Serial1.begin(115200);
  Wire.begin();
  delay(500);

  pd_init(9, 1);  
  
  /* Request for 1A after specifying the 20V src_cap obj position */
  // temp_buf[0] = 0b01100100;
  // temp_buf[1] = 0b10010000;
  // temp_buf[2] = 0b00000001;
  // temp_buf[3] = 0b00110000;
}

void loop() {

}


/* ORIGINAL
  temp_buf[0] = 0b01100100;  
  temp_buf[1] = 0b10010000;  
  temp_buf[2] = 0b00000001;  
  temp_buf[3] = 0b00100000;
*/

/* Binary Stream Representation of Sink Capability Messages 
  1. 00010000000000011001000001100100   (vSafe5V)
  2. 00000000000001100100000001100100   (Request 20V 1A)

  temp_buf[0] = 0b01100100;
  temp_buf[1] = 0b10010000;
  temp_buf[2] = 0b00000001;
  temp_buf[3] = 0b00010000;
  temp_buf[4] = 0b01100100;
  temp_buf[5] = 0b01000000;
  temp_buf[6] = 0b00000110;
  temp_buf[7] = 0b00000000;
*/
