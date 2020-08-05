#include "ipc.h"
#include "process.h"

int send(void * self, local_id dst, const Message * msg)
{
	int num = 0;
	int id = *(int*)self;

	//message = заголовок (структура) + тело сообения (char*)
	//	Заколовок = магика + тип + длина сообщения + время
	//write возвращает количество записанных байт
	//write(int descriptor, void * val, size)
	if((num = write(pipes[id][dst].field[WRITE], &msg, sizeof(MessageHeader) + msg->s_header.s_payload_len)) == -1)
	{
		printf("Error writing from ch[%d] to ch[%d]", id, dst);
		return UNSUCCESS;
	}
	else
	{
		printf("ch[%d] WRITING {%s} { %d } bytes to ch[%d]\n", 
		       id, msg->s_header.s_type == 0 ? "STARTED" : "DONE", num, dst);

		return SUCCESS;
	}
}

int send_multicast(void * self, const Message * msg)
{
	int id = *(int*)self;	//разыменовывание указателя
	
	for(int j = 0; j <= chProcAmount; j++)
	{	
		if(id != j)
		{
			if(send(&id, j, msg) == UNSUCCESS)
			{
				return UNSUCCESS;
			}
		}
	}
	return SUCCESS;
}

int receive(void * self, local_id from, Message * msg)
{
	int tmp = 0, tmp2 = 0;
	MessageHeader *mh;		//временная переменная для хранения заголовка сообщения
	char buf[MAX_PAYLOAD_LEN];
	int id = *(int*)self;
	
	//read возвращает количество записанных байт
	tmp = read(pipes[from][id].field[READ], &mh, sizeof(MessageHeader));
	if(tmp == -1)
	{
		printf("Error 1 reading pipe from %d to %d in ch[%d]", from, id, id);
		return UNSUCCESS;
	}
	else
	{
		tmp2 = read(pipes[from][id].field[READ], buf, mh->s_payload_len);

		printf("ch[%d] READING {%s} %d symbols from ch[%d] %d\n", 
			   id, mh->s_type == 0 ? "STARTED" : "DONE", tmp, from, mh->s_payload_len);	

		if(tmp2 == -1)
		{
			printf("Error 2 reading pipe from %d to %d in ch[%d]", from, id, id);
			return UNSUCCESS;
		}
		else
		{
			strncpy(msg->s_payload, buf, tmp2);
			time_t timeStamp = (time_t)mh->s_local_time*1000ul + TIMESTAMP_2020;
			printf("MESSAGE_MAGIC - %X TIMESTAMP - %s\n", mh->s_magic, asctime(localtime(&timeStamp)));
			strncpy(msg->s_payload, buf, mh->s_payload_len);	
		}	
	}

	msg->s_header = *mh;
	return (int)mh->s_type;
}

int receive_any(void * self, Message * msg)
{
	int id = *(int*)self;
	for(int j = 1; j <= chProcAmount; j++)
	{
		if(id != j)
		{	
			if(receive(&id, j, msg) == UNSUCCESS)
			{
				return UNSUCCESS;
			}
		}
	}
	return SUCCESS;
}
