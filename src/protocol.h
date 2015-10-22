#pragma once

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
#include <time.h>

#define DEBUG 0
#define debug_print(...) \
            do { if (DEBUG) printf(__VA_ARGS__); } while (0)

#define TRANSMITTER 0
#define RECEIVER 1

#define F 0x7e // Flag
#define A 0x03 // Campo de Endereço
#define C_SET 0x07 // Comando SET
#define C_UA 0x03 // Resposta UA
#define C_DISC 0xb
#define ESCAPE 0x7d
#define ESCAPE_BYTE(byte) (byte ^ 0x20)
#define N(s) (s << 5)
#define RR(r) ((r << 5) | 1)
#define REJ(r) ((r << 5) | 5)

#define SIMULATE_ERRORS 1

#define SERIAL_PATH "/dev/ttyS%d"

// receive_frame errors 
#define READ_ERROR -1
#define UNEXPECTED_N -2
#define LLCLOSE_FAILED -3
#define TIMEOUT_FAIL -4
#define REJECTED -5

// llopen errors
#define OPEN_FAILED -1         
#define SERIAL_SETUP_FAILED -2   
#define SEND_SET_FAILED -3
//#define TIMEOUT_FAIL -4
#define DISCONNECTED -5

// llwrite errors
#define MALLOC_FAILED -1
//#define TIMEOUT_FAIL -4

struct{
    char port[20]; // Dispositivo /dev/ttySx, x = 0, 1
    unsigned int sequenceNumber; // Número de sequência da trama: 0,1 
    volatile bool timeout; // indica se occoreu timeout
	struct termios oldtio; // configuração anterior
	bool disconnected; // Indica se a ligação foi desligada
	int oflag; // Transmissor ou receptor
	int closed; // Indica se a porta série foi fechada
	unsigned int baudrate; // Baudrate usado (valor de configuração de acordo com o header termios)
	unsigned int data_length; // Número de bytes de dados (antes do stuffing) a enviar por pacote de dados
	unsigned int max_retries; // Número máximo de tentativas em caso de falha
	unsigned int timeout_interval; // Intervalo de timeout
} linkLayer;

struct{
	unsigned int timeout_counter;
	unsigned int sent_counter;
	unsigned int retry_counter;
	unsigned int received_counter;
}statistics;

int setConfig(int baudrate, int data_length, int max_retries, int timeout_interval);
int llopen(int port, int oflag);
int llwrite(int fd, char* buffer, int length);
int llread(int fd, char* buffer, unsigned int buffer_size);
int llclose(int fd);