#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#define DEBUG 1
#define debug_print(...) \
            do { if (DEBUG) printf(__VA_ARGS__); } while (0)

#define TRANSMITTER 0
#define RECEIVER 1

#define TIMEOUT 1
#define MAX_TRIES 20
#define BAUDRATE B9600

#define F 0x7e // Flag
#define A 0x03 // Campo de Endereço
#define C_SET 0x07 // Comando SET
#define C_UA 0x03 // Resposta UA
#define C_DISC 0xb
#define ESCAPE 0x7d
#define ESCAPE_BYTE(byte) (byte ^ 0x20)
#define N(s) (s << 5)
#define RR(s) ((s << 5) | 1)
#define REJ(s) ((s << 5) | 5)


#define MAX_SIZE 1024

#define SERIAL_PATH "/dev/ttyS%d"

struct {
    char port[20]; /*Dispositivo /dev/ttySx, x = 0, 1*/
    unsigned int sequenceNumber; /*Número de sequência da trama: 0,1 */
    bool timeout; /* indica se occoreu timeout */
	struct termios oldtio;
	bool disconnected;
	int oflag;
	int closed;
} linkLayer;

int llopen(int port, int oflag);
int llwrite(int fd, char* buffer, int length);
int llread(int fd, char* buffer);
int llclose(int fd);