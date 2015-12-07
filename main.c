#include "laser_control.h"
#include <stdio.h>


uint8_t* transmission_data = "We the People of the United States, in Order to form a more perfect Union, establish Justice, insure domestic Tranquility, provide for the common defence, promote the general Welfare, and secure the Blessings of Liberty to ourselves and our Posterity, do ordain and establish this Constitution for the United States of America.";



int main(int argc, char const *argv[])
{

	//Setup
	enum State state = disconnect;
	struct Command command;
	struct Packet packets[255];
	int packets_sent = 0;


	//Packetize
	int i;
	for(i = 0; transmission_data[i] != '\0'; i++){
		packets[i/PAYLOAD_SIZE].size = (i % PAYLOAD_SIZE) + 1; 
		packets[i/PAYLOAD_SIZE].payload[i % PAYLOAD_SIZE] = transmission_data[i];
	}

	i = i/PAYLOAD_SIZE;
	while(i >= 0) {
		packets[i].crc = calc_crc(-1, packets[i].payload, packets[i].size);
		i--;
	}

	//Uart Init
	//uartAx_init_9600baud(__MSP430_BASEADDRESS_EUSCI_A1__);


	//Loop
	for(;;){

		command = get_command();

		switch(state){

			case disconnect:
				if(command.cmd == CMD_CONNECT){
					state = send;
					send_packet(packets[packets_sent]);
				}
				else if(command.cmd != CMD_DISCONNECT){
					printf("Missed Connection Signal!!\n");
					state = send;
				}

			break;

			case send:

				if(command.cmd == CMD_DISCONNECT)
					state = disconnect;
				
				else {

					//Next Packet
					if(command.cmd == CMD_ACK)
						packets_sent++;
					
					//Send
					send_packet(packets[packets_sent]);
				
				}
			
			break;
		}

	}
	
	return 42; //LOOP FAIILURE
}

