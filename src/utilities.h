#pragma once

#include <stdio.h>
#include <stdlib.h>

int freeAndReturn(int returnValue, char* pointers[], int length);
int getNumberArgument(int argc, char** argv, char* name);