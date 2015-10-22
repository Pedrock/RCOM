#include "utilities.h"

int freeAndReturn(int returnValue, char* pointers[], int length)
{
	int i;
	for (i = 0; i < length; i++)
	{
		free(pointers[i]);
	}
	return returnValue;
}

int getNumberArgument(int argc, char** argv, char* name)
{
	int i;
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i],name)) break;
	}
	if (++i < arc)
	{
		return atoi(argv[i]);
	}
	return -1;
}