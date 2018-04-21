#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

unsigned short getShort(char* array, int offset) {
    return (short)(((short)array[offset + 1]) << 8) | ((unsigned char) array[offset]);
}

int main(int argc, char** argv) {
  init(HOST,PORT);
  
  msg send, rcv, *y;
  char *rez = "recv_";
  unsigned short crc;
  int fd, count = 1;
  send.payload[SEQPOS] = -1;
  rcv.payload[SEQPOS] = -1;


	while(1) {
		if (count == 1) {
			y = receive_message_timeout(2 * TIME * 1000);
			count++;
		}
		else
			y = receive_message_timeout(TIME * 1000);			
		
		if (y != NULL) {
			if (y->payload[SEQPOS] == (rcv.payload[SEQPOS] % 64))
				continue;
			memcpy(&rcv, y, sizeof(msg));
			memcpy(&send, y, sizeof(msg));
		
			crc = crc16_ccitt(rcv.payload, sizeof(MINI_KERMIT) - 3);
			unsigned short receivedCRC = getShort(rcv.payload, DATASTART + MAXL);
			
			rcv.payload[SEQPOS] %= 64;
			send.payload[SEQPOS] = rcv.payload[SEQPOS] + 1;
			send.payload[SEQPOS] %= 64;
			
			if (crc == receivedCRC) { 
				send.payload[TYPEPOS] = 'Y';

				if (rcv.payload[TYPEPOS] == 'S') {
					send_message(&send);
					printf("[%u] ACK for [%u]\n\n", send.payload[SEQPOS],
						rcv.payload[SEQPOS]);
				}

				else if (rcv.payload[TYPEPOS] == 'F') {
					char *file = malloc(sizeof(char) * (rcv.len + strlen(rez)));
					strcpy(file, rez);
					strcat(file, rcv.payload + DATASTART);

					fd = open(file, O_WRONLY | O_CREAT);

					memset(send.payload + DATASTART, 0, MAXL);
					send_message(&send);
					printf("[%u] ACK for [%u]\n\n", send.payload[SEQPOS],
						rcv.payload[SEQPOS]);
					free(file);
				}

				else if (rcv.payload[TYPEPOS] == 'D') {
					memset(send.payload + DATASTART, 0, MAXL);
					write(fd, rcv.payload + DATASTART, rcv.len);
					send_message(&send);
					printf("[%u] ACK for [%u]\n\n", send.payload[SEQPOS],
						rcv.payload[SEQPOS]);
				}

				else if (rcv.payload[TYPEPOS] == 'Z') {
					memset(send.payload + DATASTART, 0, MAXL);
					close(fd);
					send_message(&send);
					printf("[%u] ACK for [%u]\n\n", send.payload[SEQPOS],
						rcv.payload[SEQPOS]);
					count = 1;
				}

				else if (rcv.payload[TYPEPOS] == 'B') {
					memset(send.payload + DATASTART, 0, MAXL);
					send_message(&send);
					printf("[%u] ACK for [%u]\n\n", send.payload[SEQPOS],
						rcv.payload[SEQPOS]);				
					printf("[RECEIVER]: Received request to end transmision. ");
					printf("Shutting down properly.\n");
					return 0;
				}
			}
			
			else if (crc != receivedCRC) {
				memset(send.payload + DATASTART, 0, MAXL);
				send.payload[TYPEPOS] = 'N';
				send_message(&send);
				printf("[%u] NAK for [%u]\n\n", send.payload[SEQPOS],
					rcv.payload[SEQPOS]);
			}
		}
		else if (y == NULL) {
			printf("Receiver hasn't received in time!\n");
			memset(send.payload + DATASTART, 0, MAXL);

			send.payload[TYPEPOS] = 'N';
			send.payload[SEQPOS] += 2;
			send.payload[SEQPOS] %= 64;
			rcv.payload[SEQPOS] += 2;
			rcv.payload[SEQPOS] %= 64;
			
			send_message(&send);
			printf("[%u] NAK for [%u]\n\n", send.payload[SEQPOS],
				rcv.payload[SEQPOS]);
		}
  	}
}