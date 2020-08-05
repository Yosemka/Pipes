#ifndef __IFMO_DISTRIBUTED_CLASS_PROCESS__H
#define __IFMO_DISTRIBUTED_CLASS_PROCESS__H

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <getopt.h>

#define READ 0
#define WRITE 1
#define SUCCESS 0
#define UNSUCCESS -1
#define TIMESTAMP_2020 1577836800


typedef struct 
{
	int field[2];
}Pipes;

int chProcAmount;
local_id currentID;
Pipes pipes[MAX_PROCESS_ID][MAX_PROCESS_ID];

#endif 
