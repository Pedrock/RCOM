#include "protocol.h"

void on_alarm()
{
	linkLayer.timeout = true;
	statistics.timeout_counter++;
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
	statistics.sent_rej_counter++;
	return (send_SU_frame(fd, REJ(r)) == 1);
}

int stateStart(FrameInfo* info)
{
	if (info->received == F) info->state = FLAG_RCV;
	else info->state = START;
	return 0;
}

State stateFlagReceived(FrameInfo* info)
{
	if (info->received == F) info->state = FLAG_RCV;
	else if (info->received == A) info->state = A_RCV;
	else info->state = START;
	return 0;
}

State stateAddressReceived(FrameInfo* info)
{
	if (info->received == F) info->state = FLAG_RCV;
	else
	{
		info->received_control = info->received;
		info->state = C_RCV;
	}
	return 0;
}

State stateControlReceived(FrameInfo* info)
{
	if (info->received == F) info->state = FLAG_RCV;
	else if (info->received == (A^(info->received_control)))
	{
		info->state = DATA;
	}
	else if (info->receive_data)
	{
		send_rej_frame(info->fd, info->r);
		info->reset = true;
		info->state = START;
	}
	else info->state = START;
	return 0;
}

State stateBCCreceived(FrameInfo* info)
{
	if (info->received == F)
	{
		if (info->received_control != info->expected_control)
		{
			if (info->s != -1 && info->received_control == REJ(info->s)) {
				statistics.received_rej_counter++;
				if (info->use_timeout) alarm(0);
				return REJECTED;
			}
			else info->reset = true;
		}
		else info->state = STOP;
	}
	else info->state = START;
	return 0;
}

State stateData(FrameInfo* info)
{
	if (info->received == F)
	{
		if (info->received_control == C_DISC)
		{
			linkLayer.disconnected = true;
			if (info->use_timeout) alarm(0);
			if (llclose(info->fd) < 0) return LLCLOSE_FAILED;
			return 0;
		}
		else {
			if (info->received_control == N(info->r)) {
				if (info->use_timeout) alarm(0);
				return UNEXPECTED_N;
			}
			else if (info->received_control == N(info->s) && info->use_previous && info->previous_char == info->bcc2)
			{
				 info->state = STOP;
			}
			else
			{
				if (info->use_previous && info->previous_char != info->bcc2)
					debug_print("Invalid bcc2, received: %02X, expected: %02X\n", info->previous_char, info->bcc2);
				send_rej_frame(info->fd, info->r);
				info->reset = true;
			}
		}
	}
	else if (info->state == DATA) 
	{
		if (info->use_previous) {
			if (info->i >= info->buffer_size)
			{
				debug_print("Insufficient buffer space while receiving data frame.\n");
				info->reset = true;
			}
			else
			{
				info->buffer[info->i++] = info->previous_char;
				info->bcc2 ^= info->previous_char;
			}
		}
		if (info->received == ESCAPE) info->state = DATA_ESCAPE;
		else
		{
			info->previous_char = info->received;
			info->use_previous = true;
		}
	}
	else {
		info->previous_char = ESCAPE_BYTE(info->received);
		info->use_previous = true;
		info->state = DATA;
	}
	return 0;
}

int receive_frame(int fd, bool receive_data, int buffer_size, char* buffer, unsigned char expected_control, bool use_timeout) {
	FrameInfo frame_info;

	frame_info.state = START;
	frame_info.bcc2 = 0;
	frame_info.previous_char = 0;
	frame_info.use_previous = false;
	frame_info.reset = false;
	frame_info.i = 0;
	frame_info.s = -1;
	frame_info.use_timeout = use_timeout;
	frame_info.fd = fd;
	frame_info.buffer = buffer;
	frame_info.buffer_size = buffer_size;
	frame_info.receive_data = receive_data;
	frame_info.expected_control = expected_control;
	if (receive_data) frame_info.s = expected_control;
	else if (expected_control == RR(0) || expected_control == RR(1)) frame_info.s = (expected_control >> 5); // Tirar
	frame_info.r = (frame_info.s?0:1);

	linkLayer.timeout = false;

	if (use_timeout) set_alarm();
	while (frame_info.state != STOP && !linkLayer.timeout)
	{
		int res = read(fd,&frame_info.received,1);
		if (res == 0) continue;
		else if (res < 0)
		{
			if (use_timeout) alarm(0);
			return READ_ERROR;
		}
		if (linkLayer.simulate_errors)
		{
			int random = rand() % 1000;
			if (random == 1)
			{
				printf("Created error\n");
				frame_info.received = rand() % 256;
			}
		}
		
		int result;
		if (frame_info.state == START) result = stateStart(&frame_info);
		else if (frame_info.state == FLAG_RCV) result = stateFlagReceived(&frame_info);
		else if (frame_info.state == A_RCV) result = stateAddressReceived(&frame_info);
		else if (frame_info.state == C_RCV) result = stateControlReceived(&frame_info);
		else if (frame_info.state == DATA && !receive_data) result = stateBCCreceived(&frame_info);
		else if (frame_info.state == DATA || frame_info.state == DATA_ESCAPE) result = stateData(&frame_info);
		if (result) return result;

		if (frame_info.reset)
		{
			frame_info.state = START;
			frame_info.bcc2 = frame_info.i = frame_info.previous_char = 0;
			frame_info.use_previous = false;
			frame_info.reset = false;
		}
	}
	if (use_timeout) alarm(0);
	if (frame_info.state == STOP) {
		if (receive_data) statistics.received_i_counter++;
		return receive_data?frame_info.i:1;
	}
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

int setConfig(int baudrate, int data_length, int max_retries, int timeout_interval, bool simulate_errors)
{
	int value = baudrate_to_config_value(baudrate);
	if (value > 0) linkLayer.baudrate = value;
	else return -1;
	linkLayer.data_length = data_length;
	linkLayer.max_retries = max_retries;
	linkLayer.timeout_interval = timeout_interval;
	linkLayer.simulate_errors = simulate_errors;
	return 0;
}

int llopen(int port, int oflag)
{
	srand(time(NULL));
	linkLayer.disconnected = false;
	linkLayer.oflag = oflag;
	linkLayer.closed = false;

	statistics = (struct statistics_t){0,0,0,0,0,0};

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

	bool success = false;
	int numTransmissions = 0;
	
	while (!success && numTransmissions < linkLayer.max_retries)
	{
		if (oflag == TRANSMITTER)
		{
			debug_print("llopen: Trial number %d\n",numTransmissions+1);
			if (!send_set_frame(fd)) return SEND_SET_FAILED;
			if (receive_ua_frame(fd)) success = true;

		}
		else if (receive_set_frame(fd)) success = true;
			
		numTransmissions++;
	}
	if (!success)
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
	static int s = 0;
	static bool timeout_occurred = false;
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
		if(numTransmissions || timeout_occurred) statistics.retry_i_counter++;
		numTransmissions++;
	}
	if (numTransmissions > 1) debug_print("llwrite: Sent\n");
	free(frame);
	if (success) {
		timeout_occurred = false;
		statistics.sent_i_counter++;
		s = (s ? 0 : 1);
		return length;
	}
	else {
		timeout_occurred = true;
		return TIMEOUT_FAIL;
	}
}

int llread(int fd, char* buffer, unsigned int buffer_size)
{
	static int s = 0;
	int received = receive_i_frame(fd, s, buffer_size, buffer);
	if (received == UNEXPECTED_N) { 
		send_SU_frame(fd,RR(s));
		return UNEXPECTED_N;
	}
	if (received == 0 || linkLayer.closed) return 0;
	send_SU_frame(fd,RR(s?0:1));
	s = (s ? 0 : 1);
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

struct statistics_t getStatistics()
{
	return statistics;
}