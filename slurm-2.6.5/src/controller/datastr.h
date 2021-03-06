/*
 * datastr.h
 *
 *  Created on: Mar 26, 2013
 *      Author: kwang
 */

#ifndef DATASTR_H_
#define DATASTR_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "src/ZHT/src/c_zhtclient.h"
#include "src/ZHT/src/meta.pb-c.h"

extern int part_size; // number of compute nodes
extern int num_ctrl; // number of controllers
extern char **mem_list; // controller membership list
extern char *self_id;
extern char **source;
extern int num_job;
extern int num_job_fin;
extern int num_job_fail;
extern int num_proc_thread;
extern long long num_insert_msg;
extern long long num_lookup_msg;
extern long long num_cswap_msg;
extern long long num_callback_msg;
extern FILE *job_output_file;

extern pthread_mutex_t num_job_fin_mutex;
extern pthread_mutex_t num_job_fail_mutex;
extern pthread_mutex_t num_proc_thread_mutex;
extern pthread_mutex_t insert_msg_mutex;
extern pthread_mutex_t lookup_msg_mutex;
extern pthread_mutex_t cswap_msg_mutex;
extern pthread_mutex_t callback_msg_mutex;
extern pthread_mutex_t opt_mutex;
extern pthread_mutex_t global_mutex;
extern pthread_mutex_t time_mutex;
extern pthread_mutex_t job_output_mutex;

typedef struct _queue_item
{
	char *job_description;
	struct _queue_item *next;
} queue_item;

typedef struct _queue
{
	queue_item *head;
	queue_item *tail;
	int queue_length;
} queue;

typedef struct _job_resource
{
	int num_try;
	int num_node;
	char *nodelist;
	char *ctrl_ids_1;
	int num_ctrl;
	char **ctrl_ids_2;
	char **node_alloc;
	int self;
} job_resource;

extern queue* init_queue();
extern void append_queue(queue*, char*);
extern queue_item* del_first_queue(queue*);

extern job_resource* init_job_resource();
extern void reset_job_resource(job_resource*);
extern void free_job_resource(job_resource*);
extern void release_res(job_resource*);

extern char* _allocate_node(char*, char*, char**, int, int, char*);

extern void c_memset(char*, int);
extern void c_free(char*);
extern void c_free_2(char**, int);
extern char* c_calloc(int);
extern char** c_calloc_2(int, int);
extern char* int_to_str(int);
extern int str_to_int(char*);
extern int split_str(char*, char*, char**);
extern void merge_res_str(char*, char*, char*);
extern int get_size(char**);
extern int find_exist(char**, char*, int);
extern char* get_ctrl_res(char*);
extern unsigned long get_current_time();
extern long long timeval_diff(struct timeval*, struct timeval*, struct timeval*);
#endif /* DATASTR_H_ */
