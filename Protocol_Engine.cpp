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
