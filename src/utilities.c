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

bool isNumber(char number[])
{
    int i = 0;
    for (; number[i] != 0; i++)
    {
        if (!isdigit(number[i]))
            return false;
    }
    return true;
}