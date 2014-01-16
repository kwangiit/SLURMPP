#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <grp.h>
#include "slurmuse.h";

int _gen_random_value(int upper_bound)
{
	return rand() % upper_bound;
}

int _become_user(struct srun_options *opt_1)
{
	char *user = uid_to_string(opt_1->uid);
	gid_t gid = gid_from_uid(opt_1->uid);

	if (strcmp(user, "nobody") == 0)
	{
		xfree(user);
		return (error("Invalid user id %u: %m", opt_1->uid));
	}

	if (opt_1->uid == getuid())
	{
		xfree(user);
		return (0);
	}

	if ((opt_1->egid != (gid_t) -1) && (setgid(opt_1->egid) < 0))
	{
		xfree(user);
		return (error("setgid: %m"));
	}

	initgroups(user, gid); /* Ignore errors */
	xfree(user);

	if (setuid(opt_1->uid) < 0)
		return (error("setuid: %m"));

	return (0);
}

int _shepard_spawn(srun_job_t *job, bool got_alloc)
{
	int shepard_pipe[2], rc;
	pid_t shepard_pid;
	char buf[1];

	if (pipe(shepard_pipe))
	{
		error("pipe: %m");
		return -1;
	}

	shepard_pid = fork();
	if (shepard_pid == -1)
	{
		error("fork: %m");
		return -1;
	}
	if (shepard_pid != 0)
	{
		close(shepard_pipe[0]);
		return shepard_pipe[1];
	}

	/* Wait for parent to notify of completion or I/O error on abort */
	close(shepard_pipe[1]);
	while (1)
	{
		rc = read(shepard_pipe[0], buf, 1);
		if (rc == 1)
		{
			exit(0);
		}
		else if (rc == 0)
		{
			break; /* EOF */
		}
		else if (rc == -1)
		{
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			break;
		}
	}

	(void) slurm_kill_job_step(job->jobid, job->stepid, SIGKILL);

	if (got_alloc)
		slurm_complete_job(job->jobid, NO_VAL);
	exit(0);
	return -1;
}

int _get_index(char *ctl_id)
{
	int i = 0;
	for (i = 0; i < num_ctrl; i++)
	{
		if (!strcmp(ctl_id, mem_list[i]))
		{
			break;
		}
	}
	return i;
}



void update_job_resource(
						    job_resource *a_job_res,
						    int num_node_allocated,
						    char *ctrl_id,
						    char *nodelist)
{
	a_job_res->num_try = 0;
	a_job_res->num_node += num_node_allocated;
	strcat(a_job_res->nodelist, nodelist);
	int index = find_exist(a_job_res->ctrl_ids_2, ctrl_id, num_ctrl);
	if (index < 0)
	{
		strcat(a_job_res->ctrl_ids_1, ctrl_id);
		strcat(a_job_res->ctrl_ids_1, ",");
		strcat(a_job_res->ctrl_ids_2[a_job_res->num_ctrl], ctrl_id);
		strcat(a_job_res->node_alloc[a_job_res->num_ctrl], nodelist);
		strcat(a_job_res->node_alloc[a_job_res->num_ctrl], ",");
		a_job_res->num_ctrl++;
	}
	else
	{
		strcat(a_job_res->node_alloc[index], nodelist);
		strcat(a_job_res->node_alloc[index], ",");
	}
	int self_idx = _get_index(self_id);
	int ctrl_idx = _get_index(ctrl_id);
	if (ctrl_idx < self_idx)
	{
		a_job_res->self = 0;
	}
}

int do_allocate(
				  char *ctrl_id,
				  char *ctrl_res,
				  job_resource *a_job_res,
				  char *query_value,
				  int num_more_node)
{
	char *ctrl_res_copy = strdup(ctrl_res);
	char *p[part_size + 1];
	int count = split_str(ctrl_res, ",", p);
	int num_node = str_to_int(p[0]);
	if (num_node > 0)
	{
		int num_node_attempt = num_node > num_more_node ? num_more_node : num_node;
		//int len = strlen(p[0]);
		//(*p) += len;
		char *nodelist = _allocate_node(ctrl_id, ctrl_res_copy, p,
					num_node_attempt, num_node, query_value);
		pthread_mutex_lock(&cswap_msg_mutex);
		num_cswap_msg++;
		pthread_mutex_unlock(&cswap_msg_mutex);
		if (nodelist == NULL)
		{
			return 0;
		}
		else
		{
			update_job_resource(a_job_res, num_node_attempt, ctrl_id, nodelist);
			return 1;
		}
	}
	else
	{
		return -1;
	}
}

void allocate_one_res(char *ctrl_id, job_resource *a_job_res, int num_more_node)
{
	char *ctrl_res = get_ctrl_res(ctrl_id);

	pthread_mutex_lock(&lookup_msg_mutex);
	num_lookup_msg++;
	pthread_mutex_unlock(&lookup_msg_mutex);

	if (ctrl_res == NULL)
	{
		a_job_res->num_try++;
	}
	else
	{
		char *query_value = c_calloc((part_size + 1) * 30);
		int ret = do_allocate(ctrl_id, ctrl_res, a_job_res, query_value, num_more_node);
		while (ret == 0)
		{
			c_memset(ctrl_res, strlen(ctrl_res));
			strcpy(ctrl_res, query_value);
			ret = do_allocate(ctrl_id, ctrl_res, a_job_res, query_value, num_more_node);
		}
		if (ret < 0)
		{
			a_job_res->num_try++;
			usleep(100000);
		}
	}
}

job_resource *allocate_res(int num_node_required)
{
	unsigned int iseed = (unsigned int) time(NULL);
	srand(iseed);
	job_resource *a_job_res = init_job_resource();
	char *ctrl_id = self_id;
	while (a_job_res->num_node < num_node_required)
	{
		allocate_one_res(ctrl_id, a_job_res, num_node_required - a_job_res->num_node);
		if (a_job_res->num_try > 3)
		{
			release_res(a_job_res);
			reset_job_resource(a_job_res);
			usleep(100000);
		}
		ctrl_id = mem_list[_gen_random_value(num_ctrl)];
	}
	return a_job_res;
}

void insert_jobinfo_zht(uint32_t job_id, job_resource *a_job_res)
{
	long num_insert_msg_local = 0L;
	/* insert (job_id, origin_ctrl_id) */
	char str_job_id[20] = { 0 };
	sprintf(str_job_id, "%u", job_id);
	c_zht_insert(str_job_id, self_id);
	num_insert_msg_local++;

	/* insert (job_id + origin_ctrl_id + "ctrls", involved controllers),
	 * if this job will be returned to the original  controller, then
	 * insert (job_id + origin_ctrl_id, "I am here")*/
	char *jobid_origin_ctrlid = c_calloc(strlen(str_job_id) + strlen(self_id) + 2);
	strcat(jobid_origin_ctrlid, str_job_id);
	strcat(jobid_origin_ctrlid, self_id);
	if (a_job_res->self)
	{
		if (find_exist(a_job_res->ctrl_ids_2, self_id, a_job_res->num_ctrl) >=0 )
		{
			c_zht_insert(jobid_origin_ctrlid, "I am here");
			num_insert_msg_local++;
		}
	}

	char *jobid_origin_ctrlid_ctrls = c_calloc(strlen(jobid_origin_ctrlid) + 7);
	strcat(jobid_origin_ctrlid_ctrls, jobid_origin_ctrlid);
	strcat(jobid_origin_ctrlid_ctrls, "ctrls");
	c_zht_insert(jobid_origin_ctrlid_ctrls, a_job_res->ctrl_ids_1);
	num_insert_msg_local++;

	/* for each involved controller, insert (job_id + origin_ctrl_id +
	 * involved_ctrl_id, involved nodelist)*/
	int i = 0;
	char *jobid_origin_ctrlid_invid = c_calloc(strlen(jobid_origin_ctrlid) + 30 + 2);
	for (; i < a_job_res->num_ctrl; i++)
	{
		strcat(jobid_origin_ctrlid_invid, jobid_origin_ctrlid);
		strcat(jobid_origin_ctrlid_invid, a_job_res->ctrl_ids_2[i]);
		c_zht_insert(jobid_origin_ctrlid_invid, a_job_res->node_alloc[i]);
		num_insert_msg_local++;
		c_memset(jobid_origin_ctrlid_invid, strlen(jobid_origin_ctrlid_invid));
	}
	c_free(jobid_origin_ctrlid);
	c_free(jobid_origin_ctrlid_ctrls);
	c_free(jobid_origin_ctrlid_invid);
	free_job_resource(a_job_res);

	pthread_mutex_lock(&insert_msg_mutex);
	num_insert_msg += num_insert_msg_local;
	pthread_mutex_unlock(&insert_msg_mutex);
}

void _create_srun_job(srun_job_t **p_job, uint32_t jobid, env_t *env,
						slurm_step_launch_callbacks_t *step_callbacks,
						struct srun_options *opt_1)
{

	srun_job_t *job = NULL;

	if (part_size * num_ctrl < opt_1->min_nodes)
	{
		pthread_mutex_lock(&num_job_fail_mutex);
		num_job_fail++;
		pthread_mutex_unlock(&num_job_fail_mutex);
		pthread_exit(NULL);
	}
	else
	{
		job_resource *a_job_res = allocate_res(opt_1->min_nodes);
		opt_1->nodelist = strdup(a_job_res->nodelist);
		job = _job_create_1(jobid, opt_1);
		create_job_step_1(job, false, opt_1);
		if (_become_user(opt_1) < 0)
			info("Warning: Unable to assume uid=%u", opt_1->uid);
		*p_job = job;
		insert_jobinfo_zht(job->jobid, a_job_res);
		/* to be continued with zht message statistics */
	}
}

int _slurm_debug_env_val(void)
{
	long int level = 0;
	const char *val;

	if ((val = getenv("SLURM_DEBUG")))
	{
		char *p;
		if ((level = strtol(val, &p, 10)) < -LOG_LEVEL_INFO)
			level = -LOG_LEVEL_INFO;
		if (p && *p != '\0')
			level = 0;
	}
	return ((int) level);
}

void _set_exit_code(void)
{
	int i;
	char *val;

	if ((val = getenv("SLURM_EXIT_ERROR")))
	{
		i = atoi(val);
		if (i == 0)
			error("SLURM_EXIT_ERROR has zero value");
		else
			error_exit = i;
	}

	if ((val = getenv("SLURM_EXIT_IMMEDIATE")))
	{
		i = atoi(val);
		if (i == 0)
			error("SLURM_EXIT_IMMEDIATE has zero value");
		else
			immediate_exit = i;
	}
}

int _set_rlimit_env(struct srun_options *opt_1)
{
	int rc = SLURM_SUCCESS;
	struct rlimit rlim[1];
	unsigned long cur;
	char name[64], *format;
	slurm_rlimits_info_t *rli;

	/* Modify limits with any command-line options */
	if (opt_1->propagate && parse_rlimits(opt_1->propagate, PROPAGATE_RLIMITS))
	{
		error("--propagate=%s is not valid.", opt_1->propagate);
		exit(error_exit);
	}

	for (rli = get_slurm_rlimits_info(); rli->name != NULL; rli++)
	{
		if (rli->propagate_flag != PROPAGATE_RLIMITS)
			continue;

		if (getrlimit(rli->resource, rlim) < 0)
		{
			error("getrlimit (RLIMIT_%s): %m", rli->name);
			rc = SLURM_FAILURE;
			continue;
		}

		cur = (unsigned long) rlim->rlim_cur;
		snprintf(name, sizeof(name), "SLURM_RLIMIT_%s", rli->name);
		if (opt_1->propagate && rli->propagate_flag == PROPAGATE_RLIMITS)
			/*
			 * Prepend 'U' to indicate user requested propagate
			 */
			format = "U%lu";
		else
			format = "%lu";

		if (setenvf(NULL, name, format, cur) < 0)
		{
			error("unable to set %s in environment", name);
			rc = SLURM_FAILURE;
			continue;
		}

		debug("propagating RLIMIT_%s=%lu", rli->name, cur);
	}

	/*
	 *  Now increase NOFILE to the max available for this srun
	 */
	if (getrlimit(RLIMIT_NOFILE, rlim) < 0)
		return (error("getrlimit (RLIMIT_NOFILE): %m"));

	if (rlim->rlim_cur < rlim->rlim_max)
	{
		rlim->rlim_cur = rlim->rlim_max;
		if (setrlimit(RLIMIT_NOFILE, rlim) < 0)
			return (error("Unable to increase max no. files: %m"));
	}

	return rc;
}

void _set_prio_process_env(void)
{
	int retval;

	errno = 0; /* needed to detect a real failure since prio can be -1 */

	if ((retval = getpriority(PRIO_PROCESS, 0)) == -1)
	{
		if (errno)
		{
			error("getpriority(PRIO_PROCESS): %m");
			return;
		}
	}

	if (setenvf(NULL, "SLURM_PRIO_PROCESS", "%d", retval) < 0)
	{
		error("unable to set SLURM_PRIO_PROCESS in environment");
		return;
	}

	debug("propagating SLURM_PRIO_PROCESS=%d", retval);
}

int _set_umask_env(struct srun_options *opt_1)
{
	if (!getenv("SRUN_DEBUG"))
	{
		/* do not change current value */
		/* NOTE: Default debug level is 3 (info) */
		int log_level = LOG_LEVEL_INFO + _verbose - opt_1->quiet;

		if (setenvf(NULL, "SRUN_DEBUG", "%d", log_level) < 0)
			error("unable to set SRUN_DEBUG in environment");
	}

	if (!getenv("SLURM_UMASK"))
	{
		/* do not change current value */
		char mask_char[5];
		mode_t mask;

		mask = (int) umask(0);
		umask(mask);

		sprintf(mask_char, "0%d%d%d", ((mask >> 6) & 07), ((mask >> 3) & 07), mask & 07);
		if (setenvf(NULL, "SLURM_UMASK", "%s", mask_char) < 0)
		{
			error("unable to set SLURM_UMASK in environment");
			return SLURM_FAILURE;
		}
		debug("propagating UMASK=%s", mask_char);
	}

	return SLURM_SUCCESS;
}

void _set_submit_dir_env(void)
{
	char buf[MAXPATHLEN + 1];

	if ((getcwd(buf, MAXPATHLEN)) == NULL)
	{
		error("getcwd failed: %m");
		exit(error_exit);
	}

	if (setenvf(NULL, "SLURM_SUBMIT_DIR", "%s", buf) < 0)
	{
		error("unable to set SLURM_SUBMIT_DIR in environment");
		return;
	}
}

void check_job_belong(srun_job_t *job)
{
	char *exist = c_calloc(30);
	char str[20] = { 0 }; sprintf(str, "%u", job->jobid);
	char *key = c_calloc(100);
	strcpy(key, str); strcat(key, self_id);
	c_zht_lookup(key, exist);
	pthread_mutex_lock(&lookup_msg_mutex);
	num_lookup_msg++;
	pthread_mutex_unlock(&lookup_msg_mutex);

	if (strcmp(exist, "I am here"))
	{
		strcat(key, "Fin");
		//char *fin = c_calloc(10);
		long num_callback_msg_local = 0L;
		int wait = 1;
		while (c_state_change_callback(key, "Finished", wait) != 0)
		{
			num_callback_msg_local++;
			wait *= 2;
		}
		num_callback_msg_local++;
		/*c_zht_lookup(key, fin);
		while (1)
		{
			if (!strcmp(fin, "Finished") || num_job_fin + num_job_fail >= num_job)
			{
				c_free(fin);
				break;
			}
			else
			{
				usleep(100000);
				c_memset(fin, 10);
				c_zht_lookup(key, fin);
			}
		}*/
		if (num_job_fin + num_job_fail < num_job)
		{
			pthread_mutex_lock(&num_job_fin_mutex);
			num_job_fin++;
			pthread_mutex_unlock(&num_job_fin_mutex);
			unsigned long time_in_micros = get_current_time();
			pthread_mutex_lock(&job_output_mutex);
			fprintf(job_output_file, "%u\tend time\t%lu\n", job->jobid, time_in_micros);
			pthread_mutex_unlock(&job_output_mutex);
		}
		pthread_mutex_lock(&callback_msg_mutex);
		num_callback_msg += num_callback_msg_local;
		pthread_mutex_unlock(&callback_msg_mutex);
	}
	c_free(exist);
	c_free(key);
}

extern void drun_proc(int count, char **job_char_desc)
{
	srand48(pthread_self());
	uint32_t jobid = MIN_NOALLOC_JOBID +
			((uint32_t) lrand48() %
			 (MAX_NOALLOC_JOBID - MIN_NOALLOC_JOBID + 1));
	unsigned long time_in_micros = get_current_time();
	pthread_mutex_lock(&job_output_mutex);
	fprintf(job_output_file, "%u\tstart time\t%lu\n", jobid, time_in_micros);
	pthread_mutex_unlock(&job_output_mutex);
	int debug_level;
	env_t *env = xmalloc(sizeof(env_t));
	log_options_t logopt = LOG_OPTS_STDERR_ONLY;
	bool got_alloc = false;
	slurm_step_io_fds_t cio_fds = SLURM_STEP_IO_FDS_INITIALIZER;
	slurm_step_launch_callbacks_t step_callbacks;

	env->stepid = -1;
	env->procid = -1;
	env->localid = -1;
	env->nodeid = -1;
	env->cli = NULL;
	env->env = NULL;
	env->ckpt_dir = NULL;

	pthread_mutex_lock(&global_mutex);
	debug_level = _slurm_debug_env_val();
	logopt.stderr_level += debug_level;
	log_init(xbasename(job_char_desc[0]), logopt, 0, NULL);

	_set_exit_code();

	if (slurm_select_init(1) != SLURM_SUCCESS)
		fatal("failed to initialize node selection plugin");
	if (switch_init() != SLURM_SUCCESS)
		fatal("failed to initialize switch plugin");

	struct srun_options *opt_1 = (struct srun_options*)calloc(
									1, sizeof(struct srun_options));
	int global_rc = 0;

	if (xsignal_block(sig_array) < 0)
		error("Unable to block signals");

	init_spank_env();
	if (spank_init(NULL) < 0)
	{
		error("Plug-in initialization failed");
		exit(error_exit);
	}

	/* Be sure to call spank_fini when srun exits.
	 */
	if (atexit((void (*)(void)) spank_fini) < 0)
		error("Failed to register atexit handler for plugins: %m");

	pthread_mutex_unlock(&global_mutex);

	initialize_and_process_args_1(count, job_char_desc, opt_1);

	record_ppid();

	if (spank_init_post_opt() < 0)
	{
		error("Plugin stack post-option processing failed.");
		exit(error_exit);
	}

	/* reinit log with new verbosity (if changed by command line)
	 */
	if (&logopt && (_verbose || opt_1->quiet))
	{
		/* If log level is already increased, only increment the
		 *   level to the difference of _verbose an LOG_LEVEL_INFO
		 */
		if ((_verbose -= (logopt.stderr_level - LOG_LEVEL_INFO)) > 0)
			logopt.stderr_level += _verbose;
		logopt.stderr_level -= opt_1->quiet;
		logopt.prefix_level = 1;
		log_alter(logopt, 0, NULL);
	}
	else
		_verbose = debug_level;
	(void) _set_rlimit_env(opt_1);
	_set_prio_process_env();
	(void) _set_umask_env(opt_1);
	_set_submit_dir_env();

	/* create a job structure of SLURM*/
	srun_job_t *job = NULL;
	_create_srun_job(&job, jobid, env, &step_callbacks, opt_1);

	opt_1->spank_job_env = NULL;
	opt_1->spank_job_env_size = 0;

	if (job)
	{
		_enhance_env(env, job, &step_callbacks, opt_1);
		unsigned long time_in_micros = get_current_time();
		pthread_mutex_lock(&job_output_mutex);
		fprintf(job_output_file, "%u\tlaunch time\t%lu\n", job->jobid, time_in_micros);
		pthread_mutex_unlock(&job_output_mutex);
		launch_g_step_launch_1(job, &cio_fds, &global_rc, &step_callbacks, opt_1);

		pthread_mutex_lock(&num_proc_thread_mutex);
		num_proc_thread--;
		pthread_mutex_unlock(&num_proc_thread_mutex);

		if (opt_1 != NULL)
		{
			free(opt_1);
			opt_1 = NULL;
		}
		check_job_belong(job);
	}
	//fini_srun(job, got_alloc, &global_rc, 0);
}

void dcancel_proc(char *job_char_desc)
{
	// to be continued;
}

void dinfo_proc(char *job_char_desc)
{
	// to be continued;
}
