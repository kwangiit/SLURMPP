#include "datastr.h"
#include <unistd.h>
#include <pthread.h>
#include "pro_req.h"
#include "slurm/slurm_errno.h"
#include "src/common/log.h"
#include "src/common/slurm_protocol_defs.h"
#include "src/common/timers.h"
#include "src/common/net.h"
#include "src/common/read_config.h"
#include "slurmctld.h"
#include "slurmuse.h"

int part_size;
int num_ctrl;
char **mem_list = NULL;
char **source = NULL;
char *self_id = NULL;
char *resource = NULL;
queue *job_queue = NULL;
int num_regist_recv = 0;
int ready = 0;

int num_job = 0;
int num_job_fin = 0;
int num_job_fail = 0;
int num_proc_thread = 0;
int max_proc_thread = 0;

long long num_insert_msg = 0LL;
long long num_lookup_msg = 0LL;
long long num_cswap_msg = 0LL;
long long num_callback_msg = 0LL;

pthread_mutex_t regist_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t num_job_fin_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t num_job_fail_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t num_proc_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t insert_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lookup_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cswap_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t callback_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t opt_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;

struct timeval start, end;

typedef struct connection_arg
{
	int newsockfd;
} connection_arg_t;

void _initialize(char**);
void _read_memlist(char*);
void _read_workload(char*);
void* _recv_msg_proc(void*);
void* _service_connection(void*);
void _controller_req(slurm_msg_t*);
void _regist_msg_proc(slurm_msg_t*);
void _step_complete_msg_proc(slurm_msg_t*);
void* _job_proc(void*);
void _print_result(struct timeval*, struct timeval*);

int main(int argc, char *argv[])
{
	/* there should be 8 arguments provided to the controller */
	if (argc < 9)
	{
		fprintf(stderr, "usage:./controller slurm_conf_file numController memList "
				"partitionSize workload zht_config_file zht_memlist num_proc_thread\n");
		exit(1);
	}

	_initialize(argv);

	/* controller forks a new thread to receive messages coming to it*/
	pthread_t *recv_msg_thread = (pthread_t*) malloc(sizeof(pthread_t));
	while (pthread_create(recv_msg_thread, NULL, _recv_msg_proc, NULL))
	{
		fprintf(stderr, "pthread_create error!");
		sleep(1);
	}

	/* blocked until all the slurmds have already registered*/
	while (!ready)
	{
		usleep(1);
	}

	/* sleep for some time to ensure that all the other
	 * controllers also finished registration */
	sleep(30);

	/* start to launch jobs */
	gettimeofday(&start, 0x0); // get the starting time stamp

	/* as long as there are still jobs in the queue,
	 * forks a launching thread to launch the first job*/
	while (job_queue->queue_length > 0)
	{
		if (num_proc_thread < max_proc_thread)
		{
			queue_item *job = del_first_queue(job_queue);
			pthread_t *job_proc_thread = (pthread_t*) malloc(sizeof(pthread_t));
			while (pthread_create(job_proc_thread, NULL, _job_proc, (void*) job))
			{
				fprintf(stderr, "pthread_create error!");
				sleep(1);
			}
			pthread_mutex_lock(&num_proc_thread_mutex);
			num_proc_thread++;
			pthread_mutex_unlock(&num_proc_thread_mutex);
		}
		else
		{
			usleep(1);
		}
	}

	/* blocked until all jobs are finished */
	while (num_job_fin + num_job_fail < num_job)
	{
		usleep(1);
	}

	gettimeofday(&end, 0x0); // get the ending time stamp

	_print_result(&end, &start);

	/* sleep some time in case other controllers
	 * need to talk for resource stealing */
	sleep(10000);
}

void _initialize(char* argv[])
{
	slurm_conf_reinit(argv[1]); //load the slurm configuration file
	self_id = strdup(slurmctld_conf.control_machine); //get the controller id

	num_ctrl = str_to_int(argv[2]); // get the number of controllers
	mem_list = c_calloc_2(num_ctrl, 30);

	_read_memlist(argv[3]); // read the controller membership list

	part_size = str_to_int(argv[4]); //get the partition size
	source = c_calloc_2(part_size + 1, 30);

	/* fill the node list of the controller (part_size, node_1, node_2, ...) */
	resource = c_calloc((part_size + 1) * 30);
	strcat(resource, argv[4]); strcat(resource, ",");

	/* read workload and fill it in the job queue*/
	job_queue = init_queue();
	_read_workload(argv[5]);
	num_job = job_queue->queue_length;

	c_zht_init(argv[6], argv[7]); // initialize the controller as a ZHT client

	max_proc_thread = str_to_int(argv[8]);
}

void _read_memlist(char *mem_file_path)
{
	int i = 0;
	size_t ln = 0;
	FILE* file = fopen(mem_file_path, "r");

	if (file != NULL)
	{
		char line[100];
		while (fgets(line, sizeof(line), file) != NULL)
		{
			ln = strlen(line);
			if (line[ln - 1] == '\n')
			{
				line[ln - 1] = '\0';
			}
			strcpy(mem_list[i++], line);
		}
	}
}

void _read_workload(char *file_path)
{
	FILE *file = fopen(file_path, "r");
	size_t ln = 0;

	if (file != NULL)
	{
		char line[100];
		while (fgets(line, sizeof(line), file) != NULL)
		{
			ln = strlen(line);
			if (line[ln - 1] == '\n')
			{
				line[ln - 1] = '\0';
			}
			append_queue(job_queue, line);
		}
	}
}

void *_recv_msg_proc(void *no_data)
{
	slurm_fd_t sock_fd, new_sock_fd;
	slurm_addr_t client_addr;
	connection_arg_t *conn_arg = (connection_arg_t*)malloc(sizeof(connection_arg_t));

	sock_fd = slurm_init_msg_engine_addrname_port(slurmctld_conf.control_addr,
												slurmctld_conf.slurmctld_port);

	if (sock_fd == SLURM_SOCKET_ERROR)
	{
		fatal("slurm_init_msg_engine_addrname_port error %m");
	}

	while (1)
	{
		if ((new_sock_fd = slurm_accept_msg_conn(sock_fd,
					&client_addr))== SLURM_SOCKET_ERROR)
		{
			error("slurm_accept_msg_conn: %m");
			continue;
		}

		conn_arg->newsockfd = new_sock_fd;

		pthread_t* serv_thread = (pthread_t*)malloc(sizeof(pthread_t));
		while (pthread_create(serv_thread, NULL, _service_connection, (void*) conn_arg))
		{
			error("pthread_create error:%m");
			sleep(1);
		}
	}

	return NULL;
}

void *_service_connection(void* arg)
{
	connection_arg_t *conn = (connection_arg_t*) arg;
	slurm_msg_t *msg = (slurm_msg_t*)malloc(sizeof(slurm_msg_t));
	slurm_msg_t_init(msg);

	if (slurm_receive_msg(conn->newsockfd, msg, 0) != 0)
	{
		error("slurm_receive_msg: %m");
		slurm_close_accepted_conn(conn->newsockfd);
		goto cleanup;
	}

	_controller_req(msg);

	if ((conn->newsockfd >= 0) && slurm_close_accepted_conn(conn->newsockfd) < 0)
	{
		error("close(%d): %m", conn->newsockfd);
	}

	cleanup: free(msg);
	pthread_exit(NULL);
	return NULL;
}

void _controller_req(slurm_msg_t* msg)
{
	if (msg == NULL)
	{
		return;
	}
	if (msg->msg_type == MESSAGE_NODE_REGISTRATION_STATUS)
	{
		_regist_msg_proc(msg);
	}
	else if (msg->msg_type == REQUEST_STEP_COMPLETE)
	{
		_step_complete_msg_proc(msg);
	}
}

void _regist_msg_proc(slurm_msg_t *msg)
{
	slurm_send_rc_msg(msg, SLURM_SUCCESS);
	int num_insert_msg_local = 0;

	pthread_mutex_lock(&regist_mutex);
	char* target = ((slurm_node_registration_status_msg_t *) msg->data)->node_name;

	if (find_exist(source, target, part_size) >= 0)
	{
		strcpy(source[num_regist_recv++], target);
		strcat(resource,target);

		if (num_regist_recv == part_size)
		{
			c_zht_insert(self_id, resource);
			ready = 1;
			num_insert_msg_local++;
			c_free(resource);
			c_free_2(source, part_size + 1);
		}
		else
		{
			strcat(resource, ",");
		}
	}
	pthread_mutex_unlock(&regist_mutex);

	if (num_insert_msg_local > 0)
	{
		pthread_mutex_lock(&insert_msg_mutex);
		num_insert_msg += num_insert_msg_local;
		pthread_mutex_unlock(&insert_msg_mutex);
	}
}

void _step_complete_msg_proc(slurm_msg_t* msg)
{
	slurm_send_rc_msg(msg, SLURM_SUCCESS);
	step_complete_msg_t *complete = (step_complete_msg_t*) msg->data;
	size_t ln;
	char str[20] = { 0 };
	sprintf(str, "%u", complete->job_id);

	char *origin_ctlid = c_calloc(num_byte_per_node);
	char *jobid_origin_ctlid = c_calloc(num_byte_per_node * 2);
	char *jobid_origin_ctlid_ctls = c_calloc(num_byte_per_node * 3);
	int num_lookup_msg_local = 0, num_insert_msg_local = 0,
			num_comswap_msg_local = 0;

	c_zht_lookup(str, origin_ctlid);

	strcat(jobid_origin_ctlid, str);
	strcat(jobid_origin_ctlid, origin_ctlid);
	strcat(jobid_origin_ctlid_ctls, jobid_origin_ctlid);
	strcat(jobid_origin_ctlid_ctls, "ctls");

	char *ctl_ids_one = c_calloc(num_controller * num_byte_per_node);

	c_zht_lookup(jobid_origin_ctlid_ctls, ctl_ids_one);
	c_free(jobid_origin_ctlid_ctls);

	char **ctl_ids = c_malloc_2(num_controller + 1, num_byte_per_node);
	int num_ctl = split_str(ctl_ids_one, ",", ctl_ids);
	char *pre_node_ass = c_calloc(partition_size * num_byte_per_node);
	char *pre_node_ass_copy = c_calloc(partition_size * num_byte_per_node);
	char *node_ass = c_calloc(partition_size * num_byte_per_node);
	char *node_ass_copy = c_calloc(partition_size * num_byte_per_node);
	char *node_ass_new = c_calloc(partition_size * num_byte_per_node);
	char *jobid_origin_ctlid_selfid = c_calloc(num_byte_per_node * 3);

	num_lookup_msg_local += 2;
	int flag = 0;
	char *query_value = c_calloc(partition_size * num_byte_per_node);

	//fprintf(stdout, "The job %u number of controller is %d\n", complete->job_id, num_ctl);
	//fflush(stdout);
	int i;
	for (i = 0; i < num_ctl; i++) {
		flag = 0;
		c_memset(query_value, partition_size * num_byte_per_node);
		c_memset(jobid_origin_ctlid_selfid, num_byte_per_node * 3);
		c_memset(node_ass_copy, partition_size * num_byte_per_node);
		c_memset(node_ass, partition_size * num_byte_per_node);

		strcat(jobid_origin_ctlid_selfid, jobid_origin_ctlid);
		strcat(jobid_origin_ctlid_selfid, ctl_ids[i]);

		c_zht_lookup(jobid_origin_ctlid_selfid, node_ass);

		num_lookup_msg_local++;

		strcpy(node_ass_copy, node_ass);

		int j, k;
		again: j = k = 0;

		c_memset(pre_node_ass, partition_size * num_byte_per_node);
		c_memset(node_ass, partition_size * num_byte_per_node);
		c_memset(pre_node_ass_copy, partition_size * num_byte_per_node);
		c_memset(node_ass_new, partition_size * num_byte_per_node);

		if (!flag || !strcmp(query_value, "") || query_value == NULL) {
			c_zht_lookup(ctl_ids[i], pre_node_ass);
			num_lookup_msg_local++;
		} else {
			strcpy(pre_node_ass, query_value);
		}

		strcpy(pre_node_ass_copy, pre_node_ass);
		strcpy(node_ass, node_ass_copy);

		j = 0;
		k = 0;

		char **pre = c_malloc_2(partition_size + 2, num_byte_per_node);
		if (pre_node_ass != NULL)
		{
			j = split_str(pre_node_ass, ",", pre);
		}
		else
		{
			goto again;
		}
		char **add = c_malloc_2(partition_size + 2, num_byte_per_node);
		k = split_str(node_ass, ",", add);

		char *str_1 = int_to_str(j + k - 1);
		strcat(node_ass_new, str_1);
		strcat(node_ass_new, ",");
		c_free(str_1);

		int idx = 1;
		for (; idx < j; idx++) {
			strcat(node_ass_new, pre[idx]);
			strcat(node_ass_new, ",");
		}
		for (idx = 0; idx < k; idx++) {
			strcat(node_ass_new, add[idx]);
			if (idx != k - 1) {
				strcat(node_ass_new, ",");
			}
		}
		/*for (idx = 0; idx < partition_size + 2; idx++)
		{
			c_free(pre[idx]);
			c_free(add[idx]);
		}*/
		if (pre != NULL)
		{
			free(pre);
			pre = NULL;
		}
		if (add != NULL)
		{
			free(add);
			add = NULL;
		}
		num_comswap_msg_local++;
		//fprintf(stdout, "before free:%s, after free:%s\n", pre_node_ass_copy, node_ass_new);
		//fflush(stdout);
		if (c_zht_compare_swap(ctl_ids[i], pre_node_ass_copy, node_ass_new,
				query_value) != 0) {
			flag = 1;
			//fprintf(stdout, "OK, compare and swap failed!\n");
			//fflush(stdout);
			goto again;
		}
	}
	/*for (i = 0; i < num_ctl; i++) {
		c_free(ctl_ids[i]);
	}
	if (ctl_ids != NULL)
	{
		free(ctl_ids);
		ctl_ids = NULL;
	}*/
	c_free(pre_node_ass_copy);
	c_free(node_ass_copy);
	c_free(node_ass_new);
	c_free(jobid_origin_ctlid_selfid);
	c_free(query_value);

	if (!strcmp(controller_id, origin_ctlid)) {
		pthread_mutex_lock(&num_job_fin_mutex);
		num_job_fin++;
		//fprintf(stdout, "Number of jobs finished is:%d\n", num_job_fin);
		//fflush(stdout);
		pthread_mutex_unlock(&num_job_fin_mutex);
	} else {
		char *key = c_calloc(3 * num_byte_per_node);
		strcat(key, jobid_origin_ctlid);
		strcat(key, "Fin");
		c_zht_insert(key, "Finished");
		num_insert_msg_local++;

		c_free(key);
	}

	c_free(origin_ctlid);
	c_free(jobid_origin_ctlid);
	pthread_mutex_lock(&lookup_msg_mutex);
	num_lookup_msg += num_lookup_msg_local;
	pthread_mutex_unlock(&lookup_msg_mutex);
	pthread_mutex_lock(&insert_msg_mutex);
	num_insert_msg += num_insert_msg_local;
	pthread_mutex_unlock(&insert_msg_mutex);
	pthread_mutex_lock(&cswap_msg_mutex);
	num_cswap_msg += num_comswap_msg_local;
	pthread_mutex_unlock(&cswap_msg_mutex);
}

void *_job_proc(void* data)
{
	queue_item *job = (queue_item*) data;
	char **job_desc = c_calloc_2(10, 30);
	int count = split_str(job->job_description, " ", job_desc);
	if (count == 0)
	{
		fprintf(stdout, "This job has some problem!\n");
	}
	else if (!strcmp(job_desc[0], "srun"))
	{
		drun_proc(count, job_desc);
	}
	else if (!strcmp(job_desc[0], "dcancel"))
	{
		//dcancel_proc(job_origin_desc_left);
	}
	else if (!strcmp(job_desc[0], "dinfo"))
	{
		//dinfo_proc(job_origin_desc_left);
	};
	//free(job_desc);
	pthread_exit(NULL);
	return NULL;
}

void _print_result(struct timeval *end_time, struct timeval *start_time)
{
	/* calculate the overall launching time */
	long long time_diff = timeval_diff(NULL, end_time, start_time);
	double time_s = time_diff / 1000000.0;
	double throughput = num_job_fin / time_s; // calculate the throughput

	/* printing out the measurement resutls */
	fprintf(stdout, "I finished all the jobs in %.16f sec, and "
			"the throughput is:%.16f jobs/sec\n", time_s, throughput);
	fprintf(stdout, "The number of insert message is:%lld\n", num_insert_msg);
	fprintf(stdout, "The number of lookup message is:%lld\n", num_lookup_msg);
	fprintf(stdout, "The number of compare_swap message is:%lld\n", num_cswap_msg);
	fprintf(stdout, "The number of all message to ZHT is:%lld\n", num_insert_msg +
						num_lookup_msg + num_cswap_msg);
	fflush(stdout);
}
