#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int freeAndReturn(int returnValue, char* pointers[], int length);
int getNumberArgument(int argc, char** argv, char* name);