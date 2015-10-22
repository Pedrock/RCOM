#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/time.h>

#include "protocol.h"
#include "utilities.h"


#define DATA_PACKET 0
#define START_PACKET 1
#define END_PACKET 2

#define DEFAULT_BAUDRATE 9600
#define DEFAULT_PACKET_DATA_LENGTH 100
#define DEFAULT_MAX_TRIES 5
#define DEFAULT_TIMEOUT_INTERVAL 1


#define BUFFER_SIZE 5000

struct {
	int fd; /*Descritor correspondente à porta série*/
	int status; /*TRANSMITTER | RECEIVER*/
} appLayer;