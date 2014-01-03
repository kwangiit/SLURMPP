/*
 * datastr.h
 *
 *  Created on: Mar 26, 2013
 *      Author: kwang
 */

#ifndef DATASTR_H_
#define DATASTR_H_

#include <stdlib.h>
#include <string.h>
#include "src/ZHT/src/c_zhtclient.h"
#include "src/ZHT/src/meta.pb-c.h"

extern int partition_size; // number of compute nodes
extern int num_controller; // number of controllers
extern char** mem_list_file; // controller membership list
extern char* controller_id;
extern char** source;

extern int num_byte_per_node;
extern int job_desc_size;

extern int ratio_lookup;
extern int ratio_com_and_swap;

extern int num_proc_thread;

extern int num_job;
extern int num_job_fin;
extern int num_job_fail;
extern pthread_mutex_t num_job_fin_mutex;
extern pthread_mutex_t num_job_fail_mutex;
extern pthread_mutex_t opt_mutex;

extern long long num_insert_msg;
extern long long num_lookup_msg;
extern long long num_comswap_msg;
extern long long num_callback_lookup_msg;

extern pthread_mutex_t insert_msg_mutex;
extern pthread_mutex_t lookup_msg_mutex;
extern pthread_mutex_t comswap_msg_mutex;
extern pthread_mutex_t callback_lookup_msg_mutex;

extern pthread_mutex_t ratio_lookup_msg_mutex;
extern pthread_mutex_t ratio_comswap_msg_mutex;

extern pthread_mutex_t global_mutex;

extern pthread_mutex_t time_mutex;

extern pthread_mutex_t num_proc_thread_mutex;

typedef struct _queue_item {
	char *job_description;
	struct _queue_item *next;
} queue_item;

typedef struct _queue {
	queue_item* head;
	queue_item* tail;
	int queue_length;
} queue;

extern queue* init_queue();
extern void append_queue(queue*, char* job_description);
extern queue_item* del_first_queue(queue*);

extern char* _allocate_node(char*, char*, char**, int, int, char*);

extern void c_memset(char*, int);
extern void c_free(char*);

extern char* c_calloc(int);
extern char** c_malloc_2(int, int);
extern char* int_to_str(int);
extern int str_to_int(char*);
extern int split_str(char*, char*, char**);
extern int get_size(char**);
extern int find_exist(char**, char*);
extern long long timeval_diff(struct timeval*, struct timeval*, struct timeval*);
#endif /* DATASTR_H_ */