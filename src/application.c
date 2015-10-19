#include "application.h"

bool send_data_packet(char* buffer, int length, unsigned char N)
{
	unsigned char* packet = malloc(4+length);
	packet[0] = DATA_PACKET;
	packet[1] = N++;
	packet[2] = (uint8_t)(length/256);
	packet[3] = (uint8_t)(length - packet[2]*256);
	int i;
	//debug_print("Sent data: ");
	for (i = 0; i < length; i++)
	{
		//debug_print("0x%02X ",(unsigned char)buffer[i]);
		packet[4+i] = (unsigned char)buffer[i];
	}
	/*debug_print("\n");
	int k;
	printf("Packet sent: ");
	for (k = 0; k < i+4; k++)
	{
		printf("%02X ",packet[k]);
	}
	printf("\n");*/
	int result = llwrite(appLayer.fd,(char*)packet,4+i);
	free(packet);
	return result > 0;
}

bool send_control_packet(char control, unsigned int file_size, char* filename)
{
	char* packet = malloc(sizeof(file_size)+strlen(filename)+10);
	int i = 0, n;
	packet[i++] = control;
	packet[i++] = 0; //T
	packet[i++] = sizeof(file_size); //L
	for (n = sizeof(typeof(file_size))-1; n >= 0 ; n--)
		packet[i++] = (unsigned char)((file_size >> (n << 3)) & 0xff);
	packet[i++] = 1; //T
	packet[i++] = strlen(filename)+1; //L
	strcpy(packet+i,filename);
	i += strlen(filename)+1;
	int result = llwrite(appLayer.fd,packet,i);
	/*int k;
	printf("Packet sent: ");
	for (k = 0; k < i; k++)
	{
		printf("%02X ",(unsigned char)packet[k]);
	}
	printf("\n");*/
	free(packet);
	return result > 0;
}

bool send_file(char* filename)
{
	printf("Sending file...\n");
	FILE* file = fopen(filename, "rb");
	if (file == NULL)
	{
		perror("fopen failed");
		return false;
	}
	fseek(file, 0L, SEEK_END);
	unsigned int file_size = ftell(file);
	fseek(file, 0L, SEEK_SET);
	int r = 0;
	if (!send_control_packet(START_PACKET, file_size, filename))
	{
		perror("send_control_packet failed");
		return false;
	}
	char buffer[DATA_PACKET_SIZE];
	uint8_t N = 0;
	do
	{
		r = fread(buffer, 1, DATA_PACKET_SIZE, file);
		if (r < 0)
		{
			perror("fread failed");
			return false;
		}
		if (r > 0 && !send_data_packet(buffer, r, N++))
		{
			perror("send_data_packet failed");
			return false;
		}
	}
	while (r > 0);
	if (!send_control_packet(END_PACKET, file_size, filename))
	{
		perror("send_control_packet failed");
		return false;
	}
	printf("File sent\n");
	return true;
}

int receive_file()
{
	char* file_name = NULL;
	FILE* output_file = NULL;
	unsigned int file_size = 0;
	unsigned char packet[MAX_SIZE];
	int f_i = 0;
	uint8_t N = 0;
	typedef enum {START, DATA, END} State;
	struct timeval tv0, tv1;
	State state = START;
	int length;
	gettimeofday(&tv0, NULL); 
	while (state != END)
	{
		int r = llread(appLayer.fd, (char*)packet);
		if (r == 0 && r==UNEXPECTED_N) continue;
		if (r < 0) return -1;
		if (packet[0] == START_PACKET)
		{
			if (packet[1] != 0)
			{
				perror("unexpected value - parameter 1");
				free(file_name);
				return -1;
			}
			N = f_i = 0;
			if (file_name != NULL) free(file_name);
			int i;
			file_size = 0;
			for (i = 0; i < packet[2]; i++)
			{
				file_size = (file_size << 8);
				file_size += packet[3+i];
			}
			i = 3+packet[2];
			if (packet[i++] != 1)
			{
				perror("unexpected value - parameter 2");
				free(file_name);
				return -1;
			}
			unsigned char str_length = (unsigned char)(packet[i++]);
			file_name = malloc(str_length);
			memcpy(file_name, packet+i, str_length);
			if (output_file != NULL) fclose(output_file);
			char* file_name_dir = malloc(9+str_length);
			sprintf(file_name_dir,"received/%s",file_name);
			output_file = fopen(file_name_dir, "wb");
			if (output_file == NULL)
			{
				perror(file_name_dir);
				free(file_name_dir);
				free(file_name);
				return -1;
			}
			free(file_name_dir);
			state = DATA;
		}
		else if (state == DATA)
		{
			if (packet[0] == END_PACKET) state = END;
			else if (packet[0] == DATA_PACKET && packet[1] == N)
			{
				length = 256*(unsigned char)(packet[2]) + (unsigned char)(packet[3]);
				fwrite(packet+4, 1, length, output_file);
				f_i += length;
				N++;
			}
			else debug_print("invalid data\n");
		}
		if (file_size && f_i && state != END) {
			gettimeofday(&tv1, NULL); 
			int percentage = 100.0*f_i/file_size;
 			float elapsed = tv1.tv_sec-tv0.tv_sec + (tv1.tv_usec-tv0.tv_usec)/1000000.0;
 			int remaining_time = (file_size-f_i)*elapsed/f_i + 0.5;
			printf("Received %d%%, remaining time: %d seconds\n",percentage,remaining_time);
		}
	}
	if (state == END)
	{
		debug_print("state END\n");
		if (packet[1] != 0)
		{
			perror("unexpected value - parameter 1");
			free(file_name);
			return -1;
		}
		int file_size2 = 0;
		int i;
		for (i = 0; i < packet[2]; i++)
		{
			file_size2 = (file_size2 << 8);
			file_size2 += packet[3+i];
		}
		i = 3+packet[2];
		if (packet[i++] != 1)
		{
			perror("unexpected value - parameter 2");
			free(file_name);
			return -1;
		}
		unsigned char str_length = (unsigned char)(packet[i++]);
		char* file_name2 = malloc(str_length);
		memcpy(file_name2, packet+i, str_length);
		if (file_size != file_size2)
		{
			perror("Headers file sizes do not match");
			free(file_name);
			free(file_name2);
			return -1;
		}
		else if (file_size != f_i)
		{
			debug_print("file_size: %d, f_i: %d\n",file_size,f_i);
			perror("Headers file size and received size do not match");
			free(file_name);
			free(file_name2);
			return -1;
		}
		if (strcmp(file_name, file_name2) != 0)
		{
			perror("File names do not match");
			free(file_name);
			free(file_name2);
			return -1;
		}
		free(file_name);
		free(file_name2);
		fclose(output_file);
		debug_print("received with success\n");
		return f_i;
	}
	return -1;
}

void invalid_args(char* name)
{
	printf("Usage:\t%s <serial port number> [file_to_send]\n",name);
    exit(1);
}

int main(int argc, char** argv)
{
	if (argc != 3 && argc != 2) invalid_args(argv[0]);
    int port;
    if (!sscanf(argv[1],"%d",&port)) invalid_args(argv[0]);

    int oflag = (argc==3?TRANSMITTER:RECEIVER);

    if (oflag == TRANSMITTER) printf("Establishing connection...\n");
    else printf("Waiting for connection...\n");
	appLayer.fd = llopen(port,oflag);
	if (appLayer.fd < 0)
	{
		perror("llopen error");
		exit(1);
	}
	printf("Connection established\n");
	if (oflag == TRANSMITTER)
	{
		if (!send_file(argv[2]))
		{
			perror("send_file error");
			exit(1);
		}
	}
	else
	{
		int length = receive_file();
		if (length < 0)
		{
			perror("receive_file error");
			exit(1);
		}
		else printf("File received with success\n");
	}
	llclose(appLayer.fd);
	printf("Serial port closed\n");
	return 0;
}