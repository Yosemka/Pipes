/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.c
 * Copyright (C) 2020 Linux <linux@linux-VirtualBox>
 * 
 * Pipes is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Pipes is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "ipc.h"
#include "common.h"
#include "pa1.h"
#include "process.h"

//прототипы
int CheckOptionAndGetValue(int, char**);
void CreatePipes(int, FILE*);
void CreateChilds(int);
void WriteEventLog(const char *, FILE *, ...);
void WritePipeLog(FILE *, int, int, char*, int);

pid_t pid = 1;	

int main(int argc, char *argv[])
{
	printf("I am %d parent process\n", (int)getpid());
	chProcAmount = 0;

	currentID = 0;
	
	Message msgDone = { {MESSAGE_MAGIC, strlen(""), DONE, 0}, ""};
	Message msgStart = { {MESSAGE_MAGIC, strlen(""), STARTED, 0}, ""};
	
	if((chProcAmount = CheckOptionAndGetValue(argc, argv)) <= 0)
	{
		printf("Must specify -p option...");
		exit(1);
	}

	//Создание дескрипторов файлов для логирования pipe и events		
	FILE *fileEv, *filePipe;	

	//Открытие файла для добавления логов каналов
	filePipe = fopen(pipes_log, "w");
	
	//Создание pipe-ов каналов, pipe[Из][Куда] 
	//например pipe[1][2] передает от доч.проц. 1 в доч.проц. 2

	CreatePipes(chProcAmount, filePipe);	

	fclose(filePipe);

	filePipe = fopen(pipes_log, "a");
	//Открытие файла для добавления логов событий
	fileEv = fopen(events_log, "a");
	
	//Создание дочерних процессов
	for(int i = 1; i <= chProcAmount; i++)
	{
		pid = fork();	//Возвращает PID дочернего процесса, если он все еще в родительском процессе		
			if(pid == -1)	//Не удалось создать дочерний процесс
			{	
				printf("Error on creating child %d. Exiting...\n", i);//amount);
				_exit(0);
			}
			//Выполняется в дочернем процессе
			if(pid == 0)	//Сейчас в дочернем процессе
			{
				currentID = i;	//currentID хранит локальный id процесса
				break;			//выйти из цикла, ибо в дочернем незачем создавать процессы
			}
		printf("Child %d was created %d\n", i, pid);
	}

	//2 дочерних -> 6 каналов, 
	for(int i = 0; i <= chProcAmount; i++)
	{
		for(int j = 0; j <= chProcAmount; j++)
		{
			if(i != currentID && i != j)
			{
				//Например, в дочернем процессе currentID = 2 
				//Закрыть pipe[0][1], pipe[1][0]  на запись
				close(pipes[i][j].field[WRITE]);
				WritePipeLog (filePipe, i, j, "WRITE", (int)getpid());
			}
			if(j != currentID && i != j)
			{
				//Например, в дочернем процессе currentID = 2 
				//Закрыть pipe[1][0], pipe[0][1]  на чтение
				close(pipes[i][j].field[READ]);
				WritePipeLog (filePipe, i, j, "READ", (int)getpid());
			}
		}
	}
	///В дочернем процессе
	if(pid == 0)	//Один из дочерних процессов
	{			
		Message msg;
		int resultStarted = 0;
		int resultDone = 0;

		WriteEventLog(log_started_fmt, fileEv, currentID, (int)getpid(), (int)getppid());

		time_t timeFrom2020 = (time(NULL) - TIMESTAMP_2020)/1000ull;
		msgStart.s_header.s_local_time = (timestamp_t)timeFrom2020;	
		
		//Рассылка всем процессам сообщений STARTED из текущего дочернего процесса		
		if(send_multicast(&currentID, &msgStart) == UNSUCCESS)
		{
			exit(UNSUCCESS);
		}
		
		for(int i = 1; i <= chProcAmount; i++)
		{
			if(currentID != i)	//чтобы не считывать с pipe[1][1], например
			{	
				printf("Trying to read from ch[%d] currentID = %d\n", i, currentID);

				//Чтение сообщений STARTED с доч. проц. под номером i
				//receive возвращает msg.s_header.s_type
				if(receive(&currentID, i, &msg) == STARTED)
				{
					printf("Message returned %d\n", STARTED);
					resultStarted++;	//увеличение счетчика сообщений STARTED от доч. проц.
				}	
			}
		}

		//Проверка на получение всех сообщений STARTED
		if(resultStarted == chProcAmount - 1)
		{	
			printf("CHILD %d recieved ALL STARTED messages", currentID);

			//Запись событий "Получены все STARTED" и ...		
			WriteEventLog(log_received_all_started_fmt, fileEv, currentID);
			//... так как полезной работы нет, "Доч. проц. заверши "полезную" работу" в лог-файл	
			WriteEventLog(log_done_fmt, fileEv, currentID);

			timeFrom2020 = (time(NULL) - TIMESTAMP_2020)/1000ull;
			msgDone.s_header.s_local_time = (timestamp_t)timeFrom2020;	
			
			//Рассылка сообщений DONE всем остальным процессам
			send_multicast(&currentID, &msgDone);
		}
		
		//Чтение сообщений DONE с доч. проц. под номером i
		//receive возвращает msg.s_header.s_type
		for(int i = 1; i <= chProcAmount; i++)
			if(currentID != i)
			{
				if(receive(&currentID, i, &msg) == DONE)
				{
					printf("Message returned %d\n", DONE);
					resultDone++;	//увеличение счетчика сообщений DONE от доч. проц.
				}				
			}

		//Проверка на получение всех сообщений DONE
		if(resultDone == chProcAmount - 1)
		{
			//Запись в лог-файл
			WriteEventLog(log_received_all_done_fmt, fileEv, currentID);
			printf("CHILD %d recieved ALL DONE messages and can go home\n", currentID);
			exit(SUCCESS);	//завершение текущего доч. проц. с кодом 0 (SUCCESS)		
		}
	}
	else //pid != 0 => родительский процесс
	if(PARENT_ID == currentID)	//Родительский процесс
	{
		printf("Waiting child process ending...\n");

		int countStarted = 0;
		int countDone = 0;
		Message msg, ms;
		int done = 0;		

		//Получение STARTED сообщений от дочерних процессов
		for(int i = 1; i <= chProcAmount; i++)
		{			//currentID = 0		
			if(receive(&currentID, i, &msg) == STARTED)
			{
				countStarted++;		//увеличение счетчика сообщений STARTED от доч. проц.
				printf("PARENT received STARTED from CHILD %d\n", i);
			}
		}
		
		//Проверка получения STARTED от всех дочерних процессов
		if(countStarted == chProcAmount)
		{
			WriteEventLog(log_received_all_started_fmt, fileEv, currentID);
			printf("PARENT received ALL STARTED messages\n");
		}
		
		//Получение DONE сообщений от дочерних процессов
		for(int i = 1; i <= chProcAmount; i++)
		{		
			if(receive(&currentID, i, &ms) == DONE)
			{
				countDone++;		//увеличение счетчика сообщений DONE
				printf("PARENT received DONE from CHILD %d\n", i);
			}
		}		
		
		for(int i = 1; i <= chProcAmount; i++)
		{			
			if(wait(&done) == -1)	//Ожидание окончания всех дочерних процессов
			{
				printf("Error waiting child %d\n", i);
				exit(EXIT_FAILURE);
			}
			else
			{	//в done содержится статус выхода из дочернего процесса
				//при успешном завершении доч. проц. в done будет 0
				if(WEXITSTATUS(done) == SUCCESS)	//
				{
					printf("Child %i ended with DONE\n", i);
				}
			}			
		}

		//Проверка получения DONE от всех дочерних процессов
		if(countDone == chProcAmount)
		{
			WriteEventLog(log_received_all_done_fmt, fileEv, currentID);
			printf("PARENT received ALL DONE messages\n");
		}

		//Запись в лог-файл об окончании работы родительского процесса
		WriteEventLog(log_done_fmt, fileEv, currentID);
		printf("Parent process has DONE");
		
		fclose (filePipe);
		fclose(fileEv);	
		
		return SUCCESS;
	}
}
/*
 * Проверка атрибута после симвода 'p'
 */
int CheckOptionAndGetValue(int argc, char *argv[])
{
	int option;
	int childAmount = 0;
	while((option = getopt(argc, argv, "p:")) != UNSUCCESS)	//"p:" - двоеточие говорит, что p обязателен
	{
		switch(option)	//getopt возвращает символ аргумента, а optarg хранит значение аргумента
		{				//то есть optarg количество доч. процессов
			case('p'):
			{
				printf("p Argumnet %s\n", optarg);
				if((childAmount = atoi(optarg)) == 0)
				{
					printf("Incorrect 'child process amount' value");
					return UNSUCCESS;
				}
				else if(childAmount > MAX_PROCESS_ID)	//Если число выше MAX_PROCESS_ID (15)...
				{
					printf("'child process amount' couldnt be more than %d", MAX_PROCESS_ID); 
					childAmount = MAX_PROCESS_ID;		//... то установить количество доч. проц. равным MAX_PROCESS_ID
				}
				else break;			
			}				
			case('?'): printf("Option -p needs an argument"); return UNSUCCESS;
			default: printf("No such option"); return UNSUCCESS;
		}
	}
	return childAmount;
}

void CreatePipes(int procAmount, FILE * file)
{
	int countPipes = 0;		//счетчик активных/созданных/используемых каналов
	for(int i = 0; i <= procAmount; i++)	//Всего каналов надо ([количество доч. проц.] + 1)*2
	{
		for(int j = 0; j <= procAmount; j++)
		{
			
			if(i != j)		//pipe[i][i] внутри одного процесса не нужны
			{
				if(pipe(pipes[i][j].field) == -1)	//Ошибка при создании канала
				{
					printf("Error on creating pipe %d -> %d. Exiting...\n", i, j);
					exit(0);
				}
				else
				{
					if(fprintf(file, "Pipe from %d to %d created, R: %d  W: %d\n",
						   i, j, pipes[i][j].field[READ], pipes[i][j].field[WRITE]) == 0)
					{
						printf("Error writing on \"%s\" pipe[%d][%d]", pipes_log, i, j);
					}

					printf("Pipe from ch[%d] to ch[%d] created, R: %d  W: %d\n",
						   i, j, pipes[i][j].field[READ], pipes[i][j].field[WRITE]);
					
					countPipes++;	//увеличение счетчика при успешном создании канала
				}
			}
		}
	}
	printf("%d Pipes created\n", countPipes);
}

/*
 *  text - константные значения сообщений из файла "pa1.h"
 *  file - дескриптор открытого лог-файла (сделать проверку)
 */
void WriteEventLog(const char *text, FILE *file, ...)
{	
	va_list vars;			//Хранит список аргументов после аргумента file
	va_start(vars, file);	//из библиотеки <stdarg.h>
	//Применение описано https://metanit.com/cpp/c/5.13.php

	if(vfprintf(file, text, vars) == 0)	//Неуспешная запись в лог-файл
	{
		printf ("Error writing on \"event.log\" ");
		printf(text, vars);
	}
	va_end(vars);
}

void WritePipeLog(FILE *file, int from, int to, char* type, int curPID)
{ 
	if(fprintf(file, "Pipe from %d to %d closed to %s in process %d\n", from, to, type, curPID) == 0)
	{
		printf("Error writing on \"pipes.log\" closing pipe from %d to %d to %s in process %d\n", from, to, type, curPID);
	}
}

