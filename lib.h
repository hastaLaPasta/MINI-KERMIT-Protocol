#ifndef LIB
#define LIB

#define MAXL 250
#define TIME 5
#define MAXSEND 3

#define SEQPOS 2
#define TYPEPOS 3
#define DATASTART 4

typedef struct {
    int len;
    char payload[1400];
} msg;

typedef struct __attribute__((__packed__)) {
	char soh;
	char len;
	char seq;
	char type;
	char data[MAXL];
	unsigned short check;
	char mark;
} MINI_KERMIT;

typedef struct __attribute__((__packed__)){
	char maxl;
	char time;
	char npad;
	char padc;
	char eol;
	char qctl;
	char qbin;
	char chkt;
	char rept;
	char capa;
	char r;
} S_PACKAGE;

void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

#endif

