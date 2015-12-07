#include "laser_control.h"


uint8_t send_disconnect(){

	uint8_t command = 0x00;

	//Send Xbee singal 

	return command;
}

uint8_t send_connect(uint8_t adc_number){
	uint8_t command = (0x1 << 6);

	command += (adc_number << 2);
	command += (calculate_parity(command) & 0x1);

	//Send Xbee signal

	return command;

}

uint8_t send_nack(uint8_t adc_number){

	uint8_t command = (0x2 << 6);

	command += (adc_number << 2);
	command += (0x0 << 1); // Set flag ----off for now!!!!!
	command += (calculate_parity(command) & 0x1);

	//Send Xbee signal

	return command;
}

uint8_t send_ack(uint8_t adc_number){
	uint8_t command = (0x3 << 6);

	command += (adc_number << 2);
	command += (0x1 << 1); // Set flag
	command += (calculate_parity(command) & 0x1);

	//Send Xbee signal

	return command;
}


struct Command get_command(){

	struct Command c;

	//GET XBEE
	uint8_t signal_in = 0x0;

	if(calculate_parity(signal_in) == (signal_in & 0x1)){
		c.cmd = (signal_in >> 6) & 0x2;
		c.adc = (signal_in >> 2) & 0x4;
		c.flag =  (signal_in >> 1) & 0x1;
	}

	
	return c;
}

void send_packet(struct Packet p){
	

}



//Calculates the even parity of a byte in place
int calculate_parity(uint8_t byte){
	int parity = 0;
	int i;
	for (i = 1; i < 8; ++i){
		parity+= ((byte >> i) & 0x1);
	}
	return parity % 2;
}



uint16_t calc_crc(uint16_t seed, uint8_t *buf, int len)
{
  uint16_t crc;
  uint8_t *p;
  crc = seed;
  p = buf;
  while (len--) {
    crc = crc_table[((crc >> 8) ^ *p++) & 0xff] ^ (crc << 8);
  }
  return (uint16_t)crc;
}

