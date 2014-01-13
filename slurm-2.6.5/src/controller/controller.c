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

FILE *job_output_file = NULL;

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
pthread_mutex_t job_output_mutex = PTHREAD_MUTEX_INITIALIZER;

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
job_resource *_create_job_resource(char*);
void _step_complete_msg_proc(slurm_msg_t*);
void* _job_proc(void*);
void _print_result(struct timeval*, struct timeval*);

int main(int argc, char *argv[])
{
	/* there should be 8 arguments provided to the controller */
	if (argc < 10)
	{
		fprintf(stderr, "usage:./controller slurm_conf_file numController memList "
				"partitionSize workload zht_config_file zht_memlist num_proc_thread "
				"controller_index\n");
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
	//sleep(30);

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

	char *job_output_path = c_calloc(100);
	strcpy(job_output_path, "./job_output_");
	strcat(job_output_path, argv[9]);
	job_output_file = fopen(job_output_path, "w");
	c_free(job_output_path);
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

	if (find_exist(source, target, part_size) < 0)
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

job_resource *_create_job_resource(char* jobid_origin_ctrlid)
{
	job_resource *a_job_res = init_job_resource();

	/* get the involved controller list in one dimension array*/
	char *jobid_origin_ctrlid_ctrls = c_calloc(strlen(jobid_origin_ctrlid) + 7);
	strcat(jobid_origin_ctrlid_ctrls, jobid_origin_ctrlid);
	strcat(jobid_origin_ctrlid_ctrls, "ctrls");
	c_zht_lookup(jobid_origin_ctrlid_ctrls, a_job_res->ctrl_ids_1);
	c_free(jobid_origin_ctrlid_ctrls);

	/* split the one dimension of involved controller list to two dimension */
	char *ctrl_ids_1_backup = strdup(a_job_res->ctrl_ids_1);
	a_job_res->num_ctrl = split_str(ctrl_ids_1_backup, ",", a_job_res->ctrl_ids_2);

	/* get the allocated nodelist for each involved controller*/
	char *jobid_origin_ctrlid_invid = c_calloc(strlen(jobid_origin_ctrlid) + 30 + 2);

	int i = 0;
	for (; i < a_job_res->num_ctrl; i++)
	{
		strcat(jobid_origin_ctrlid_invid, jobid_origin_ctrlid);
		strcat(jobid_origin_ctrlid_invid, a_job_res->ctrl_ids_2[i]);
		c_zht_lookup(jobid_origin_ctrlid_invid, a_job_res->node_alloc[i]);
		c_memset(jobid_origin_ctrlid_invid, strlen(jobid_origin_ctrlid_invid));
	}

	return a_job_res;
}

void _step_complete_msg_proc(slurm_msg_t* msg)
{
	slurm_send_rc_msg(msg, SLURM_SUCCESS);
	step_complete_msg_t *complete = (step_complete_msg_t*) msg->data;

	/* get the job id */
	char str_job_id[20] = { 0 };
	sprintf(str_job_id, "%u", complete->job_id);
	/* lookup for the origin controller id of this job */
	char *origin_ctrlid = c_calloc(30);
	c_zht_lookup(str_job_id, origin_ctrlid);

	/* concat to get the jobid+origin_ctrlid */
	char *jobid_origin_ctrlid = c_calloc(strlen(origin_ctrlid) + 30 + 2);
	strcat(jobid_origin_ctrlid, str_job_id);
	strcat(jobid_origin_ctrlid, origin_ctrlid);

	/* create the job resource from looking up zht */
	job_resource *a_job_res = _create_job_resource(jobid_origin_ctrlid);
	release_res(a_job_res);
	free_job_resource(a_job_res);

	/* check to see whether this job belongs to the controller */
	if (!strcmp(self_id, origin_ctrlid))
	{
		pthread_mutex_lock(&num_job_fin_mutex);
		num_job_fin++;
		pthread_mutex_unlock(&num_job_fin_mutex);
		unsigned long time_in_micros = get_current_time();
		pthread_mutex_lock(&job_output_mutex);
		fprintf(job_output_file, "%u\tend time\t%lu\n", complete->job_id, time_in_micros);
		pthread_mutex_unlock(&job_output_mutex);
	}
	else	// if it is not, then insert a notification message
	{
		char *key = c_calloc(strlen(jobid_origin_ctrlid) + 5);
		strcat(key, jobid_origin_ctrlid);
		strcat(key, "Fin");
		c_zht_insert(key, "Finished");
		c_free(key);
	}
	c_free(origin_ctrlid);
	c_free(jobid_origin_ctrlid);
	/*pthread_mutex_lock(&lookup_msg_mutex);
	num_lookup_msg += num_lookup_msg_local;
	pthread_mutex_unlock(&lookup_msg_mutex);
	pthread_mutex_lock(&insert_msg_mutex);
	num_insert_msg += num_insert_msg_local;
	pthread_mutex_unlock(&insert_msg_mutex);
	pthread_mutex_lock(&cswap_msg_mutex);
	num_cswap_msg += num_comswap_msg_local;
	pthread_mutex_unlock(&cswap_msg_mutex);*/
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
	fclose(job_output_file);
}
