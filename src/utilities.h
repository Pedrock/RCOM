#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

int freeAndReturn(int returnValue, char* pointers[], int length);
bool isNumber(char number[]);