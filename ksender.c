#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

msg createSPack() {
	S_PACKAGE data;
	MINI_KERMIT kermit;
	msg result;

	data.maxl = MAXL;
	data.time = TIME;
	data.npad = 0x00;
	data.padc = 0x00;
	data.eol = 0x0D;
	data.qctl = 0x00;
	data.qbin = 0x00;
	data.chkt = 0x00;
	data.rept = 0x00;
	data.capa = 0x00;
	data.r = 0x00;

	kermit.soh = 0x01;
	kermit.len = sizeof(MINI_KERMIT) - 2;
	kermit.seq = 0x00;
	kermit.type = 'S';
	kermit.mark = data.eol;

	memset(kermit.data, 0, sizeof(kermit.data));
	memcpy(kermit.data, &data, sizeof(S_PACKAGE));

	unsigned short crc = crc16_ccitt(&kermit, sizeof(MINI_KERMIT) - 3);
	kermit.check = crc;
	memset(result.payload, 0, sizeof(result.payload));
	memcpy(result.payload, &kermit, sizeof(kermit));
	result.len = sizeof(kermit);

	return result;
}

void updateCRC(msg *send) {
	unsigned short crc = crc16_ccitt(send->payload, sizeof(MINI_KERMIT) - 3);
	memcpy(send->payload + DATASTART + MAXL, &crc, sizeof(crc));
}

void updateSEQ(msg *send, msg *rcv) {
	rcv->payload[SEQPOS] %= 64;
	send->payload[SEQPOS] = rcv->payload[SEQPOS] + 1;
	send->payload[SEQPOS] %= 64;
}

int sendMsg(msg *send, msg *rcv, char *type, int number) {
	int countTIMEOUT = 0;
	while(1) {
		if (countTIMEOUT == MAXSEND) {
			printf("Ending transmision. Sent enough for today.\n");
			return -1;
		}

		updateCRC(send);

		send_message(send);
		
		if (number == -1)
			printf("[%u] %s\n", send->payload[SEQPOS], type);
		else
			printf("[%u] %s %d\n", send->payload[SEQPOS], type, number);

		msg *y = receive_message_timeout(TIME * 1000);
			
		if (y == NULL) {
			printf("Sender hasn't received in time!\n");
			countTIMEOUT++;
			continue;
		}

		else if (y != NULL) {
			memcpy(rcv, y, sizeof(msg));
				
			if (rcv->payload[TYPEPOS] == 'N') {
				updateSEQ(send, rcv);
				countTIMEOUT = 0;
				continue;
			}

			else if (rcv->payload[TYPEPOS] == 'Y')
				countTIMEOUT = 0;
				break;
		}        
	}
	return 0;
}

int sendData(int fd, msg *send, msg *rcv) {
	int count = 1;

	while (1) {
		updateSEQ(send, rcv);
		memset(send->payload + DATASTART, 0, MAXL);
		char buff[MAXL];
		int reading = read(fd, buff, MAXL);
			
		if (reading == 0) {
			close(fd);
			break;
		}

		else if (reading > 0 && reading < MAXL) {
			for (int j = 1; j < reading + 1; j++) {
				send->payload[DATASTART + j - 1] = buff[j - 1];
			}
			send->len = reading;
		}
			
		else if (reading == MAXL) {
			memcpy(send->payload + DATASTART, buff, MAXL);
			send->len = MAXL;
		}

		if (!sendMsg(send, rcv, "Data", count))
			count++;
		else
			return -1;
	}
	return 0;
}

int main(int argc, char** argv) {
	init(HOST,PORT);
	msg send, rcv;

	msg eot = createSPack();
	memset(eot.payload + DATASTART, 0, MAXL);
	eot.payload[TYPEPOS] = 'B';

	for (int i = 1; i < argc; i++) {
		send = createSPack();
		if (sendMsg(&send, &rcv, "Send-init", -1))
			return -1; 

		updateSEQ(&send, &rcv);
		
		//Create Package F
		send.payload[TYPEPOS] = 'F';
		memset(send.payload + DATASTART, 0, MAXL);
		memcpy(send.payload + DATASTART, argv[i], strlen(argv[i]));
		send.len = strlen(argv[i]);

		if (sendMsg(&send, &rcv, "File Header", i)) 
			return -1;

		int fd = open(argv[i], O_RDONLY);
		
		if (fd < 0) {
			printf("Error opening file %d.\n", i);
			return -1;
		}

		//Create Package D
		updateSEQ(&send, &rcv);
		send.payload[TYPEPOS] = 'D';
		memset(send.payload + DATASTART, 0, MAXL);

		if (sendData(fd, &send, &rcv))
			return -1;
		
		//Create Z Package
		updateSEQ(&send, &rcv);
		memset(send.payload + DATASTART, 0, MAXL);
		send.payload[TYPEPOS] = 'Z';
		
		if (sendMsg(&send, &rcv, "EOF", -1))
			return -1; 

	}

	updateSEQ(&eot, &rcv);
	if (sendMsg(&eot, &rcv, "EOT", -1))
		return -1;
	else {
		printf("[SENDER]: Received confirmation for ending transmision. ");
		printf("Shutting down properly.\n");
	}
}