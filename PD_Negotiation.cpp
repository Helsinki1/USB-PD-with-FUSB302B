#include <Arduino.h>
#include <Wire.h>

const uint8_t PD_ADDR = 0x22;
uint8_t tx_buf[80];
uint8_t rx_buf[80];
uint8_t temp_buf[80];
uint16_t dev_library[10][3] = { // VID, PID, dev type (0:charger, 1:monitor, 2:tablet, 3:laptop/computer)
  {0x2B01, 0xF663, 0}, // long xiaomi charger
  {0x05AC, 0x7109, 2}, // ipad
  {0x413C, 0xB057, 3}, // Dell XPS laptop
  {0x04E8, 0x0, 2}, // Samsung Fold
  {0x05C6, 0x0, 2}, // OnePlus Phone (qualcomm processor)
  {0,0,0},
  {0,0,0}
  };
int dev_type; // (0:charger, 1:monitor, 2:laptop/computer)
int volt_options[5] = {-1,-1,-1,-1,-1};
int amp_options[5] = {-1,-1,-1,-1,-1};
int options_pos[5] = {-1,-1,-1,-1,-1};
int spec_revs[4] = {2,0, 0,0}; // true revision major, true revision minor, true version major, true version minor
int msg_id = 0;
bool int_flag = 0;
bool attached = 0;
bool new_attach = 0;
int meas_cc1; // value of bc lvl after meas cc1
int meas_cc2; // value of bc lvl after meas cc2
int cc_line;
int vconn_line;

void sendPacket( \
      bool extended, \
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
  if (getReg(0x41) & 0x20) {
    Serial1.println("rx empty - receive packet");
    return false;
  }
  receiveBytes(rx_buf, 2);
  num_data_objects = ((rx_buf[1] & 0x70) >> 4);
  message_id       = ((rx_buf[1] & 0x0E) >> 1);
  port_power_role  = (rx_buf[1] & 0x01);
  spec_rev         = ((rx_buf[0] & 0xC0) >> 6);
  port_data_role   = ((rx_buf[0] & 0x10) >> 5);
  if(spec_revs[0] = 3){
    message_type     = (rx_buf[0] & 0x1F);
  }else{
    message_type     = (rx_buf[0] & 0x0F);
  }
  if(num_data_objects){
    Serial1.println("Data Msg received");
  }else if(message_type == 0x1){
    Serial1.println("GoodCRC Msg received");
    receiveBytes(rx_buf, 4); // crc-32
    return true;
  }else{
    Serial1.print("Control Msg received, type: ");
    Serial1.println(message_type, BIN);
  }
  // Serial1.println("Received SOP Packet");
  // Serial1.print("Header: 0x");
  // Serial1.println(*(int *)rx_buf, HEX);
  // Serial1.print("num_data_objects = ");
  // Serial1.println(num_data_objects, DEC);
  // Serial1.print("message_id       = ");
  // Serial1.println(message_id, DEC);
  // Serial1.print("port_power_role  = ");
  // Serial1.println(port_power_role, DEC);
  // Serial1.print("spec_rev         = ");
  // Serial1.println(spec_rev, DEC);
  // Serial1.print("port_data_role   = ");
  // Serial1.println(port_data_role, DEC);
  // Serial1.print("message_type     = ");
  // Serial1.println(message_type, DEC);

  receiveBytes(rx_buf, (num_data_objects*4));
  // each data object is 32 bits
  for (uint8_t i=0; i<num_data_objects; i++) {
    // Serial1.print("Object: 0x");
    // uint32_t byte1 = rx_buf[0 + i*4];
    // uint32_t byte2 = rx_buf[1 + i*4]<<8;
    // uint32_t byte3 = rx_buf[2 + i*4]<<16;
    // uint32_t byte4 = rx_buf[3 + i*4]<<24;
    // Serial1.println(byte1|byte2|byte3|byte4, HEX);
  }  
  // CRC-32
  receiveBytes(rx_buf, 4);
  // Serial1.print("CRC-32: 0x");
  // Serial1.println(*(long *)rx_buf, HEX);
  // Serial1.println();
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
      bool extended, \
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
  tx_buf[4]  = (0x80 | (2 + (4*(num_data_objects & 0x07))));
  tx_buf[5]  = (message_type & 0x1F); // only for extended src cap request
  tx_buf[5] |= ((port_data_role & 0x01) << 5);
  tx_buf[5] |= ((spec_rev & 0x03) << 6);
  tx_buf[6]  = (port_power_role & 0x01);
  tx_buf[6] |= ((message_id & 0x07) << 1);
  tx_buf[6] |= ((num_data_objects & 0x07) << 4);
  tx_buf[6] |= (extended << 7);

  temp = 7;
  for(uint8_t i=0; i<num_data_objects; i++) {
    tx_buf[temp]   = data_objects[(4*i)];
    tx_buf[temp+1] = data_objects[(4*i)+1];
    tx_buf[temp+2] = data_objects[(4*i)+2];
    tx_buf[temp+3] = data_objects[(4*i)+3];
    temp += 4;
  }

  tx_buf[temp] = 0xFF; // CRC
  tx_buf[temp+1] = 0x14; // EOP
  tx_buf[temp+2] = 0xFE; // TXOFF

  temp = getReg(0x06);
  sendBytes(tx_buf, (10+(4*(num_data_objects & 0x07))) );
  setReg(0x06, (temp | (0x01))); // Flip on TX_START
  msg_id++;
}






/*  Higher Level Abstractions   */

void read_rx_fifo(){ // for debugging purposes
  int i = 1;
  while(!(getReg(0x41) & 0x20)){
    Serial1.print("byte number ");
    Serial1.print(i);
    Serial1.print(": 0x");
    receiveBytes(rx_buf, 1);
    Serial1.println(rx_buf[0], HEX);
    i++;
  }
}

bool read_pdo(){  // (spec rev is updated here) modified version of receive packet function that handles src cap pdos
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
  if((message_type == 0x1)&(num_data_objects>0)){
    Serial1.println("src cap msg received");
    // spec_revs[0] = spec_rev + 1;
    //Serial1.print("header spec_rev was set to: ");
    //Serial1.println(spec_rev + 1);
  }else{
    Serial1.println("msg received, but not src cap msg");
    return false;
  }
  //Serial1.print("num_data_objects = ");
  //Serial1.println(num_data_objects, DEC);
  receiveBytes(rx_buf, (num_data_objects*4));
  int index = 0;
  for (uint8_t i=0; i<num_data_objects; i++) {
    //Serial1.print("PDO: ");
    uint32_t byte1 = rx_buf[0 + i*4];
    uint32_t byte2 = rx_buf[1 + i*4]<<8;
    uint32_t byte3 = rx_buf[2 + i*4]<<16;
    uint32_t byte4 = rx_buf[3 + i*4]<<24;
    int32_t pdo = byte1|byte2|byte3|byte4;
    switch(pdo>>30){
      case 0x0: // fixed supply
        //Serial1.print("fixed supply pdo: ");
        volt_options[index] = ((pdo>>10) & 0x3FF)/20;   // Converted to 1 volt units
        amp_options[index] = ((pdo & 0x3FF));   // Left in 10mA units
        options_pos[index] = i+1; // 0001 is always safe5v
        // Serial1.print(volt_options[index], DEC);
        // Serial1.print("V, ");
        // Serial1.print(amp_options[index]*10, DEC);
        // Serial1.print("mA max, ");
        // Serial1.println(options_pos[index]);
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
  uint8_t num_data_objects;

  receiveBytes(rx_buf, 1);
  if (getReg(0x41) & 0x20) {
    Serial1.println("NO RESPONSE RECEIVED - get req outcome");
    return false;
  }
  receiveBytes(rx_buf, 2);
  message_type = (rx_buf[0] & 0x0F);
  num_data_objects = ((rx_buf[1] & 0x70) >> 4);
  receiveBytes(rx_buf, 4); // crc32
  if(message_type == 0x3){
    Serial1.println("request accepted");
    return true;
  }else if(message_type == 0x6){
    Serial1.println("power supply ready");
    return true;
  }else{
    Serial1.print("error, msg type: ");
    Serial1.println(message_type, DEC);
    Serial1.print("num data objects: ");
    Serial1.println(num_data_objects, DEC);
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
bool read_ext_src_cap(){ // reads extended src cap msg for vid & pid, compares them with those in "dev_library", prints them
  uint8_t num_data_objects;
  uint16_t ext_data_size;
  uint8_t message_type;
  bool extended_msg;
  uint16_t VID;
  uint16_t PID;
  
  receiveBytes(rx_buf, 1); // captures preamble
  if (getReg(0x41) & 0x20) {
    Serial1.println("empty rx_fifo - read_ext_src_cap");
    return false;
  }
  receiveBytes(rx_buf, 2); // captures header
  num_data_objects = ((rx_buf[1] & 0x70) >> 4);
  if(spec_revs[0] == 3){
    message_type = (rx_buf[0] & 0x1F); // represents ext msg type
  }else{
    message_type = (rx_buf[0] & 0x0F); // represents ext msg type
  }
  extended_msg     = (rx_buf[1] >> 7);
  if(extended_msg){
    Serial1.println("Extended Msg received");
    receiveBytes(rx_buf, 2); // captures extended header
    ext_data_size = (((rx_buf[1] & 0x1)<<8) | rx_buf[0]);
    if((ext_data_size>=24) & (message_type==1)){
      receiveBytes(rx_buf, ext_data_size); // captures extended src cap content
      VID = (rx_buf[1]<<8) | rx_buf[0];
      PID = (rx_buf[3]<<8) | rx_buf[2];
      Serial1.print("Device VID: ");
      Serial1.println(VID, HEX);
      Serial1.print("Device PID: ");
      Serial1.println(PID, HEX);



      setReg(0x07, 0x04); // Flush RX
      return true;
    }else{
      Serial1.print("data size: ");
      Serial1.println(ext_data_size, DEC);
      Serial1.print("msg type: ");
      Serial1.println(message_type, BIN);
      setReg(0x07, 0x04); // Flush RX
      return false;
    }
  }else{
    if((message_type == 16) & (num_data_objects == 0)){
      Serial1.println("extended src cap not supported");
      setReg(0x07, 0x04); // Flush RX
      return false;
    }
    Serial1.println("wrong type of message received - read_ext_src_cap");
    setReg(0x07, 0x04); // Flush RX
    return false;
  }
  return false;

// later on, after filling out the dev_library, add code to do device recognition and be able to update dev_type

}
bool read_dis_idt_response(){ // reads dis idt msg for vid & pid, compares them with those in "dev_library", prints them
  uint8_t num_data_objects;
  uint8_t message_type;
  uint8_t command;
  uint8_t cmd_type;
  uint16_t VID;
  uint16_t PID;
  
  receiveBytes(rx_buf, 1); // captures preamble
  if (getReg(0x41) & 0x20) {
    Serial1.println("empty rx_fifo - read_dis_idt_response");
    return false;
  }
  receiveBytes(rx_buf, 2); // captures header
  num_data_objects = ((rx_buf[1] & 0x70) >> 4);
  message_type     = (rx_buf[0] & 0x1F);
  if(message_type == 15){
    Serial1.println("VDM received");
    receiveBytes(rx_buf, 2); // captures vdm header
    command = rx_buf[0] & 0x1F; // dis idt
    cmd_type = (rx_buf[0] & 0xC0)>>6;  // req/ack/nack/busy
    if((command==1) & (cmd_type==1)){
      receiveBytes(rx_buf, num_data_objects*4); // captures vdm content
      VID = (rx_buf[1]<<8) | rx_buf[0];
      PID = (rx_buf[11]<<8) | rx_buf[10];
      Serial1.print("Device VID: ");
      Serial1.println(VID, HEX);
      Serial1.print("Device PID: ");
      Serial1.println(PID, HEX);
    }else{
      if((command==1)&&(message_type==0xF)){
        Serial1.println("VDM request NACK");
        return false;
      }
      Serial1.print("command: ");
      Serial1.println(command, DEC);
      Serial1.print("msg type: ");
      Serial1.println(message_type, BIN);
      return false;
    }
  }else{
    Serial1.println("wrong type of message received - read_dis_idt_response");
    return false;
  }
  receiveBytes(rx_buf, 4); // captures crc-32
  return true;

// later on, after filling out the dev_library, add code to do device recognition and be able to update dev_type
  return true;
}

void get_src_cap(){
  Serial1.println("fetching src cap info...");
  sendPacket( 0, 0, msg_id, 0, spec_revs[0]-1, 0, 0x7, NULL );
  //setReg(0x03, 0x26);
  while ( getReg(0x41) & 0x20 ) {} // while RX_empty, wait
  receivePacket();
  while ( getReg(0x41) & 0x20 ) {} // while RX_empty, wait
  read_pdo();
}
bool sel_src_cap(int volts, int amps){
  uint8_t temp_buf[80] = {};
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
    //Serial1.println("requesting voltage & current...");
    uint32_t request_msg = (options_pos[idx]<<28) | ((amps*100)<<10) | (amp_options[idx]);
    temp_buf[0] = request_msg & 0xFF;
    temp_buf[1] = (request_msg>>8) & 0xFF;
    temp_buf[2] = (request_msg>>16) & 0xFF;
    temp_buf[3] = (request_msg>>24) & 0xFF;
    //Serial1.print("requested obj pos: ");
    //Serial1.println(options_pos[idx]);
    sendPacket( 0, 1, msg_id, 0, spec_revs[0]-1, 0, 0x2, temp_buf );
    Serial1.println("voltage and current requested from src");

    unsigned long time = millis();
    while ((getReg(0x41) & 0x20) && (millis()<(time+500))) {} // while RX_empty, wait for goodcrc
    receivePacket();
    while ((getReg(0x41) & 0x20) && (millis()<(time+500))) {}  // while RX_empty, wait for accept
    get_req_outcome(); 
    while ((getReg(0x41) & 0x20) && (millis()<(time+500))) {}  // while RX_empty, wait for ps_ready
    if(get_req_outcome()){
      return true;
    }
  }else if(!possible_v){
    Serial1.println("voltage not available");
    return false;
  }else{
    Serial1.println("amp request too high for your voltage");
    return false;
  }
  return false;
}
void send_snk_cap(int volts, int amps){
  if(volts==5){
    uint32_t sink_cap_msg = (0<<29) | (1<<26) | (0<<25)| ((5*20)<<10) | (amps*100);  // DR power = 0, higher cap & unconstrained power = 0, comm cap = 1, DR data = 0
    temp_buf[0] = sink_cap_msg & 0xFF;
    temp_buf[1] = (sink_cap_msg>>8) & 0xFF;
    temp_buf[2] = (sink_cap_msg>>16) & 0xFF;
    temp_buf[3] = (sink_cap_msg>>24) & 0xFF;
    sendPacket( 0, 1, msg_id, 0, spec_revs[0]-1, 0, 0x4, temp_buf );
    Serial1.println("sink cap sent (5v only)");
  }else if(volts>5){ // 1st msg bytes 3-0, 2nd msg bytes 7-4
    uint32_t sink_cap_msg_p1 = (0<<29)|(1<<28)|(1<<26)|(0<<25)|((5*20)<<10)|(3*100); // first PDO
    uint32_t sink_cap_msg_p2 = ((volts*20)<<10)|(amps*100); // second PDO, everything after 1st 20 bits are blank
    temp_buf[0] = sink_cap_msg_p1 & 0xFF;
    temp_buf[1] = (sink_cap_msg_p1>>8) & 0xFF;
    temp_buf[2] = (sink_cap_msg_p1>>16) & 0xFF;
    temp_buf[3] = (sink_cap_msg_p1>>24) & 0xFF;
    temp_buf[4] = (sink_cap_msg_p2) & 0xFF;
    temp_buf[5] = (sink_cap_msg_p2>>8) & 0xFF;
    temp_buf[6] = (sink_cap_msg_p2>>16) & 0xFF;
    temp_buf[7] = (sink_cap_msg_p2>>24) & 0xFF;
    sendPacket( 0, 2, msg_id, 0, spec_revs[0]-1, 0, 0x4, temp_buf );
    Serial1.println("sink cap sent (higher cap than 5v)");
  }
}
void send_dis_idt_request(){
  uint8_t temp_buf[80];
  uint16_t dev_PD_SID = 0xFF00; // SVID field of VDM header must be set to this for dis idt cmd

  uint32_t vdm_header = (dev_PD_SID<<16) | (1<<15) | (0<<13) | (0<<6) | (1); // REQ for Dis Idt
  temp_buf[0] = vdm_header & 0xFF;
  temp_buf[1] = (vdm_header>>8) & 0xFF;
  temp_buf[2] = (vdm_header>>16) & 0xFF;
  temp_buf[3] = (vdm_header>>24) & 0xFF;
  sendPacket( 0, 1, msg_id, 0, spec_revs[0]-1, 0, 0xF, temp_buf );
  Serial1.println("fetching discovery identity info...");
}
void send_dis_idt_response(){ // sends a response to vdm discovery identity request
  uint16_t dev_VID = 0x0483; // VID given to us from Intel Corp
  uint16_t dev_PID = 0x1307; // PID given to us from Intel Corp
  uint16_t dev_PD_SID = 0xFF00; // SVID field of VDM header must be set to this for dis idt cmd

  uint32_t vdm_header = (dev_PD_SID<<16) | (1<<15) | (1<<13) | (1<<6) | (1);
  uint32_t id_header = (1<<30) | (2<<27) | (2<<21) | (dev_VID);
  uint32_t cert_stat = 0; // empty XID
  uint32_t product_vdo = (dev_PID<<16) | (0); // empty BCDdevice code
  uint32_t UFP_type_vdo = (3<<29) | (4<<24) | (1); // reported as USB 3.2 capable with gen 1 superspeed support

  temp_buf[0] = vdm_header & 0xFF;
  temp_buf[1] = (vdm_header>>8) & 0xFF;
  temp_buf[2] = (vdm_header>>16) & 0xFF;
  temp_buf[3] = (vdm_header>>24) & 0xFF;

  temp_buf[4] = (id_header) & 0xFF;
  temp_buf[5] = (id_header>>8) & 0xFF;
  temp_buf[6] = (id_header>>16) & 0xFF;
  temp_buf[7] = (id_header>>24) & 0xFF;

  temp_buf[8] = (cert_stat) & 0xFF;
  temp_buf[9] = (cert_stat>>8) & 0xFF;
  temp_buf[10] = (cert_stat>>16) & 0xFF;
  temp_buf[11] = (cert_stat>>24) & 0xFF;

  temp_buf[12] = (product_vdo) & 0xFF;
  temp_buf[13] = (product_vdo>>8) & 0xFF;
  temp_buf[14] = (product_vdo>>16) & 0xFF;
  temp_buf[15] = (product_vdo>>24) & 0xFF;

  temp_buf[16] = (UFP_type_vdo) & 0xFF;
  temp_buf[17] = (UFP_type_vdo>>8) & 0xFF;
  temp_buf[18] = (UFP_type_vdo>>16) & 0xFF;
  temp_buf[19] = (UFP_type_vdo>>24) & 0xFF;

  sendPacket( 0, 5, msg_id, 0, spec_revs[0]-1, 0, 0xF, temp_buf );
  Serial1.println("discovery identity response sent");
}
void send_dis_svid_response(){ // sends a response to vdm discovery svid request
  uint16_t dev_VID = 0x0483; // VID given to us from Intel Corp
  uint16_t dev_PID = 0x1307; // PID given to us from Intel Corp
  uint16_t dev_PD_SID = 0xFF00; // SVID field of VDM header must be set to this for dis idt cmd

  uint32_t vdm_header = (dev_PD_SID<<16) | (1<<15) | (1<<13) | (2<<6) | (2);

  temp_buf[0] = vdm_header & 0xFF;
  temp_buf[1] = (vdm_header>>8) & 0xFF;
  temp_buf[2] = (vdm_header>>16) & 0xFF;
  temp_buf[3] = (vdm_header>>24) & 0xFF;

  sendPacket( 0, 1, msg_id, 0, spec_revs[0]-1, 0, 0xF, temp_buf );
  Serial1.println("discovery svid response sent");
}
void send_ext_src_cap(){
  uint16_t dev_VID = 0x0483; // VID given to us from Intel Corp
  uint16_t dev_PID = 0x1307; // PID given to us from Intel Corp

  temp_buf[0] = 0; // ext msg header
  temp_buf[1] = 0x19; // data size (25 bytes in SCEDB)

  temp_buf[2] = dev_VID & 0xFF;
  temp_buf[3] = (dev_VID>>8) & 0xFF;
  temp_buf[4] = (dev_PID) & 0xFF;
  temp_buf[5] = (dev_PID>>8) & 0xFF; // rest of SCEDB is left blank

  temp_buf[10] = 0xFF; // firmware version number (bs)
  temp_buf[11] = 0xFF; // hardware version number (bs)
  temp_buf[13] = 0x3; // report 3ms for holdup time bc they said "should" (bs)
  temp_buf[15] = 0x7; // leakage is minimal, ground pin exists, ground pin hooked to protective earth (bs)

  temp_buf[25] = 0x2E; // PDP rating = 46 (bs)

  sendPacket( 1, 0, msg_id, 0, spec_revs[0]-1, 0, 0x1, temp_buf );  // when chunked=0 & extended=1, num_data_objs is reserved
  Serial1.println("ext src cap response sent");
}
void get_spec_rev(){   // saves revision majors and minors
  sendPacket( 0, 0, msg_id, 0, spec_revs[0]-1, 0, 0x18, NULL );
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
bool read_rest(int volts, int amps){ // recursion to read out rest of trailing messages (vendor defined, sink_cap_req, etc)
  temp_buf[80] = {};
  rx_buf[80] = {};
  uint8_t message_type;
  uint8_t num_data_objects;
  bool extended;

  receiveBytes(rx_buf, 1);
  if (getReg(0x41) & 0x20) {  // if rx_fifo empty
    Serial1.println("no more trailing messages - read rest");
    return true;
  }
  receiveBytes(rx_buf, 2);
  num_data_objects = ((rx_buf[1] & 0x70) >> 4);
  if(spec_revs[0]==3){
    message_type = (rx_buf[0] & 0x1F);
  }else{
    message_type = (rx_buf[0] & 0x0F);
  }
  extended     = (rx_buf[1] >> 7);
  if((num_data_objects==0) & (message_type == 0x8)){ // sink cap requested
    Serial1.println("sink cap requested - read rest");
    receiveBytes(rx_buf, 4); // clear crc-32
    send_snk_cap(volts,amps);
    unsigned long time = millis();
    while ( (getReg(0x41) & 0x20) && (millis()<(time+300)) ) {} // while RX_empty, wait for goodcrc
    receivePacket();
    while ( (getReg(0x41) & 0x20) && (millis()<(time+3300)) ) {} // while RX_empty
    read_rest(volts,amps);
    return true;
  }else if((num_data_objects>0) & (message_type == 0x1)){ // src cap received
    Serial1.println("source capabilities message received - read rest");
    receiveBytes(rx_buf, (num_data_objects*4));
    int index = 0;
    while(!int_flag){}
    for (uint8_t i=0; i<num_data_objects; i++) {
      uint32_t byte1 = rx_buf[0 + i*4];
      uint32_t byte2 = rx_buf[1 + i*4]<<8;
      uint32_t byte3 = rx_buf[2 + i*4]<<16;
      uint32_t byte4 = rx_buf[3 + i*4]<<24;
      int32_t pdo = byte1|byte2|byte3|byte4;
      if((pdo>>30)==0){ // fixed supply
          volt_options[index] = ((pdo>>10) & 0x3FF)/20;   // Converted to 1 volt units
          amp_options[index] = ((pdo & 0x3FF));   // Left in 10mA units
          options_pos[index] = i+1; // 0001 is always safe5v
          index++;
          break;
      }
    }
    receiveBytes(rx_buf, 4); // CRC-32  
    sel_src_cap(volts, amps);
    unsigned long time = millis();
    while ( (getReg(0x41) & 0x20) && (millis()<(time+3300)) ) {} // while RX_empty
    read_rest(volts, amps);
    return true;
  }else if((num_data_objects>0) & (message_type == 0xF)){ // vdm received (discover identity or discover svid)
    receiveBytes(rx_buf, (num_data_objects*4));
    uint8_t command = rx_buf[0] & 0x1F;
    uint8_t cmd_type = rx_buf[0] >> 6;
    receiveBytes(rx_buf, 4); // CRC-32

    if((command==1) & (cmd_type==0)){
      Serial1.println("discovery identity request vdm - read rest");
      send_dis_idt_response();
      unsigned long time = millis();
      while ( (getReg(0x41) & 0x20) && (millis()<(time+300)) ) {}
      receivePacket();
      while ( (getReg(0x41) & 0x20) && (millis()<(time+500)) ) {}
      read_rest(volts, amps);
      return true;
    }else if((command==2) & (cmd_type==0)){
      Serial1.println("discovery svid request vdm - read rest");
      send_dis_svid_response();
      unsigned long time = millis();
      while ( (getReg(0x41) & 0x20) && (millis()<(time+300)) ) {}
      receivePacket();
      while ( (getReg(0x41) & 0x20) && (millis()<(time+500)) ) {}
      read_rest(volts, amps);
      return true;
    }
  }else if((num_data_objects==0) & (message_type == 0x7)){ // src cap requested
    receiveBytes(rx_buf, 4); // CRC-32
    Serial1.println("src cap requested from src");
    sendPacket( 0, 0, msg_id, 0, spec_revs[0]-1, 0, 0x10, NULL );  // rejected: 0x4
    Serial1.println("replied with 'not supported'");
    unsigned long time = millis();
    while ( (getReg(0x41) & 0x20) && (millis()<(time+300)) ) {}
    receivePacket();
    read_rest(volts, amps);
    return true;
  }else if((num_data_objects==0) & (message_type == 0x11)){ // extended src cap requested
    receiveBytes(rx_buf, 4); // CRC-32
    // Serial1.println("ext src cap request ignored"); // commented out to increase speed here
    // send_ext_src_cap();
    // unsigned long time = millis();
    // while ( (getReg(0x41) & 0x20) && (millis()<(time+300)) ) {}
    // receivePacket();
    read_rest(volts, amps);
    return true;
  }else{
    Serial1.println("misc msg detected - read rest");
    Serial1.print("extended: ");
    Serial1.println(extended);
    Serial1.print("num_data_objects: ");
    Serial1.println(num_data_objects);
    Serial1.print("message_type: ");
    Serial1.println(message_type);
    receiveBytes(rx_buf, 4); // crc-32
    unsigned long time = millis();
    while ( (getReg(0x41) & 0x20) && (millis()<(time+3300)) ) {}
    read_rest(volts,amps);
    return true;
  }
  return true;
}

void reset_fusb(){
  setReg(0x0C, 0x01); // Reset FUSB302
  setReg(0x0B, 0x0F); // FULL POWER!  
  setReg(0x07, 0x04); // Flush RX
  setReg(0x06, 0x00); // disable all interrupt masks
  setReg(0x0A, 0x77); // mask all interrupts except for vbusok
  //setReg(0x3E, 0xBF); // mask all of interrupta reg except for i_togdone
  //setReg(0x3F, 0x1); // mask interruptb reg (goodcrc sent)    Masking these commented out interrupts leads to init never triggering...
  setReg(0x02, 0x03); // enable both pull downs (enables attach detection)
  setReg(0x03, 0x20); // turn off auto goodcrc and set power and data roles to SNK
}
void check_interrupt(){
  u_int8_t i_reg = getReg(0x42); // interrupts reg
  bool i_alert = i_reg & 0x8;
  bool i_vbusok = i_reg>>7;
  bool vbusok = getReg(0x40)>>7; // from status0
  if(i_vbusok){
    if(vbusok){
      if(!attached){
        new_attach = 1;
        Serial1.println("NEW ATTACH");
      }else{
        new_attach = 0;
      }
      attached = 1;
    }else{
      attached = 0;
      Serial1.println("DETACHED");
    }
  }else if(i_alert){
    Serial1.println("tx_full or rx_full, i_alert");
  }else{
    Serial1.println("interrupt triggered but i_vbusok=0 and i_alert=0");
    if(attached){
      new_attach = 0;
    }
  }
}
void orient_cc(){ // figure out which cc pin is vconn and which is cc (must be called before init)
  setReg(0x02, 0x07); // measure cc1
  unsigned long time = millis();
  while(millis()<(time+150)) {}
  meas_cc1 = getReg(0x40)&3;
  Serial1.print("bc lvl after meas_cc1: ");
  Serial1.println(meas_cc1, BIN);

  setReg(0x02, 0x0B); // switch to measuring cc2
  time = millis();
  while(millis()<(time+150)) {}
  meas_cc2 = getReg(0x40)&3;
  Serial1.print("bc lvl after meas_cc2: ");
  Serial1.println(meas_cc2, BIN);

  if(meas_cc1>meas_cc2){
    cc_line = 1;
    vconn_line = 2;
  }else{
    cc_line = 2;
    vconn_line = 1;
  }
}
void enable_tx_cc(int cc, bool autocrc){
  if(cc == 1){
    setReg(0x02, 0x07); // Switch on MEAS_CC1
    setReg(0x07, 0x04); // Flush RX
    if(autocrc){
      setReg(0x03, 0x25); // Enable BMC Tx on CC1, autocrc ON (25) OFF (21)
    }else{
      setReg(0x03, 0x21);
    }
  }else if(cc == 2){
    setReg(0x02, 0x0B); // Switch on MEAS_CC2
    setReg(0x07, 0x04); // Flush RX
    if(autocrc){
      setReg(0x03, 0x26); // Enable BMC Tx on CC2, autocrc ON (26) OFF (22)
    }else{
      setReg(0x03, 0x22);
    }
  }
}
bool pd_init(int volts, int amps){  // turns on autocrc after setup disables it
  // orient_cc();
  // enable_tx_cc(cc_line, 1);

  while(getReg(0x41) & 0x20){} // while fifo rx is empty
  read_pdo();
  unsigned long time = millis();
  if(sel_src_cap(volts, amps)){
    while((getReg(0x41) & 0x20) && (millis()<(time+3300))) {} 
    read_rest(volts,amps);
  }

  return true;
}
bool reneg_pd(int volts, int amps){ // renegotiates power after init's power negotiation
  setReg(0x07, 0x04); // Flush RX
  get_src_cap();
  sel_src_cap(volts, amps);
  return true;
}
void recog_dev(int volts, int amps){ // fetches vid & pid from extended src cap, {compares them with those in "dev_library," updates dev_type int var}
  Serial1.println("(Spec Rev 3)");
  spec_revs[0] = 3; // we temporarily switch to spec rev 3
  pd_init(volts, amps);

  sendPacket( 0, 0, msg_id, 0, spec_revs[0]-1, 0, 0x11, NULL ); // ask for ext src cap
  Serial1.println("requested for extended src cap");
  unsigned long time = millis();
  while ( (getReg(0x41) & 0x20) && (millis()<(time+500)) ) {} // while RX_empty, wait
  receivePacket(); // for goodcrc
  while ( (getReg(0x41) & 0x20) && (millis()<(time+500)) ) {} // while RX_empty, wait
  if(read_ext_src_cap()){ // ext src cap way
    Serial1.println("vid & pid registered successfully");
  }else{
    Serial1.println("vid & pid detection failed, defaulting dev type to --> charger");
    dev_type = 0;
  }

  setReg(0x09, 0x40);
  //sendPacket( 0, 0, msg_id, 0, spec_revs[0]-1, 0, 0xD, NULL );
  Serial1.println("hard reset sent");
  // time = millis();
  // while ( (getReg(0x41) & 0x20) && (millis()<(time+500)) ) {} // while RX_empty, wait
  // receivePacket(); // for goodcrc
  // while ((getReg(0x41) & 0x20) && (millis()<(time+500))) {}  // while RX_empty, wait for accept
  // get_req_outcome(); // for accept
  spec_revs[0] = 2;
  Serial1.println("(Spec Rev 2)");
}

void InterruptFlagger(uint gpio, uint32_t events){
  int_flag = 1;
}

void setup() {
}
void setup1() {
  Serial1.begin(115200);
  Wire.begin();
  pinMode(6, INPUT_PULLUP); // interrupt pin from fusb
  gpio_set_irq_enabled_with_callback(6, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &InterruptFlagger); // setup interrupt pin + handler function

  delay(150);

  reset_fusb();
}

void loop() {
}
void loop1() {
  if(int_flag){
    check_interrupt(); // processing attach / detach
    if(attached & new_attach){
      orient_cc();
      enable_tx_cc(cc_line, 1);
      recog_dev(5, 0.5);

      reset_fusb();
      enable_tx_cc(cc_line, 1);
      pd_init(5, 0.5);


      Serial1.println("-------------------------------------");

      // delay(5000);
      // reneg_pd(20, 1);
    }else if(attached == 0){
      reset_fusb();
    }
    int_flag = 0;
  }
}
