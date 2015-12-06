
#include "laser_control.h"
#include <stdio.h>


int main(int argc, char const *argv[])
{

	int command = send_disconnect();
	printf("Disconnect: 0x%X\n",command);

	int i;
	for (i = 0; i < 9; ++i){
		command = send_connect(i);
		printf("Connect via %d: 0x%X\n",i, command);
	}

	for (i = 0; i < 9; ++i){
		command = send_nack(i);
		printf("Nack via %d: 0x%X\n",i, command);
	}

	for (i = 0; i < 9; ++i){
		command = send_ack(i);
		printf("Ack via %d: 0x%X\n",i, command);
	}
	

	return 0;
}