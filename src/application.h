#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "protocol.h"


#define DATA_PACKET 0
#define START_PACKET 1
#define END_PACKET 2

#define DATA_PACKET_SIZE 8

struct {
	int fd; /*Descritor correspondente à porta série*/
	int status; /*TRANSMITTER | RECEIVER*/
} appLayer;