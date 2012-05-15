/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/

#ifndef __MESSAGE_H_8X3E__
#define __MESSAGE_H_8X3E__

#include <pthread.h>

#define MAX_MESSAGE_ELEMENTS 2048

typedef struct message_t message_t;
struct message_t
{
	int 				id;
	int 				command;
//	int                 priority;
	int 				para0;
	void* 				data;
	struct message_t*	next;
};

typedef struct message_quene_t
{
	message_t* 		head;
	message_t* 		tail;
	int 			message_count;
	pthread_mutex_t mutex;
}message_quene_t;

int  message_create(message_quene_t* message);
void message_destroy(message_quene_t* msg_queue);
void flush_message(message_quene_t* msg_queue);
int  put_message(message_quene_t* msg_queue, message_t *msg_in);
int  get_message(message_quene_t* msg_queue, message_t *msg_out);
int  get_message_count(message_quene_t* message);

#endif
