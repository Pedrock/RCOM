#include "protocol.h"

void on_alarm()
{
	linkLayer.timeout = true;
}

void set_alarm()
{
	struct sigaction sa;
	sa.sa_handler = &on_alarm;
	sa.sa_flags = 0;
	if (sigaction(SIGALRM,&sa,NULL) < 0) perror("sigaction failed");
	alarm(linkLayer.timeout_interval);
}


bool send_SU_frame(int fd, char control)
{
	unsigned char frame[5];
	frame[0] = F;
	frame[1] = A;
	frame[2] = control;
	frame[3] = frame[1]^frame[2];
	frame[4] = F;
	return (write(fd,frame,5) == 5);
}

bool send_ua_frame(int fd)
{
	return (send_SU_frame(fd, C_UA) == 1);
}

bool send_set_frame(int fd)
{
	return (send_SU_frame(fd, C_SET) == 1);
}

bool send_disc_frame(int fd)
{
	return (send_SU_frame(fd, C_DISC) == 1);
}

bool send_rej_frame(int fd, unsigned char r)
{
	printf("send REJ\n");
	return (send_SU_frame(fd, REJ(r)) == 1);
}

int receive_frame(int fd, bool data, int buffer_size, char* buffer, unsigned char control, bool use_timeout) {
	typedef enum {START=0,FLAG_RCV,A_RCV,C_RCV,DATA,DATA_ESCAPE,STOP} State;
	State state = START;
	unsigned char bcc2 = 0;
	unsigned char previous_char = 0;
	bool use_previous = false;
	unsigned char received, received_control;
	bool reset = false;
	int i = 0;
	int s = -1, r;
	if (data) s = control;
	else if (control == RR(0) || control == RR(1)) s = (control >> 5);
	r = (s?0:1);
	char expected[2];
	expected[0] = F;
	expected[1] = A;
	linkLayer.timeout = false;

	if (use_timeout) set_alarm();
	while (state != STOP && !linkLayer.timeout)
	{
		int res = read(fd,&received,1);
		if (res == 0) continue;
		else if (res < 0)
		{
			return READ_ERROR;
		}
		#ifdef SIMULATE_ERRORS
			int random = rand() % 1000;
			if (random == 1)
			{
				printf("Created error\n");
				received = rand() % 256;
			}
		#endif
		if (received == F)
		{
			if (state < DATA) state = FLAG_RCV;
			else
			{
				if (control != C_DISC && received_control == C_DISC)
				{
					linkLayer.disconnected = true;
					if (data)
					{
						if (llclose(fd) < 0) return LLCLOSE_FAILED;
						return 0;
					}
				}
				else if (!data) {
					if (received_control != control)
					{
						if (s != -1 && received_control == REJ(s)) return REJECTED;
						else reset = true;
					}
					else state = STOP;
				}
				else {
					if (received_control == N(r)) return UNEXPECTED_N;
					else if (received_control == N(s) && use_previous && previous_char == bcc2) state = STOP;
					else
					{
						if (use_previous) debug_print("Invalid bcc2, received: %02X, expected: %02X\n", previous_char, bcc2);
						send_rej_frame(fd, r);
						reset = true;
					}
				}
			}
		}
		else if (state < A_RCV && received == expected[state]) state++;
		else if (state == A_RCV) {
			received_control = received; 
			state++;
		}
		else if (state == C_RCV) {
			if (received == (expected[1]^received_control))state++;
			else if (data)
			{
				send_rej_frame(fd, r);
				reset = true;
			}
		}
		else if (state >= DATA && data)
		{
			if (state == DATA) {
				if (use_previous) {
					if (i >= buffer_size)
					{
						debug_print("Insufficient buffer space while receiving data frame.\n");
						reset = true;
					}
					else
					{
						buffer[i++] = previous_char;
						bcc2 ^= previous_char;
					}
				}
				if (received == ESCAPE) state = DATA_ESCAPE;
				else
				{
					previous_char = received;
					use_previous = true;
				}
			}
			else if (state == DATA_ESCAPE) {
				previous_char = ESCAPE_BYTE(received);
				use_previous = true;
				state = DATA;
			}
		}
		else reset = true;
		if (reset)
		{
			state = START;
			bcc2 = i = previous_char = 0;
			use_previous = false;
			reset = false;
		}
	}
	if (use_timeout) alarm(0);
	if (state == STOP) return data?i:1;
	else return TIMEOUT_FAIL;
}

int receive_i_frame(int fd, char s, unsigned int buffer_size, char* buffer)
{
	return receive_frame(fd, true, buffer_size, buffer, s, false);
}

bool receive_set_frame(int fd)
{
	return (receive_frame(fd, false, 0, NULL, C_SET, false) == 1);
}

bool receive_ua_frame(int fd)
{
	return (receive_frame(fd, false, 0, NULL, C_UA, true) == 1);
}

bool receive_disc_frame(int fd)
{
	return (receive_frame(fd, false, 0, NULL, C_DISC, true) == 1);
}

bool receive_SU_frame(int fd, char control)
{
	return (receive_frame(fd, false, 0, NULL, control, true) == 1);
}

int baudrate_to_config_value(int baudrate)
{
	switch (baudrate)
	{
		case 50: return B50;
		case 75: return B75;
		case 110: return B110;
		case 134: return B134;
		case 150: return B150;
		case 200: return B200;
		case 300: return B300;
		case 600: return B600;
		case 1200: return B1200;
		case 1800: return B1800;
		case 2400: return B2400;
		case 4800: return B4800;
		case 9600: return B9600;
		case 19200: return B19200;
		case 38400: return B38400;
		case 57600: return B57600;
		case 115200: return B115200;
		case 230400: return B230400;
		case 460800: return B460800;
		case 500000: return B500000;
		case 576000: return B576000;
		case 921600: return B921600;
		case 1000000: return B1000000;
		case 1152000: return B1152000;
		case 1500000: return B1500000;
		case 2000000: return B2000000;
		case 2500000: return B2500000;
		case 3000000: return B3000000;
		case 3500000: return B3500000;
		case 4000000: return B4000000;
		default: return -1;
	}
}

int setConfig(int baudrate, int data_length, int max_retries, int timeout_interval)
{
	int value = baudrate_to_config_value(baudrate);
	if (value > 0) linkLayer.baudrate = value;
	else return -1;
	linkLayer.data_length = data_length;
	linkLayer.max_retries = max_retries;
	linkLayer.timeout_interval = timeout_interval;
	return 0;
}

int llopen(int port, int oflag)
{
	#ifdef SIMULATE_ERRORS
		srand(time(NULL)); 
	#endif
	linkLayer.disconnected = false;
	linkLayer.oflag = oflag;
	linkLayer.closed = false;
	typedef enum {START = 0,FLAG_RCV,A_RCV,C_RCV,BCC_OK,STOP} State;
	sprintf(linkLayer.port,SERIAL_PATH,port);
	debug_print("opening '%s'\n",linkLayer.port);
	int fd = open(linkLayer.port, O_RDWR | O_NOCTTY);
	if (fd < 0) return OPEN_FAILED;
	debug_print("serial port open\n");
	struct termios newtio;
	if (tcgetattr(fd,&linkLayer.oldtio) == -1) {
		perror("tcgetattr");
		return SERIAL_SETUP_FAILED;
	}
	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = linkLayer.baudrate | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = OPOST;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN]  = 1;

	tcflush(fd, TCIFLUSH);

	if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		return SERIAL_SETUP_FAILED;
	}

	State state = START;
	int numTransmissions = 0;
	
	while (state != STOP && numTransmissions < linkLayer.max_retries)
	{
		if (oflag == TRANSMITTER)
		{
			debug_print("llopen: Trial number %d\n",numTransmissions+1);
			if (!send_set_frame(fd)) return SEND_SET_FAILED;
			if (receive_ua_frame(fd)) state = STOP;
		}
		else if (receive_set_frame(fd)) state = STOP;
		numTransmissions++;
	}
	if (state != STOP)
	{
		debug_print("llopen failed\n");
		close(fd);
		return TIMEOUT_FAIL;
	}
	if (oflag == RECEIVER)
	{
		if (!send_ua_frame(fd)) return -1;
		debug_print("ua sent\n");
	}
	if (linkLayer.disconnected)
	{
		debug_print("Disconnected.\n");
		return DISCONNECTED;
	}
	debug_print("llopen success.\n");
	return fd;
}

unsigned char* create_i_frame(char* buffer, int length, int s, int* frame_size)
{
	int reserved_space = (int)(1.1*length+0.5)+8;
	unsigned char* frame = malloc(reserved_space);
	if (frame == 0) return NULL;
	frame[0] = F;
	frame[1] = A;
	frame[2] = N(s);
	char S = frame[1] ^ frame[2];
	int p = 4, i;
	if (S == F || S == ESCAPE)
	{
		frame[3] = ESCAPE;
		frame[4] = ESCAPE_BYTE(S);
		p = 5;
	}
	else frame[3] = S;
	unsigned char bcc2 = 0;
	for (i = 0; i < length; i++)
	{
		bcc2 ^= buffer[i];
		if (p + 2 >= reserved_space)
		{
			reserved_space += (int)(1.1*(length-i)+0.5);
			frame = realloc(frame,reserved_space);
			if (frame == 0) return NULL;
		}
		if (buffer[i] == F || buffer[i] == ESCAPE)
		{
			frame[p++] = ESCAPE;
			frame[p++] = ESCAPE_BYTE(buffer[i]);
		}
		else frame[p++] = buffer[i];
	}
	if (bcc2 == F || bcc2 == ESCAPE)
	{
		frame[p++] = ESCAPE;
		frame[p++] = ESCAPE_BYTE(bcc2);
	}
	else frame[p++] = bcc2;
	frame[p++] = F;
	*frame_size = p;
	return frame;
}

int llwrite(int fd, char* buffer, int length)
{
	static int s = 1;
	s = (s ? 0 : 1);
	int frame_size;
	unsigned char* frame = create_i_frame(buffer, length, s, &frame_size);
	if (frame == 0) return MALLOC_FAILED;

	int numTransmissions = 0;
	int success = false;
	char expected = RR(s?0:1);

	while (!success && numTransmissions < linkLayer.max_retries)
	{
		if (numTransmissions > 0) debug_print("llwrite: Trial number %d\n",numTransmissions+1);
		write(fd,frame,frame_size);
		success = receive_SU_frame(fd,expected);
		numTransmissions++;
	}
	if (numTransmissions > 1) debug_print("llwrite: Sent\n");
	free(frame);
	if (success) return length;
	else return TIMEOUT_FAIL;
}

int llread(int fd, char* buffer, unsigned int buffer_size)
{
	static int s = 1;
	s = (s ? 0 : 1);
	int received = receive_i_frame(fd, s, buffer_size, buffer);
	if (received == 0 || linkLayer.closed) return 0;
	char rr = RR(s?0:1);
	send_SU_frame(fd,rr);
	return received;
}

int llclose(int fd)
{
	if (linkLayer.closed) return 0;
	if (linkLayer.oflag == TRANSMITTER)
	{
		bool success = false;
		int numTransmissions = 0;
		while (!success && numTransmissions < linkLayer.max_retries)
		{
			debug_print("Sending disconnected, trial: %d\n",numTransmissions+1);
			if (!send_disc_frame(fd)) return -1;
			success = receive_disc_frame(fd);
			numTransmissions++;
		}
		if (success)
		{
			if (!send_ua_frame(fd)) return -1;
		}
		else return -1;
	}
	else if (!send_disc_frame(fd)) return -1;
	if (close(fd) < 0) return -1;
	linkLayer.closed = true;
	debug_print("Closed\n");
	return 1;
}