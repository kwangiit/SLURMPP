#include "stdbool.h"
#include "datastr.h"
#include "malloc.h"
#include <stdio.h>
#include <stdlib.h>

/* initialize a queue */
extern queue* init_queue() {
	queue* new_queue = (queue*) malloc(sizeof(queue));

	new_queue->head = NULL;
	new_queue->tail = NULL;
	new_queue->queue_length = 0;

	return new_queue;
}

/* append to the end of queue */
extern void append_queue(queue* q, char* job_description) {
	queue_item *new_item = (queue_item*) malloc(sizeof(queue_item));

	if (new_item == NULL) {
		printf("malloc() failed when adding element to the queue!\n");
		return;
	}

	new_item->job_description = c_calloc(job_desc_size);
	strcpy(new_item->job_description, job_description);
	new_item->next = NULL;

	if (q == NULL) {
		printf("Queue has not yet been initialized!\n");
		return;
	} else if (q->head == NULL && q->tail == NULL) {
		q->head = new_item;
		q->tail = new_item;
		q->queue_length += 1;
		return;
	} else if (q->head == NULL || q->tail == NULL) {
		printf("The queue is not in the correct format, please check!\n");
		return;
	} else {
		q->tail->next = new_item;
		q->tail = new_item;
		q->queue_length += 1;
	}
}

/* delete and return the first element of queue */
extern queue_item* del_first_queue(queue* q) {
	queue_item *h = NULL;
	queue_item *p = NULL;

	if (q == NULL) {
		printf("The queue does not exist!\n");
		return NULL;
	} else if (q->head == NULL && q->tail == NULL) {
		printf("The queue is empty!\n");
		return NULL;
	} else if (q->head == NULL || q->tail == NULL) {
		printf("The queue is not in the correct format, please check!\n");
		return NULL;
	}

	h = q->head;
	p = h->next;
	q->head = p;
	q->queue_length -= 1;

	if (q->head == NULL) {
		q->tail = q->head;
	}

	return h;
}

extern char* _allocate_node(char* key, char* seen_value,
		char** seen_value_array, int num_node_allocate, int num_node_before,
		char* query_value) {
	int i = 0;
	char* nodelist = c_calloc(partition_size * num_byte_per_node);
	char* new_value = c_calloc(partition_size * num_byte_per_node);
	int num_node_left = num_node_before - num_node_allocate;
	char *_num_node_left = int_to_str(num_node_left);
	//usleep(1000);
	//pthread_mutex_lock(&time_mutex);
	//ntime++;
	//pthread_mutex_unlock(&time_mutex);
	strcat(new_value, _num_node_left);
	c_free(_num_node_left);
	strcat(new_value, ",");
	for (i = 0; i < num_node_allocate; i++) {
		strcat(nodelist, seen_value_array[i]);
		if (i != num_node_allocate - 1) {
			strcat(nodelist, ",");
		}
	}
	for (i = 0; i < num_node_left; i++) {
		strcat(new_value, seen_value_array[i + num_node_allocate]);
		if (i != num_node_left - 1) {
			strcat(new_value, ",");
		}
	}
	int res = c_zht_compare_swap(key, seen_value, new_value, query_value);
	pthread_mutex_lock(&ratio_comswap_msg_mutex);
	ratio_com_and_swap++;
	pthread_mutex_unlock(&ratio_comswap_msg_mutex);
	c_free(new_value);
	if (!res) {
		return nodelist;
	} else {
		c_free(nodelist);
		return NULL;
	}
}

extern char* c_calloc(int size) {
	char* str = (char*) calloc(size, sizeof(char));
	while (!str) {
		sleep(1);
		str = (char*) calloc(size, sizeof(char));
	}
	return str;
}

extern void c_memset(char *str, int size) {
	if (!str) {
		str = c_calloc(size);
	} else {
		memset(str, '\0', size);
	}
}

extern void c_free(char *str)
{
	if (str != NULL)
	{
		free(str);
		str = NULL;
	}
}

extern char** c_malloc_2(int first_dim, int second_dim) {
	char** str = (char**) calloc(first_dim, sizeof(char*));
	int i = 0;
	for (; i < first_dim; i++) {
		str[i] = c_calloc(second_dim);
	}
	return str;
}

extern char* int_to_str(int num) {
	char *str = c_calloc(20);
	sprintf(str, "%d", num);
	return str;
}

extern int str_to_int(char* str) {
	char **end = NULL;
	int num = (int) (strtol(str, end, 10));
	return num;
}

extern int split_str(char* str, char* delim, char** res) {
	int i = 0;
	char *pch = NULL;
	res[i] = strtok_r(str, delim, &pch);
	while (res[i] != NULL && res[i] != "") {
		i++;
		res[i] = strtok_r(NULL, delim, &pch);
	}
	return i;
}

//extern char** split_str(char* str, char* delim, int first_dim, int second_dim) {
//	char** res = c_malloc_2(first_dim, second_dim);
//	int i = 0;
//	char *pch;
//
//	char *tmp = strtok_r(str, delim, &pch);
//	while (tmp) {
//		res[i++] = tmp;
//		tmp = strtok_r(NULL, delim, &pch);
////		res[i] = strtok(NULL, delim);
//	}
//	return res;
//}

extern int get_size(char** str) {
	int size = 0;
	char* tmp = str[size];
	while (tmp) {
		size++;
		tmp = str[size];
	}
	return size;
}

extern int find_exist(char** source, char* target)
{
	int exist = 0;
	int i = 0;
	char* tmp = source[i];
	while (tmp && strcmp(tmp, "") && strcmp(tmp, "\0"))
	{
		fflush(stdout);
		if (!strcmp(tmp, target))
		{
			exist = 1;
			break;
		}
		i++;
		if (i > 50)
		{
			break;
		}
		tmp = source[i];
	}
	return exist;
}

long long timeval_diff(struct timeval *difference, struct timeval *end_time,
		struct timeval *start_time) {
	struct timeval temp_diff;
	if (difference == NULL) {
		difference = &temp_diff;
	}

	difference->tv_sec = end_time->tv_sec - start_time->tv_sec;
	difference->tv_usec = end_time->tv_usec - start_time->tv_usec;

	while (difference->tv_usec < 0) {
		difference->tv_usec += 1000000;
		difference->tv_sec -= 1;
	}

	return 1000000LL * difference->tv_sec + difference->tv_usec;
}
