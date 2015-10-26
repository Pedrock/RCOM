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

#define SERIAL_PATH "/dev/ttyS%d"

// receive_frame errors 
#define READ_ERROR -1
#define UNEXPECTED_N -2
#define LLCLOSE_FAILED -3
#define TIMEOUT_FAIL -4
#define REJECTED -5

// llread errors
#define UNEXPECTED_N -2

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
	bool simulate_errors; // Simulação de erros
} linkLayer;

struct statistics_t {
	unsigned int sent_i_counter; // Número de tramas I únicas recebidas
	unsigned int retry_i_counter; // Número de tramas I reenviadas
	unsigned int received_i_counter; // Número de tramas I recebidas
	unsigned int timeout_counter; // Número de ocorrências de timeout
	unsigned int sent_rej_counter; // Número de tramas REJ enviadas
	unsigned int received_rej_counter; // Número de tramas REJ recebidas
} statistics;

typedef enum {START=0,FLAG_RCV,A_RCV,C_RCV,DATA,DATA_ESCAPE,STOP} State; // Estados possíveis na máquina de estados

typedef struct {
	State state; // Estado atual
	int fd; // File descriptor da porta série
	bool receive_data; // Indica se é para receber dados
	bool use_timeout; // Indica se é para usar timeout
	unsigned char bcc2; // BCC2 calculado
	unsigned char previous_char; // Caracter recebido anteriormente
	bool use_previous; // Indica se já foi recebido um caracter anteriormente
	unsigned char received; // Caracter recebido
	unsigned char received_control; // Controlo recebido
	unsigned char expected_control; // Controlo esperado
	bool reset; // Indica se é para reeniciar a máquina de estados
	int i; // Índice a escrever no buffer
	int s; // Valor de s, para por exemplo N(s)
	int r; // Valor de r, para por exemplo RR(r)
	char* buffer; // Buffer de dados
	int buffer_size; // Tamanho do buffer de dados
} FrameInfo;

int setConfig(int baudrate, int data_length, int max_retries, int timeout_interval, bool simulate_errors);
int llopen(int port, int oflag);
int llwrite(int fd, char* buffer, int length);
int llread(int fd, char* buffer, unsigned int buffer_size);
int llclose(int fd);
struct statistics_t getStatistics();