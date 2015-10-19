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
	alarm(TIMEOUT);
}

int receive_frame(int fd, bool data, int size, char* buffer, char control, bool timeout) {
	typedef enum {START=0,FLAG_RCV,A_RCV,C_RCV,DATA,DATA_ESCAPE,STOP} State;
	State state = START;
	unsigned char bcc2 = 0;
	unsigned char previous_char = 0;
	bool use_previous = false;
	unsigned char received;
	bool reset = false;
	int i = 0;
	if (size <= 0) size = MAX_SIZE;

	char expected[4];
	expected[0] = F;
	expected[1] = A;
	expected[2] = control;
	expected[3] = expected[1]^expected[2];
	linkLayer.timeout = false;

	if (timeout) set_alarm();
	while (state != STOP && !linkLayer.timeout)
	{
		int res = read(fd,&received,1);
		if (res == 0) continue;
		else if (res < 0)
		{
			return READ_ERROR;
		}
		if (received == F)
		{
			if (state < DATA) state = FLAG_RCV;
			else
			{
				if (!data) state = STOP;
				else if (previous_char == bcc2) state = STOP;
				else
				{
					debug_print("invalid bcc2, received: %02X, expected: %02X\n", previous_char, bcc2);
					reset = true;
				}
			}
		}
		else if (state == A_RCV && received == C_DISC) state = STOP;
		else if (state < DATA && received == expected[state]) state++;
		else if (state == DATA && !data && received == F) state = STOP;
		else if (state >= DATA && data)
		{
			if (state == DATA) {
				if (use_previous) {
					if (i >= size)
					{
						perror("receive_frame: Insufficient buffer space.");
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
		else if (data && state == A_RCV && (received == N(0) || received == N(1))) return UNEXPECTED_N;
		else reset = true;
		if (reset)
		{
			state = START;
			bcc2 = i = previous_char = 0;
			use_previous = false;
			reset = false;
		}
	}
	if (timeout) alarm(0);
	if (state == STOP && received == C_DISC)
	{
		linkLayer.disconnected = true;
		if (data)
		{
			if (llclose(fd) < 0) return LLCLOSE_FAILED;
			return 0;
		}
	}
	if (state == STOP) return data?i:1;
	else return TIMEOUT_FAIL;
}

int receive_i_frame(int fd, char control, char* buffer)
{
	return receive_frame(fd, true, 0, buffer, control, false);
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

int llopen(int port, int oflag)
{
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
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
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
	while (state != STOP && numTransmissions < MAX_TRIES)
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
	if (frame == 0) return -1;

	int numTransmissions = 0;
	int success = false;
	char expected = RR(s?0:1);

	while (!success && numTransmissions < MAX_TRIES)
	{
		if (numTransmissions > 0) debug_print("llwrite: Trial number %d\n",numTransmissions+1);
		write(fd,frame,frame_size);
		success = receive_SU_frame(fd,expected);
		numTransmissions++;
	}
	if (numTransmissions > 1) debug_print("llwrite: Sent\n");
	free(frame);
	if (success) return length;
	else return -1;
}

int llread(int fd, char* buffer)
{
	static int s = 1;
	s = (s ? 0 : 1);
	char control = N(s);
	int received = receive_i_frame(fd, control, buffer);
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
		while (!success && numTransmissions < MAX_TRIES)
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