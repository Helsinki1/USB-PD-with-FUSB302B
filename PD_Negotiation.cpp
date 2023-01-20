#include <Arduino.h>
#include <Wire.h>
#include "Protocol_Engine.cpp"

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
    Serial1.println("Expected RMDO, incorrect packet type received");
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


void get_spec_rev(){   // returns revision major
  sendPacket( 0, msg_id, 0, spec_revs[0]-1, 0, 0x18, NULL );
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


bool pd_init(int volts, int amps){
  setReg(0x0C, 0x01); // Reset FUSB302
  setReg(0x0B, 0x0F); // FULL POWER!  
  setReg(0x07, 0x04); // Flush RX
  setReg(0x02, 0x0B); // Switch on MEAS_CC2
  //setReg(0x02, 0x07); // Switch on MEAS_CC1
  setReg(0x03, 0x26); // Enable BMC Tx on CC2
  //setReg(0x03, 0x25); // Enable BMC Tx on CC2
  readAllRegs();

  get_src_cap();
  sel_src_cap(volts, amps);

  get_spec_rev();

  return true;
}



void setup() {
  // put your setup code here, to run once:
  Serial1.begin(115200);
  Wire.begin();
  delay(1250);

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
