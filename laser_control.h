#ifndef _LASER_CONTROL_H
#define _LASER_CONTROL_H
#include <stdint.h>


//Header Structure - 1 btye 
//2 bit command
//4 bit adc #
//1 bit flag
//1 bit parity


//Packet structure
//1 byte size
//252 byte payload
//2 byte CRC_16

//Commands
//00 Disconnect
//01 Connect
//10 Ack
//11 Nack



uint8_t send_disconnect();

uint8_t send_connect(uint8_t adc_number);

uint8_t send_ack(uint8_t adc_number);

uint8_t send_nack(uint8_t adc_number);

uint8_t parse_signal(uint8_t signal_in); 

uint8_t send_packet();


uint8_t parse_packet();




int calculate_parity(uint8_t byte);

uint16_t calc_crc(uint16_t seed, uint8_t *buf, int len);

#endif

