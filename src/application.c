#include "application.h"

bool tryAgain()
{
	char answer = 0;
	while (answer != 'y' && answer != 'n')
	{
		fseek(stdin,0,SEEK_END);
		printf("Max number of tries reached. Do you want to try again? (y/n) ");
		fflush(stdout);
		answer=tolower(getchar());
		if (getchar() != '\n') answer = 0;
	}
	if (answer == 'y') printf("Sending file...\n");
	return (answer == 'y');
}

int send_data_packet(char* buffer, int length, unsigned char N)
{
	unsigned char* packet = malloc(4+length);
	int result;
	if (packet == NULL) return MALLOC_FAILED;
	packet[0] = DATA_PACKET;
	packet[1] = N;
	packet[2] = (uint8_t)(length / 256);
	packet[3] = (uint8_t)(length % 256);
	memcpy(packet+4,buffer,length);
	do
	{
		result = llwrite(appLayer.fd,(char*)packet,length+4);
	} while (result == TIMEOUT_FAIL && tryAgain());
	free(packet);
	return result;
}

int send_control_packet(char control, unsigned int file_size, char* filename)
{
	char* packet = malloc(sizeof(file_size)+strlen(filename)+10);
	if (packet == NULL) return MALLOC_FAILED;
	int i = 0, n, result;
	packet[i++] = control;
	packet[i++] = 0; //T
	packet[i++] = sizeof(file_size); //L
	for (n = sizeof(typeof(file_size))-1; n >= 0 ; n--)
		packet[i++] = (unsigned char)((file_size >> (n << 3)) & 0xff);
	packet[i++] = 1; //T
	packet[i++] = strlen(filename)+1; //L
	strcpy(packet+i,filename);
	i += strlen(filename)+1;
	do
	{
		result = llwrite(appLayer.fd,packet,i);
	} while (result == TIMEOUT_FAIL && tryAgain());
	free(packet);
	return result;
}

int send_file(char* filename, unsigned int data_length)
{
	printf("Sending file...\n");
	FILE* file = fopen(filename, "rb");
	if (file == NULL)
	{
		perror(filename);
		return OPEN_FILE_ERROR;
	}
	fseek(file, 0L, SEEK_END);
	unsigned int file_size = ftell(file);
	fseek(file, 0L, SEEK_SET);
	int r = 0;
	int result = send_control_packet(START_PACKET, file_size, filename);
	if (result > 0)
	{
		char buffer[data_length];
		uint8_t N = 0;
		do
		{
			r = fread(buffer, 1, data_length, file);
			if (r < 0)
			{
				fprintf(stderr, "An error occured while reading the file.\n");
				fclose(file);
				exit(1);
			}
			if (r > 0)
			{
				result = send_data_packet(buffer, r, N++);
			}
		}
		while (r > 0 && result > 0);
		if (result > 0) result = send_control_packet(END_PACKET, file_size, filename);
	}
	if (result < 0)
	{
		if (result == TIMEOUT_FAIL) 
			fprintf(stderr, "Max number of tries reached. Check your connection and configuration.\n");
		else fprintf(stderr, "An error occured while sending data.\n");
	}
	return result;
}

int processControlPacket(unsigned char* packet, char** file_name, unsigned int* file_size)
{
	if (packet[1] != 0)
	{
		fprintf(stderr, "Error: Invalid header received\n");
		return INVALID_HEADER;
	}
	int i;
	*file_size = 0;
	for (i = 0; i < packet[2]; i++)
	{
		*file_size = (*file_size << 8);
		*file_size += packet[3+i];
	}
	i = 3+packet[2];
	if (packet[i++] != 1)
	{
		fprintf(stderr, "Error: Invalid header received\n");
		return INVALID_HEADER;
	}
	unsigned char str_length = (unsigned char)(packet[i++]);
	if (*file_name != NULL) free(*file_name);
	*file_name = malloc(str_length);
	memcpy(*file_name, packet+i, str_length);
	return 0;
}

int receive_file(unsigned int data_length)
{
	char* file_name = NULL;
	char* file_name2 = NULL;
	FILE* output_file = NULL;
	unsigned int file_size = 0, file_size2 = 0;
	unsigned int buffer_size = 300+data_length;
	unsigned char* packet = malloc(buffer_size);
	int f_i = 0;
	uint8_t N = 0;
	typedef enum {START, DATA, END} State;
	struct timeval tv0, tv1;
	State state = START;
	int length;

	char* pointers[] = {file_name,file_name2,(char*)packet};
	unsigned int n_pointers = sizeof(pointers)/sizeof(char*);

	gettimeofday(&tv0, NULL); 
	while (state != END)
	{
		int r = llread(appLayer.fd, (char*)packet, buffer_size);
		if (r==UNEXPECTED_N) continue;
		if (r < 0) return freeAndReturn(r,pointers,n_pointers);
		if (r <= 2) continue;

		if (packet[0] == START_PACKET)
		{
			N = f_i = 0;
			int result = processControlPacket(packet,&file_name,&file_size);
			if (result < 0) return freeAndReturn(result,pointers,n_pointers);
			if (output_file != NULL) fclose(output_file);
			char* file_name_dir = malloc(9+strlen(file_name));
			sprintf(file_name_dir,"received/%s",file_name);
			output_file = fopen(file_name_dir, "wb");
			if (output_file == NULL)
			{
				perror(file_name_dir);
				free(file_name_dir);
				return freeAndReturn(OUTPUT_FILE_ERROR,pointers,n_pointers);
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
		int result = processControlPacket(packet,&file_name2,&file_size2);
		if (result < 0) return freeAndReturn(result,pointers,n_pointers);
		if (file_size != file_size2)
		{
			fprintf(stderr, "Error: Headers file sizes do not match.\n");
			return freeAndReturn(HEADERS_DO_NOT_MATCH,pointers,n_pointers);
		}
		else if (file_size != f_i)
		{
			fprintf(stderr, "Error: Headers file size and received size do not match.\n");
			return freeAndReturn(HEADERS_DO_NOT_MATCH,pointers,n_pointers);
		}
		if (strcmp(file_name, file_name2) != 0)
		{
			fprintf(stderr, "Error: File names do not match.\n");
			return freeAndReturn(HEADERS_DO_NOT_MATCH,pointers,n_pointers);
		}
		fclose(output_file);
		return freeAndReturn(f_i,pointers,n_pointers);
	}
	return -1;
}

void invalid_args(char* program_name)
{
	printf("Usage:\t%s <serial port number> [file to send] [optional arguments]\n",program_name);
	printf("Arguments\n");
	printf("  %s\n","-b INT\t Baud rate to use");
	printf("  %s\n","-l INT\t Packet data length");
	printf("  %s\n","-t INT\t Max tries per frame");
	printf("  %s\n","-i INT\t Timeout interval");
	printf("  %s\n","-e\t\t Simulate errors");
    exit(1);
}

void printConfiguration(int baudrate, int packet_data_length, int max_tries, int timeout_interval, bool simulate_errors)
{
	printf("\n************ Configuration ************\n");
	printf("\tBaud Rate: %d\n", baudrate);
	printf("\tPacket Data Length: %d\n", packet_data_length);
	printf("\tMax Tries per Packet: %d\n", max_tries);
	printf("\tTimeout Interval: %d\n", timeout_interval);
	printf("\tSimulate Errors: %s\n", simulate_errors?"yes":"no");
	printf("***************************************\n\n");
}

void configWithArguments(int argc, char** argv, int* port, char** file, int* data_length)
{
	const char* int_args[] = {"-b", "-l", "-t", "-i"};
	int int_args_length = 4;
	// Args - baudrate, packet data length, max tries, timeout interval, simulate errors
	int args[] = {DEFAULT_BAUDRATE,DEFAULT_PACKET_DATA_LENGTH,DEFAULT_MAX_TRIES,DEFAULT_TIMEOUT_INTERVAL,false};
	*file = NULL;
	if (argc < 2) invalid_args(argv[0]);
	if (!isNumber(argv[1]) || !sscanf(argv[1],"%d",port)) invalid_args(argv[0]);
	int i = 2;
	if (argc >= 3 && strncmp(argv[2],"-",1) != 0) *file = argv[i++];
	for (; i < argc; i++)
	{
		int found_index = -1, k;
		for (k = 0; k < int_args_length; k++)
		{
			if (strcmp(int_args[k],argv[i]) == 0)
			{
				found_index = k;
				break;
			}
		}
		if (found_index == -1)
		{
			if (strcmp(argv[i],"-e") == 0)
			{
				args[int_args_length] = true;
			}
			else invalid_args(argv[0]);
		}
		else if (++i == argc
				|| !isNumber(argv[i]) 
				|| !sscanf(argv[i],"%d",&args[found_index]))
					invalid_args(argv[0]);
	}
	if (setConfig(args[0],args[1],args[2],args[3], args[4]) < 0)
    {
    	fprintf(stderr, "Invalid baudrate.\n");
    	exit(-1);
    }
    printConfiguration(args[0],args[1],args[2],args[3],args[4]);
    *data_length = args[1];
}

void printStatistics()
{
	struct statistics_t statistics = getStatistics();
	printf("\n******************* Statistics *******************\n");
	printf("\tUnique I frames sent: %d\n", statistics.sent_i_counter);
	printf("\tI frames re-sent: %d\n", statistics.retry_i_counter);
	printf("\tI Frames received: %d\n", statistics.received_i_counter);
	printf("\tTimeouts occurrences: %d\n", statistics.timeout_counter);
	printf("\tREJ frames sent: %d\n", statistics.sent_rej_counter);
	printf("\tREJ Frames received: %d\n", statistics.received_rej_counter);
	printf("**************************************************\n\n");
}

int main(int argc, char** argv)
{
    int port, data_length;
    char* file;
    configWithArguments(argc, argv, &port, &file, &data_length);

    appLayer.oflag = (file?TRANSMITTER:RECEIVER);

    if (appLayer.oflag == TRANSMITTER) printf("Establishing connection...\n");
    else printf("Waiting for connection...\n");
	appLayer.fd = llopen(port,appLayer.oflag);
	if (appLayer.fd < 0)
	{
		if (appLayer.fd == OPEN_FAILED) perror("Opening serial port");
		else fprintf(stderr,"Unable to establish connection.\n");
		exit(1);
	}
	printf("Connection established.\n");
	if (appLayer.oflag == TRANSMITTER)
	{
		if (send_file(argv[2],data_length) < 0) exit(1);
		else printf("File sent with success.\n");
	}
	else
	{
		if (receive_file(data_length) < 0) exit(1);
		else printf("File received with success.\n");
	}
	llclose(appLayer.fd);
	printf("Serial port closed.\n");
	printStatistics();
	return 0;
}