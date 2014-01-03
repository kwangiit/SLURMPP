/*
 * slurmuse.c
 *
 *  Created on: Apr 3, 2013
 *      Author: kwang
 */

#include "slurmuse.h"
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
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <grp.h>

#define OPT_NONE        0x00
#define OPT_INT         0x01
#define OPT_STRING      0x02
#define OPT_IMMEDIATE   0x03
#define OPT_DISTRIB     0x04
#define OPT_NODES       0x05
#define OPT_OVERCOMMIT  0x06
#define OPT_CONN_TYPE	0x08
#define OPT_RESV_PORTS	0x09
#define OPT_NO_ROTATE	0x0a
#define OPT_GEOMETRY	0x0b
#define OPT_MPI         0x0c
#define OPT_CPU_BIND    0x0d
#define OPT_MEM_BIND    0x0e
#define OPT_MULTI       0x0f
#define OPT_NSOCKETS    0x10
#define OPT_NCORES      0x11
#define OPT_NTHREADS    0x12
#define OPT_EXCLUSIVE   0x13
#define OPT_OPEN_MODE   0x14
#define OPT_ACCTG_FREQ  0x15
#define OPT_WCKEY       0x16
#define OPT_SIGNAL      0x17
#define OPT_TIME_VAL    0x18
#define OPT_CPU_FREQ    0x19

/* generic getopt_long flags, integers and *not* valid characters */
#define LONG_OPT_HELP        0x100
#define LONG_OPT_USAGE       0x101
#define LONG_OPT_XTO         0x102
#define LONG_OPT_LAUNCH      0x103
#define LONG_OPT_TIMEO       0x104
#define LONG_OPT_JOBID       0x105
#define LONG_OPT_TMP         0x106
#define LONG_OPT_MEM         0x107
#define LONG_OPT_MINCPUS     0x108
#define LONG_OPT_CONT        0x109
#define LONG_OPT_UID         0x10a
#define LONG_OPT_GID         0x10b
#define LONG_OPT_MPI         0x10c
#define LONG_OPT_RESV_PORTS  0x10d
#define LONG_OPT_DEBUG_TS    0x110
#define LONG_OPT_CONNTYPE    0x111
#define LONG_OPT_TEST_ONLY   0x113
#define LONG_OPT_NETWORK     0x114
#define LONG_OPT_EXCLUSIVE   0x115
#define LONG_OPT_PROPAGATE   0x116
#define LONG_OPT_PROLOG      0x117
#define LONG_OPT_EPILOG      0x118
#define LONG_OPT_BEGIN       0x119
#define LONG_OPT_MAIL_TYPE   0x11a
#define LONG_OPT_MAIL_USER   0x11b
#define LONG_OPT_TASK_PROLOG 0x11c
#define LONG_OPT_TASK_EPILOG 0x11d
#define LONG_OPT_NICE        0x11e
#define LONG_OPT_CPU_BIND    0x11f
#define LONG_OPT_MEM_BIND    0x120
#define LONG_OPT_MULTI       0x122
#define LONG_OPT_COMMENT     0x124
#define LONG_OPT_QOS             0x127
#define LONG_OPT_SOCKETSPERNODE  0x130
#define LONG_OPT_CORESPERSOCKET	 0x131
#define LONG_OPT_THREADSPERCORE  0x132
#define LONG_OPT_MINSOCKETS	 0x133
#define LONG_OPT_MINCORES	 0x134
#define LONG_OPT_MINTHREADS	 0x135
#define LONG_OPT_NTASKSPERNODE	 0x136
#define LONG_OPT_NTASKSPERSOCKET 0x137
#define LONG_OPT_NTASKSPERCORE	 0x138
#define LONG_OPT_MEM_PER_CPU     0x13a
#define LONG_OPT_HINT	         0x13b
#define LONG_OPT_BLRTS_IMAGE     0x140
#define LONG_OPT_LINUX_IMAGE     0x141
#define LONG_OPT_MLOADER_IMAGE   0x142
#define LONG_OPT_RAMDISK_IMAGE   0x143
#define LONG_OPT_REBOOT          0x144
#define LONG_OPT_GET_USER_ENV    0x145
#define LONG_OPT_PTY             0x146
#define LONG_OPT_CHECKPOINT      0x147
#define LONG_OPT_CHECKPOINT_DIR  0x148
#define LONG_OPT_OPEN_MODE       0x149
#define LONG_OPT_ACCTG_FREQ      0x14a
#define LONG_OPT_WCKEY           0x14b
#define LONG_OPT_RESERVATION     0x14c
#define LONG_OPT_RESTART_DIR     0x14d
#define LONG_OPT_SIGNAL          0x14e
#define LONG_OPT_DEBUG_SLURMD    0x14f
#define LONG_OPT_TIME_MIN        0x150
#define LONG_OPT_GRES            0x151
#define LONG_OPT_ALPS            0x152
#define LONG_OPT_REQ_SWITCH      0x153
#define LONG_OPT_LAUNCHER_OPTS   0x154
#define LONG_OPT_CPU_FREQ        0x155
#define LONG_OPT_LAUNCH_CMD      0x156

#define MAX_WIDTH 10


void _handle_msg(void *arg, slurm_msg_t *msg);

struct io_operations message_socket_ops = {
	.readable = &eio_message_socket_readable,
	.handle_read = &eio_message_socket_accept,
	.handle_msg = &_handle_msg
};

typedef struct env_vars
{
	const char *var;
	int type;
	void *arg;
	void *set_flag;
} env_vars_t;

uint32_t pending_job_id = 0;

env_vars_t* _gene_envs(struct srun_options* opt_1)
{
	env_vars_t *envs = (env_vars_t*)malloc(sizeof(env_vars_t) * 66);
	envs[0] = (env_vars_t){"SLURMD_DEBUG",        OPT_INT,        &opt_1->slurmd_debug,  NULL             };
	envs[1] = (env_vars_t){"SLURM_ACCOUNT",       OPT_STRING,     &opt_1->account,       NULL             };
	envs[2] = (env_vars_t){"SLURM_ACCTG_FREQ",    OPT_INT,        NULL,    NULL             };
	envs[3] = (env_vars_t){"SLURM_BLRTS_IMAGE",   OPT_STRING,     &opt_1->blrtsimage,    NULL             };
	envs[4] = (env_vars_t){"SLURM_CHECKPOINT",    OPT_STRING,     &opt_1->ckpt_interval_str, NULL         };
	envs[5] = (env_vars_t){"SLURM_CHECKPOINT_DIR",OPT_STRING,     &opt_1->ckpt_dir,      NULL             };
	envs[6] = (env_vars_t){"SLURM_CNLOAD_IMAGE",  OPT_STRING,     &opt_1->linuximage,    NULL             };
	envs[7] = (env_vars_t){"SLURM_CONN_TYPE",     OPT_CONN_TYPE,  NULL,               NULL             };
	envs[8] = (env_vars_t){"SLURM_CPUS_PER_TASK", OPT_INT,        &opt_1->cpus_per_task, &opt_1->cpus_set    };
	envs[9] = (env_vars_t){"SLURM_CPU_BIND",      OPT_CPU_BIND,   NULL,               NULL             };
	envs[10] = (env_vars_t){"SLURM_CPU_FREQ_REQ",  OPT_CPU_FREQ,   NULL,               NULL             };
	envs[11] = (env_vars_t){"SLURM_DEPENDENCY",    OPT_STRING,     &opt_1->dependency,    NULL             };
	envs[12] = (env_vars_t){"SLURM_DISABLE_STATUS",OPT_INT,        &opt_1->disable_status,NULL             };
	envs[13] = (env_vars_t){"SLURM_DISTRIBUTION",  OPT_DISTRIB,    NULL,               NULL             };
	envs[14] = (env_vars_t){"SLURM_EPILOG",        OPT_STRING,     &opt_1->epilog,        NULL             };
	envs[15] = (env_vars_t){"SLURM_EXCLUSIVE",     OPT_EXCLUSIVE,  NULL,               NULL             };
	envs[16] = (env_vars_t){"SLURM_GEOMETRY",      OPT_GEOMETRY,   NULL,               NULL             };
	envs[17] = (env_vars_t){"SLURM_GRES",          OPT_STRING,     &opt_1->gres,          NULL             };
	envs[18] = (env_vars_t){"SLURM_IMMEDIATE",     OPT_IMMEDIATE,  NULL,               NULL             };
	envs[19] = (env_vars_t){"SLURM_IOLOAD_IMAGE",  OPT_STRING,     &opt_1->ramdiskimage,  NULL             };
	/* SLURM_JOBID was used in slurm version 1.3 and below, it is now vestigial*/
	envs[20] = (env_vars_t){"SLURM_JOBID",         OPT_INT,        &opt_1->jobid,         NULL             };
	envs[21] = (env_vars_t){"SLURM_JOB_ID",        OPT_INT,        &opt_1->jobid,         NULL             };
	envs[22] = (env_vars_t){"SLURM_JOB_NAME",      OPT_STRING,     &opt_1->job_name,  &opt_1->job_name_set_env};
	envs[23] = (env_vars_t){"SLURM_KILL_BAD_EXIT", OPT_INT,        &opt_1->kill_bad_exit, NULL             };
	envs[24] = (env_vars_t){"SLURM_LABELIO",       OPT_INT,        &opt_1->labelio,       NULL             };
	envs[25] = (env_vars_t){"SLURM_LINUX_IMAGE",   OPT_STRING,     &opt_1->linuximage,    NULL             };
	envs[26] = (env_vars_t){"SLURM_MEM_BIND",      OPT_MEM_BIND,   NULL,               NULL             };
	envs[27] = (env_vars_t){"SLURM_MEM_PER_CPU",	OPT_INT,	&opt_1->mem_per_cpu,   NULL             };
	envs[28] = (env_vars_t){"SLURM_MEM_PER_NODE",	OPT_INT,	&opt_1->pn_min_memory, NULL             };
	envs[29] = (env_vars_t){"SLURM_MLOADER_IMAGE", OPT_STRING,     &opt_1->mloaderimage,  NULL             };
	envs[30] = (env_vars_t){"SLURM_MPI_TYPE",      OPT_MPI,        NULL,               NULL             };
	envs[31] = (env_vars_t){"SLURM_NCORES_PER_SOCKET",OPT_NCORES,  NULL,               NULL             };
	envs[32] = (env_vars_t){"SLURM_NETWORK",       OPT_STRING,     &opt_1->network,       NULL             };
	envs[33] = (env_vars_t){"SLURM_NNODES",        OPT_NODES,      NULL,               NULL             };
	envs[34] = (env_vars_t){"SLURM_NODELIST",      OPT_STRING,     &opt_1->alloc_nodelist,NULL             };
	envs[35] = (env_vars_t){"SLURM_NO_ROTATE",     OPT_NO_ROTATE,  NULL,               NULL             };
	envs[36] = (env_vars_t){"SLURM_NTASKS",        OPT_INT,        &opt_1->ntasks,        &opt_1->ntasks_set  };
	envs[37] = (env_vars_t){"SLURM_NPROCS",        OPT_INT,        &opt_1->ntasks,        &opt_1->ntasks_set  };
	envs[38] = (env_vars_t){"SLURM_NSOCKETS_PER_NODE",OPT_NSOCKETS,NULL,               NULL             };
	envs[39] = (env_vars_t){"SLURM_NTASKS_PER_NODE", OPT_INT,      &opt_1->ntasks_per_node, NULL           };
	envs[40] = (env_vars_t){"SLURM_NTHREADS_PER_CORE",OPT_NTHREADS,NULL,               NULL             };
	envs[41] = (env_vars_t){"SLURM_OPEN_MODE",     OPT_OPEN_MODE,  NULL,               NULL             };
	envs[42] = (env_vars_t){"SLURM_OVERCOMMIT",    OPT_OVERCOMMIT, NULL,               NULL             };
	envs[43] = (env_vars_t){"SLURM_PARTITION",     OPT_STRING,     &opt_1->partition,     NULL             };
	envs[44] = (env_vars_t){"SLURM_PROLOG",        OPT_STRING,     &opt_1->prolog,        NULL             };
	envs[45] = (env_vars_t){"SLURM_QOS",           OPT_STRING,     &opt_1->qos,           NULL             };
	envs[46] = (env_vars_t){"SLURM_RAMDISK_IMAGE", OPT_STRING,     &opt_1->ramdiskimage,  NULL             };
	envs[47] = (env_vars_t){"SLURM_REMOTE_CWD",    OPT_STRING,     &opt_1->cwd,           NULL             };
	envs[48] = (env_vars_t){"SLURM_RESTART_DIR",   OPT_STRING,     &opt_1->restart_dir ,  NULL             };
	envs[49] = (env_vars_t){"SLURM_RESV_PORTS",    OPT_RESV_PORTS, NULL,               NULL             };
	envs[50] = (env_vars_t){"SLURM_SIGNAL",        OPT_SIGNAL,     NULL,               NULL             };
	envs[51] = (env_vars_t){"SLURM_SRUN_MULTI",    OPT_MULTI,      NULL,               NULL             };
	envs[52] = (env_vars_t){"SLURM_STDERRMODE",    OPT_STRING,     &opt_1->efname,        NULL             };
	envs[53] = (env_vars_t){"SLURM_STDINMODE",     OPT_STRING,     &opt_1->ifname,        NULL             };
	envs[54] = (env_vars_t){"SLURM_STDOUTMODE",    OPT_STRING,     &opt_1->ofname,        NULL             };
	envs[55] = (env_vars_t){"SLURM_TASK_EPILOG",   OPT_STRING,     &opt_1->task_epilog,   NULL             };
	envs[56] = (env_vars_t){"SLURM_TASK_PROLOG",   OPT_STRING,     &opt_1->task_prolog,   NULL             };
	envs[57] = (env_vars_t){"SLURM_THREADS",       OPT_INT,        &opt_1->max_threads,   NULL             };
	envs[58] = (env_vars_t){"SLURM_TIMELIMIT",     OPT_STRING,     &opt_1->time_limit_str,NULL             };
	envs[59] = (env_vars_t){"SLURM_UNBUFFEREDIO",  OPT_INT,        &opt_1->unbuffered,    NULL             };
	envs[60] = (env_vars_t){"SLURM_WAIT",          OPT_INT,        &opt_1->max_wait,      NULL             };
	envs[61] = (env_vars_t){"SLURM_WCKEY",         OPT_STRING,     &opt_1->wckey,         NULL             };
	envs[62] = (env_vars_t){"SLURM_WORKING_DIR",   OPT_STRING,     &opt_1->cwd,           &opt_1->cwd_set     };
	envs[63] = (env_vars_t){"SLURM_REQ_SWITCH",    OPT_INT,        &opt_1->req_switch,    NULL             };
	envs[64] = (env_vars_t){"SLURM_WAIT4SWITCH",   OPT_TIME_VAL,   NULL,               NULL             };
	envs[65] = (env_vars_t){NULL, 0, NULL, NULL};
	return envs;
}

/*void _pty_restore(void)
{
	if (tcsetattr(STDOUT_FILENO, TCSANOW, &termdefaults) < 0)
			fprintf(stderr, "tcsetattr: %s\n", strerror(errno));
}*/

int
_is_local_file (fname_t *fname)
{
	if (fname->name == NULL)
		return 1;

	if (fname->taskid != -1)
		return 1;

	return ((fname->type != IO_PER_TASK) && (fname->type != IO_ONE));
}

bool _under_parallel_debugger (void)
{
#if defined HAVE_BG_FILES && !defined HAVE_BG_L_P
	/* Use symbols from the runjob.so library provided by IBM.
	 * Do NOT use debugger symbols local to the srun command */
	return false;
#else
	return (MPIR_being_debugged != 0);
#endif
}

void _set_node_alias(void)
{
	char *aliases, *save_ptr = NULL, *tmp;
	char *addr, *hostname, *slurm_name;

	tmp = getenv("SLURM_NODE_ALIASES");
	if (!tmp)
		return;
	aliases = xstrdup(tmp);
	slurm_name = strtok_r(aliases, ":", &save_ptr);
	while (slurm_name) {
		addr = strtok_r(NULL, ":", &save_ptr);
		if (!addr)
			break;
		slurm_reset_alias(slurm_name, addr, addr);
		hostname = strtok_r(NULL, ",", &save_ptr);
		if (!hostname)
			break;
		slurm_name = strtok_r(NULL, ":", &save_ptr);
	}
	xfree(aliases);
}

void _enhance_env(env_t *env, srun_job_t *job,
		slurm_step_launch_callbacks_t *step_callbacks, struct srun_options *opt_1)
{
	if (opt_1->cpus_set)
		env->cpus_per_task = opt_1->cpus_per_task;
	if (opt_1->ntasks_per_node != NO_VAL)
		env->ntasks_per_node = opt_1->ntasks_per_node;
	if (opt_1->ntasks_per_socket != NO_VAL)
		env->ntasks_per_socket = opt_1->ntasks_per_socket;
	if (opt_1->ntasks_per_core != NO_VAL)
		env->ntasks_per_core = opt_1->ntasks_per_core;
	env->distribution = opt_1->distribution;
	if (opt_1->plane_size != NO_VAL)
		env->plane_size = opt_1->plane_size;
	env->cpu_bind_type = opt_1->cpu_bind_type;
	env->cpu_bind = opt_1->cpu_bind;
	env->cpu_freq = opt_1->cpu_freq;
	env->mem_bind_type = opt_1->mem_bind_type;
	env->mem_bind = opt_1->mem_bind;
	env->overcommit = opt_1->overcommit;
	env->slurmd_debug = opt_1->slurmd_debug;
	env->labelio = opt_1->labelio;
	env->comm_port = slurmctld_comm_addr.port;
	env->batch_flag = 0;
	if (job)
	{
		uint16_t *tasks = NULL;
		slurm_step_ctx_get(job->step_ctx, SLURM_STEP_CTX_TASKS, &tasks);
	    env->select_jobinfo = job->select_jobinfo;
	    env->nodelist = job->nodelist;
	    env->nhosts = job->nhosts;
	    env->ntasks = job->ntasks;
	    env->task_count = _uint16_array_to_str(job->nhosts, tasks);
	    env->jobid = job->jobid;
	    env->stepid = job->stepid;
	}
	if (opt_1->pty && (set_winsize(job) < 0))
	{
		error("Not using a pseudo-terminal, disregarding --pty option");
		opt_1->pty = false;
	}
	if (opt_1->pty)
	{
		struct termios term;
		int fd = STDIN_FILENO;

		/* Save terminal settings for restore */
		//tcgetattr(fd, &termdefaults);
		tcgetattr(fd, &term);

		/* Set raw mode on local tty */
		//cfmakeraw(&term);
		tcsetattr(fd, TCSANOW, &term);
		//atexit(&_pty_restore);
		block_sigwinch();
		pty_thread_create(job);
		env->pty_port = job->pty_port;
		env->ws_col   = job->ws_col;
		env->ws_row   = job->ws_row;
	}
	setup_env(env, opt_1->preserve_env);
	xfree(env->task_count);
	xfree(env);
	_set_node_alias();

	memset(step_callbacks, 0, sizeof(*step_callbacks));
	step_callbacks->step_signal   = launch_g_fwd_signal;
}

char* _uint16_array_to_str(int array_len, const uint16_t *array)
{
	int i;
	int previous = 0;
	char *sep = ",";  /* seperator */
	char *str = xstrdup("");

	if(array == NULL)
		return str;

	for (i = 0; i < array_len; i++) {
		if ((i+1 < array_len)
		    && (array[i] == array[i+1])) {
				previous++;
				continue;
		}

		if (i == array_len-1) /* last time through loop */
			sep = "";
		if (previous > 0) {
			xstrfmtcat(str, "%u(x%u)%s",
				   array[i], previous+1, sep);
		} else {
			xstrfmtcat(str, "%u%s", array[i], sep);
		}
		previous = 0;
	}

	return str;
}

slurm_step_launch_params_t *_get_step_launch_param(srun_job_t *job, slurm_step_io_fds_t *cio_fds,
		uint32_t *global_rc, slurm_step_launch_callbacks_t *step_callbacks, struct srun_options *opt_1)
{
	slurm_step_launch_params_t *launch_params = (slurm_step_launch_params_t*)malloc(sizeof(slurm_step_launch_params_t));
	slurm_step_launch_callbacks_t callbacks;
	task_state_t task_state;
	srun_job_t *local_srun_job = NULL;
	uint32_t *local_global_rc = NULL;
	time_t launch_start_time;
	int rc = 0;
	bool first_launch = 0;

	slurm_step_launch_params_t_init(launch_params);
	memcpy(&callbacks, step_callbacks, sizeof(callbacks));

	if (!task_state)
	{
		task_state = task_state_create(job->ntasks);
		local_srun_job = job;
		local_global_rc = global_rc;
		first_launch = 1;
	}
	else
		task_state_alter(task_state, job->ntasks);

	launch_params->gid = opt_1->gid;
	launch_params->alias_list = job->alias_list;
	launch_params->argc = opt_1->argc;
	launch_params->argv = opt_1->argv;
	launch_params->multi_prog = opt_1->multi_prog ? true : false;
	launch_params->cwd = opt_1->cwd;
	launch_params->slurmd_debug = opt_1->slurmd_debug;
	launch_params->buffered_stdio = !opt_1->unbuffered;
	launch_params->labelio = opt_1->labelio ? true : false;
	launch_params->remote_output_filename =fname_remote_string(job->ofname);
	launch_params->remote_input_filename = fname_remote_string(job->ifname);
	launch_params->remote_error_filename = fname_remote_string(job->efname);
	launch_params->task_prolog = opt_1->task_prolog;
	launch_params->task_epilog = opt_1->task_epilog;
	launch_params->cpu_bind = opt_1->cpu_bind;
	launch_params->cpu_bind_type = opt_1->cpu_bind_type;
	launch_params->mem_bind = opt_1->mem_bind;
	launch_params->mem_bind_type = opt_1->mem_bind_type;
	launch_params->open_mode = opt_1->open_mode;
	if (opt_1->acctg_freq >= 0)
		launch_params->acctg_freq = opt_1->acctg_freq;
	launch_params->pty = opt_1->pty;
	if (opt_1->cpus_set)
		launch_params->cpus_per_task     = opt_1->cpus_per_task;
	else
		launch_params->cpus_per_task     = 1;
	launch_params->cpu_freq          = opt_1->cpu_freq;
	launch_params->task_dist         = opt_1->distribution;
	launch_params->ckpt_dir          = opt_1->ckpt_dir;
	launch_params->restart_dir       = opt_1->restart_dir;
	launch_params->preserve_env      = opt_1->preserve_env;
	launch_params->spank_job_env     = opt_1->spank_job_env;
	launch_params->spank_job_env_size = opt_1->spank_job_env_size;
	launch_params->user_managed_io   = opt_1->user_managed_io;

	//memcpy(&launch_params->local_fds, cio_fds, sizeof(slurm_step_io_fds_t));
	if (MPIR_being_debugged)
	{
		launch_params->parallel_debug = true;
		pmi_server_max_threads(1);
	}
	else
	{
		launch_params->parallel_debug = false;
	}

	mpir_init(job->ctx_params.task_count);
	update_job_state(job, SRUN_JOB_LAUNCHING);
	return launch_params;
}

slurm_step_launch_params_t *get_step_launch_param(srun_job_t *job, slurm_step_io_fds_t *cio_fds,
		uint32_t *global_rc, slurm_step_launch_callbacks_t *step_callbacks, opt_t *opt_1)
{
	/*if (launch_init() < 0)
	{
		return NULL;
	}*/
	return _get_step_launch_param(job, cio_fds, global_rc, step_callbacks, opt_1);
}

char *_lookup_cwd(void)
{
	char buf[PATH_MAX];

	if (getcwd(buf, PATH_MAX) != NULL)
	{
		return xstrdup(buf);
	}
	else
	{
		return NULL;
	}
}

launch_tasks_request_msg_t *_get_task_msg(slurm_step_ctx_t *ctx,
										const slurm_step_launch_params_t *params,
										const slurm_step_launch_callbacks_t *callbacks)
{

	launch_tasks_request_msg_t *launch = (launch_tasks_request_msg_t *)malloc(sizeof(launch_tasks_request_msg_t));
	int i;
	char **env = NULL, **mpi_env = NULL, **environ = NULL;
	int rc = SLURM_SUCCESS;

	debug("Entering slurm_step_launch");

	memset(launch, 0, sizeof(launch));

	if (ctx == NULL || ctx->magic != STEP_CTX_MAGIC)
	{
		error("Not a valid slurm_step_ctx_t!");
		slurm_seterrno(EINVAL);
		return NULL;
	}
	if (callbacks != NULL)
	{
		memcpy(&(ctx->launch_state->callback), callbacks, sizeof(slurm_step_launch_callbacks_t));
	}
	else
	{
		memset(&(ctx->launch_state->callback), 0, sizeof(slurm_step_launch_callbacks_t));
	}
	if (mpi_hook_client_init(params->mpi_plugin_name) == SLURM_ERROR)
	{
		slurm_seterrno(SLURM_MPI_PLUGIN_NAME_INVALID);
		return NULL;
	}
	if (mpi_hook_client_single_task_per_node())
	{
		for (i = 0; i < ctx->step_resp->step_layout->node_cnt; i++)
			ctx->step_resp->step_layout->tasks[i] = 1;
	}
	if ((ctx->launch_state->mpi_state = mpi_hook_client_prelaunch(ctx->launch_state->mpi_info, &mpi_env)) == NULL)
	{
		slurm_seterrno(SLURM_MPI_PLUGIN_PRELAUNCH_SETUP_FAILED);
		return NULL;
	}
	rc = _msg_thr_create(ctx->launch_state, ctx->step_resp->step_layout->node_cnt);
	if (rc != SLURM_SUCCESS)
	{
		return NULL;
	}

	/* Start tasks on compute nodes */
	launch->job_id = ctx->step_req->job_id;
	launch->uid = ctx->step_req->user_id;

	launch->gid = params->gid;

	launch->argc = params->argc;
	launch->argv = params->argv;
	int j = 0;
	char *tmp = launch->argv[j];
	while (tmp != NULL)
	{
		j++;
		tmp = launch->argv[j];
	}
	launch->spank_job_env = params->spank_job_env;
	launch->spank_job_env_size = params->spank_job_env_size;
	launch->cred = ctx->step_resp->cred;
	launch->job_step_id = ctx->step_resp->job_step_id;
	if (params->env == NULL)
	{
		env_array_merge(&env, (const char **)environ);
	}
	else
	{
		env_array_merge(&env, (const char **)params->env);
	}
	env_array_for_step(&env, ctx->step_resp,
						ctx->launch_state->resp_port[0],
						params->preserve_env);
	env_array_merge(&env, (const char **)mpi_env);
	env_array_free(mpi_env);
	launch->envc = envcount(env);
	launch->env = env;
	if (params->cwd != NULL)
	{
		launch->cwd = xstrdup(params->cwd);
	}
	else
	{
		launch->cwd = _lookup_cwd();
	}

	launch->alias_list       = params->alias_list;
	launch->nnodes           = ctx->step_resp->step_layout->node_cnt;
	launch->ntasks           = ctx->step_resp->step_layout->task_cnt;
	launch->slurmd_debug     = params->slurmd_debug;
	launch->switch_job       = ctx->step_resp->switch_job;
	launch->task_prolog      = params->task_prolog;
	launch->task_epilog      = params->task_epilog;
	launch->cpu_bind_type    = params->cpu_bind_type;
	launch->cpu_bind         = params->cpu_bind;
	launch->cpu_freq         = params->cpu_freq;
	launch->mem_bind_type    = params->mem_bind_type;
	launch->mem_bind         = params->mem_bind;
	launch->multi_prog       = params->multi_prog ? 1 : 0;
	launch->cpus_per_task    = params->cpus_per_task;
	launch->task_dist        = params->task_dist;
	launch->pty              = params->pty;
	launch->ckpt_dir         = params->ckpt_dir;
	launch->restart_dir      = params->restart_dir;
	launch->acctg_freq       = params->acctg_freq;
	launch->open_mode        = params->open_mode;
	launch->options          = job_options_create();
	launch->complete_nodelist = xstrdup(ctx->step_resp->step_layout->node_list);
	spank_set_remote_options (launch->options);
	launch->task_flags = 0;
	if (params->parallel_debug)
		launch->task_flags |= TASK_PARALLEL_DEBUG;
	launch->tasks_to_launch = ctx->step_resp->step_layout->tasks;
	launch->cpus_allocated  = ctx->step_resp->step_layout->tasks;
	launch->global_task_ids = ctx->step_resp->step_layout->tids;
	launch->select_jobinfo  = ctx->step_resp->select_jobinfo;
	launch->user_managed_io = params->user_managed_io ? 1 : 0;
	ctx->launch_state->user_managed_io = params->user_managed_io;

	if (!ctx->launch_state->user_managed_io)
	{
		launch->ofname = params->remote_output_filename;
		launch->efname = params->remote_error_filename;
		launch->ifname = params->remote_input_filename;
		launch->buffered_stdio = params->buffered_stdio ? 1 : 0;
		launch->labelio = params->labelio ? 1 : 0;
	   	ctx->launch_state->io.normal = client_io_handler_create(params->local_fds,
	   														ctx->step_req->num_tasks,
	   														launch->nnodes,
	   														ctx->step_resp->cred,
	   														params->labelio);
	   	if (ctx->launch_state->io.normal == NULL)
	   	{
	   		rc = SLURM_ERROR;
	   		return NULL;
	   	}
	   	/* The client_io_t gets a pointer back to the slurm_launch_state to notify it of I/O errors. */
	   	ctx->launch_state->io.normal->sls = ctx->launch_state;
	   	if (client_io_handler_start(ctx->launch_state->io.normal) != SLURM_SUCCESS)
	   	{
	   		rc = SLURM_ERROR;
	   		return NULL;
	   	}
	   	launch->num_io_port = ctx->launch_state->io.normal->num_listen;
	   	launch->io_port = xmalloc(sizeof(uint16_t)*launch->num_io_port);
	   	for (i = 0; i < launch->num_io_port; i++)
	   	{
	   		launch->io_port[i] = ctx->launch_state->io.normal->listenport[i];
	   	}
	   	/* If the io timeout is > 0, create a flag to ping the stepds
			if io_timeout seconds pass without stdio traffic to/from
			the node.
		*/
	   	ctx->launch_state->io_timeout = slurm_get_msg_timeout();
	}
	else
	{
		/* user_managed_io is true */
		ctx->launch_state->io.user = (user_managed_io_t *)xmalloc(sizeof(user_managed_io_t));
		ctx->launch_state->io.user->connected = 0;
		ctx->launch_state->io.user->sockets = (int *)xmalloc(sizeof(int)*ctx->step_req->num_tasks);
	}

	launch->num_resp_port = ctx->launch_state->num_resp_port;
	launch->resp_port = xmalloc(sizeof(uint16_t) * launch->num_resp_port);
	for (i = 0; i < launch->num_resp_port; i++)
	{
		launch->resp_port[i] = ctx->launch_state->resp_port[i];
	}

	/*rc = _launch_tasks(ctx, &launch, params->msg_timeout, launch.complete_nodelist, 0);*/

	return launch;
}

slurm_msg_t *_gen_task_launch_msg(launch_tasks_request_msg_t *launch_msg)
{
	slurm_msg_t msg;
	slurm_msg_t_init(&msg);
	msg.msg_type = REQUEST_LAUNCH_TASKS;
	msg.data = launch_msg;
	return &msg;
}

void launch_common_set_stdio_fds_1(srun_job_t *job,
					slurm_step_io_fds_t *cio_fds, struct srun_options *opt_1)
{
	bool err_shares_out = false;
	int file_flags;

	if (opt_1->open_mode == OPEN_MODE_APPEND)
		file_flags = O_CREAT|O_WRONLY|O_APPEND;
	else if (opt_1->open_mode == OPEN_MODE_TRUNCATE)
		file_flags = O_CREAT|O_WRONLY|O_APPEND|O_TRUNC;
	else
	{
		slurm_ctl_conf_t *conf;
		conf = slurm_conf_lock();
		if (conf->job_file_append)
			file_flags = O_CREAT|O_WRONLY|O_APPEND;
		else
			file_flags = O_CREAT|O_WRONLY|O_APPEND|O_TRUNC;
		slurm_conf_unlock();
	}

	/*
	 * create stdin file descriptor
	 */
	if (_is_local_file(job->ifname))
	{
		if ((job->ifname->name == NULL) || (job->ifname->taskid != -1))
		{
			cio_fds->in.fd = STDIN_FILENO;
		}
		else
		{
			cio_fds->in.fd = open(job->ifname->name, O_RDONLY);
			if (cio_fds->in.fd == -1)
			{
				error("Could not open stdin file: %m");
				exit(error_exit);
			}
		}
		if (job->ifname->type == IO_ONE)
		{
			cio_fds->in.taskid = job->ifname->taskid;
			cio_fds->in.nodeid = slurm_step_layout_host_id(
					launch_common_get_slurm_step_layout(job),
					job->ifname->taskid);
		}
	}

	/*
	 * create stdout file descriptor
	 */
	if (_is_local_file(job->ofname))
	{
		if ((job->ofname->name == NULL) || (job->ofname->taskid != -1))
		{
			cio_fds->out.fd = STDOUT_FILENO;
		}
		else
		{
			cio_fds->out.fd = open(job->ofname->name, file_flags, 0644);
			if (cio_fds->out.fd == -1)
			{
				error("Could not open stdout file: %m");
				exit(error_exit);
			}
		}
		if (job->ofname->name != NULL
			    && job->efname->name != NULL
			    && !strcmp(job->ofname->name, job->efname->name))
		{
			err_shares_out = true;
		}
	}

	/*
	 * create seperate stderr file descriptor only if stderr is not sharing
	 * the stdout file descriptor
	 */
	if (err_shares_out)
	{
		debug3("stdout and stderr sharing a file");
		cio_fds->err.fd = cio_fds->out.fd;
		cio_fds->err.taskid = cio_fds->out.taskid;
	}
	else if (_is_local_file(job->efname))
	{
		if ((job->efname->name == NULL) || (job->efname->taskid != -1))
		{
			cio_fds->err.fd = STDERR_FILENO;
		}
		else
		{
			cio_fds->err.fd = open(job->efname->name, file_flags, 0644);
			if (cio_fds->err.fd == -1)
			{
				error("Could not open stderr file: %m");
				exit(error_exit);
			}
		}
	}
}

int launch_g_step_launch_1(srun_job_t *job, slurm_step_io_fds_t *cio_fds,
						   uint32_t *global_rc,
						   slurm_step_launch_callbacks_t *step_callbacks,
						   struct srun_options *opt_1)
{
	slurm_step_launch_params_t* launch_params = get_step_launch_param(
																	  	  job,
																	  	  cio_fds,
																	  	  global_rc,
																	  	  step_callbacks,
																	  	  opt_1
																	  );


	return slurm_step_launch(job->step_ctx, launch_params, step_callbacks);
	//launch_tasks_request_msg_t *launch = _get_task_msg(job->step_ctx, launch_params, step_callbacks);
	//return launch_tasks(launch, launch->complete_nodelist);
}

void _opt_default_1(struct srun_options *opt_1)
{
	char buf[MAXPATHLEN + 1];
	int i;
	uid_t uid = getuid();

	opt_1->user = uid_to_string(uid);
	if (strcmp(opt_1->user, "nobody") == 0)
		fatal("Invalid user id: %u", uid);

	opt_1->uid = uid;
	opt_1->gid = getgid();

	if ((getcwd(buf, MAXPATHLEN)) == NULL) {
		error("getcwd failed: %m");
		exit(error_exit);
	}
	opt_1->cwd = xstrdup(buf);
	opt_1->cwd_set = false;

	opt_1->progname = NULL;

	opt_1->ntasks = 1;
	opt_1->ntasks_set = false;
	opt_1->cpus_per_task = 0;
	opt_1->cpus_set = false;
	opt_1->min_nodes = 1;
	opt_1->max_nodes = 0;
	opt_1->sockets_per_node = NO_VAL; /* requested sockets */
	opt_1->cores_per_socket = NO_VAL; /* requested cores */
	opt_1->threads_per_core = NO_VAL; /* requested threads */
	opt_1->ntasks_per_node      = NO_VAL; /* ntask max limits */
	opt_1->ntasks_per_socket    = NO_VAL;
	opt_1->ntasks_per_core      = NO_VAL;
	opt_1->nodes_set = false;
	opt_1->nodes_set_env = false;
	opt_1->nodes_set_opt = false;
	opt_1->cpu_bind_type = 0;
	opt_1->cpu_bind = NULL;
	opt_1->mem_bind_type = 0;
	opt_1->mem_bind = NULL;
	opt_1->time_limit = NO_VAL;
	opt_1->time_limit_str = NULL;
	opt_1->time_min = NO_VAL;
	opt_1->time_min_str = NULL;
	opt_1->ckpt_interval = 0;
	opt_1->ckpt_interval_str = NULL;
	opt_1->ckpt_dir = NULL;
	opt_1->restart_dir = NULL;
	opt_1->partition = NULL;
	opt_1->max_threads = MAX_THREADS;
	pmi_server_max_threads(opt_1->max_threads);

	opt_1->relative = NO_VAL;
	opt_1->relative_set = false;
	opt_1->resv_port_cnt = NO_VAL;
	opt_1->cmd_name = NULL;
	opt_1->job_name = NULL;
	opt_1->job_name_set_cmd = false;
	opt_1->job_name_set_env = false;
	opt_1->jobid    = NO_VAL;
	opt_1->jobid_set = false;
	opt_1->dependency = NULL;
	opt_1->account  = NULL;
	opt_1->comment  = NULL;
	opt_1->qos      = NULL;

	opt_1->distribution = SLURM_DIST_UNKNOWN;
	opt_1->plane_size   = NO_VAL;

	opt_1->ofname = NULL;
	opt_1->ifname = NULL;
	opt_1->efname = NULL;

	opt_1->labelio = false;
	opt_1->unbuffered = false;
	opt_1->overcommit = false;
	opt_1->shared = (uint16_t)NO_VAL;
	opt_1->exclusive = false;
	opt_1->no_kill = false;
	opt_1->kill_bad_exit = NO_VAL;

	opt_1->immediate	= 0;

	opt_1->join	= false;
	opt_1->max_wait	= slurm_get_wait_time();

	opt_1->quit_on_intr = false;
	opt_1->disable_status = false;
	opt_1->test_only   = false;
	opt_1->preserve_env = false;

	opt_1->quiet = 0;
	//_verbose = 0;
	opt_1->slurmd_debug = LOG_LEVEL_QUIET;
	opt_1->warn_signal = 0;
	opt_1->warn_time   = 0;

	opt_1->pn_min_cpus    = NO_VAL;
	opt_1->pn_min_memory  = NO_VAL;
	opt_1->mem_per_cpu     = NO_VAL;
	opt_1->pn_min_tmp_disk= NO_VAL;

	opt_1->hold	    = false;
	opt_1->constraints	    = NULL;
	opt_1->gres	    = NULL;
	opt_1->contiguous	    = false;
	opt_1->hostfile	    = NULL;
	opt_1->nodelist	    = NULL;
	opt_1->exc_nodes	    = NULL;
	opt_1->max_launch_time = 120;/* 120 seconds to launch job             */
	opt_1->max_exit_timeout= 60; /* Warn user 60 seconds after task exit */
	/* Default launch msg timeout           */
	opt_1->msg_timeout     = slurm_get_msg_timeout();

	for (i=0; i<HIGHEST_DIMENSIONS; i++) {
		opt_1->conn_type[i]    = (uint16_t) NO_VAL;
		opt_1->geometry[i]	    = (uint16_t) NO_VAL;
	}
	opt_1->reboot          = false;
	opt_1->no_rotate	    = false;
	opt_1->blrtsimage = NULL;
	opt_1->linuximage = NULL;
	opt_1->mloaderimage = NULL;
	opt_1->ramdiskimage = NULL;

	opt_1->euid	    = (uid_t) -1;
	opt_1->egid	    = (gid_t) -1;

	opt_1->propagate	    = NULL;  /* propagate specific rlimits */

	opt_1->prolog = slurm_get_srun_prolog();
	opt_1->epilog = slurm_get_srun_epilog();
	opt_1->begin = (time_t)0;

	opt_1->task_prolog     = NULL;
	opt_1->task_epilog     = NULL;

	/*
	 * Reset some default values if running under a parallel debugger
	 */
	if ((opt_1->parallel_debug = _under_parallel_debugger())) {
		opt_1->max_launch_time = 120;
		opt_1->max_threads     = 1;
		pmi_server_max_threads(opt_1->max_threads);
		opt_1->msg_timeout     = 15;
	}

	opt_1->pty = false;
	opt_1->open_mode = 0;
	opt_1->acctg_freq = -1;
	opt_1->cpu_freq = NO_VAL;
	opt_1->reservation = NULL;
	opt_1->wckey = NULL;
	opt_1->req_switch = -1;
	opt_1->wait4switch = -1;
	opt_1->launcher_opts = NULL;
	opt_1->launch_cmd = false;
}

void _process_env_var_1(env_vars_t *e, const char *val, struct srun_options *opt_1)
{
	char *end = NULL;
	task_dist_states_t dt;

	bool mpi_initialized;
	debug2("now processing env var %s=%s", e->var, val);

	if (e->set_flag) {
		*((bool *) e->set_flag) = true;
	}

	switch (e->type) {
	case OPT_STRING:
		*((char **) e->arg) = xstrdup(val);
		break;
	case OPT_INT:
		if (val != NULL) {
			*((int *) e->arg) = (int) strtol(val, &end, 10);
			if (!(end && *end == '\0')) {
				error("%s=%s invalid. ignoring...",
				      e->var, val);
			}
		}
		break;

	case OPT_DISTRIB:
		if (strcmp(val, "unknown") == 0)
			break;	/* ignore it, passed from salloc */
		dt = verify_dist_type(val, &opt_1->plane_size);
		if (dt == SLURM_DIST_UNKNOWN) {
			error("\"%s=%s\" -- invalid distribution type. "
			      "ignoring...", e->var, val);
		} else
			opt_1->distribution = dt;
		break;

	case OPT_CPU_BIND:
		if (slurm_verify_cpu_bind(val, &opt_1->cpu_bind,
					  &opt_1->cpu_bind_type))
			exit(error_exit);
		break;

	case OPT_CPU_FREQ:
		if (cpu_freq_verify_param(val, &opt_1->cpu_freq))
			error("Invalid --cpu-freq argument: %s. Ignored", val);
		break;

	case OPT_MEM_BIND:
		if (slurm_verify_mem_bind(val, &opt_1->mem_bind,
					  &opt_1->mem_bind_type))
			exit(error_exit);
		break;

	case OPT_NODES:
		opt_1->nodes_set_env = get_resource_arg_range( val ,"OPT_NODES",
							     &opt_1->min_nodes,
							     &opt_1->max_nodes,
							     false);
		if (opt_1->nodes_set_env == false) {
			error("\"%s=%s\" -- invalid node count. ignoring...",
			      e->var, val);
		} else
			opt_1->nodes_set = opt_1->nodes_set_env;
		break;

	case OPT_OVERCOMMIT:
		opt_1->overcommit = true;
		break;

	case OPT_EXCLUSIVE:
		opt_1->exclusive = true;
		opt_1->shared = 0;
		break;

	case OPT_RESV_PORTS:
		if (val)
			opt_1->resv_port_cnt = strtol(val, NULL, 10);
		else
			opt_1->resv_port_cnt = 0;
		break;

	case OPT_OPEN_MODE:
		if ((val[0] == 'a') || (val[0] == 'A'))
			opt_1->open_mode = OPEN_MODE_APPEND;
		else if ((val[0] == 't') || (val[0] == 'T'))
			opt_1->open_mode = OPEN_MODE_TRUNCATE;
		else
			error("Invalid SLURM_OPEN_MODE: %s. Ignored", val);
		break;

	case OPT_CONN_TYPE:
		verify_conn_type(val, opt_1->conn_type);
		break;

	case OPT_NO_ROTATE:
		opt_1->no_rotate = true;
		break;

	case OPT_GEOMETRY:
		if (verify_geometry(val, opt_1->geometry)) {
			error("\"%s=%s\" -- invalid geometry, ignoring...",
			      e->var, val);
		}
		break;

	case OPT_IMMEDIATE:
		if (val)
			opt_1->immediate = strtol(val, NULL, 10);
		else
			opt_1->immediate = DEFAULT_IMMEDIATE;
		break;

	case OPT_MPI:
		//xfree(opt_1->mpi_type);
		opt_1->mpi_type = xstrdup(val);
		if (mpi_hook_client_init((char *)val) == SLURM_ERROR) {
			error("\"%s=%s\" -- invalid MPI type, "
			      "--mpi=list for acceptable types.",
			      e->var, val);
			exit(error_exit);
		}
		mpi_initialized = true;
		break;

	case OPT_SIGNAL:
		if (get_signal_opts((char *)val, &opt_1->warn_signal,
				    &opt_1->warn_time)) {
			error("Invalid signal specification: %s", val);
			exit(error_exit);
		}
		break;

	case OPT_TIME_VAL:
		opt_1->wait4switch = time_str2secs(val);
		break;

	default:
		/* do nothing */
		break;
	}
}

void _opt_env_1(opt_t *opt_1)
{
	char *val = NULL;
	env_vars_t *e = _gene_envs(opt_1);
	while (e->var)
	{
		if ((val = getenv(e->var)) != NULL)
		{
			_process_env_var_1(e, val, opt_1);
		}
		e++;
	}
}

int
_get_int(const char *arg, const char *what, bool positive)
{
	char *p;
	long int result = strtol(arg, &p, 10);

	if ((*p != '\0') || (result < 0L)
	||  (positive && (result <= 0L))) {
		error ("Invalid numeric value \"%s\" for %s.", arg, what);
		exit(error_exit);
	} else if (result > INT_MAX) {
		error ("Numeric argument (%ld) to big for %s.", result, what);
	} else if (result < INT_MIN) {
		error ("Numeric argument %ld to small for %s.", result, what);
	}

	return (int) result;
}

bool _valid_node_list(char **node_list_pptr, struct srun_options *opt_1)
{
	int count = NO_VAL;

	/* If we are using Arbitrary and we specified the number of
	   procs to use then we need exactly this many since we are
	   saying, lay it out this way!  Same for max and min nodes.
	   Other than that just read in as many in the hostfile */
	if(opt_1->ntasks_set)
		count = opt_1->ntasks;
	else if(opt_1->nodes_set) {
		if(opt_1->max_nodes)
			count = opt_1->max_nodes;
		else if(opt_1->min_nodes)
			count = opt_1->min_nodes;
	}

	return verify_node_list(node_list_pptr, opt_1->distribution, count);
}

void set_options_1(const int argc, char **argv, struct srun_options *opt_1)
{
	int opt_char, option_index = 0, max_val = 0, tmp_int;
	struct utsname name;
	static struct option long_options[] = {
		{"account",       required_argument, 0, 'A'},
		{"extra-node-info", required_argument, 0, 'B'},
		{"cpus-per-task", required_argument, 0, 'c'},
		{"constraint",    required_argument, 0, 'C'},
		{"dependency",    required_argument, 0, 'd'},
		{"chdir",         required_argument, 0, 'D'},
		{"error",         required_argument, 0, 'e'},
		{"preserve-env",  no_argument,       0, 'E'},
		{"preserve-slurm-env", no_argument,  0, 'E'},
		{"geometry",      required_argument, 0, 'g'},
		{"hold",          no_argument,       0, 'H'},
		{"input",         required_argument, 0, 'i'},
		{"immediate",     optional_argument, 0, 'I'},
		{"join",          no_argument,       0, 'j'},
		{"job-name",      required_argument, 0, 'J'},
		{"no-kill",       no_argument,       0, 'k'},
		{"kill-on-bad-exit", optional_argument, 0, 'K'},
		{"label",         no_argument,       0, 'l'},
		{"licenses",      required_argument, 0, 'L'},
		{"distribution",  required_argument, 0, 'm'},
		{"ntasks",        required_argument, 0, 'n'},
		{"nodes",         required_argument, 0, 'N'},
		{"output",        required_argument, 0, 'o'},
		{"overcommit",    no_argument,       0, 'O'},
		{"partition",     required_argument, 0, 'p'},
		{"quit-on-interrupt", no_argument,   0, 'q'},
		{"quiet",            no_argument,    0, 'Q'},
		{"relative",      required_argument, 0, 'r'},
		{"no-rotate",     no_argument,       0, 'R'},
		{"share",         no_argument,       0, 's'},
		{"time",          required_argument, 0, 't'},
		{"threads",       required_argument, 0, 'T'},
		{"unbuffered",    no_argument,       0, 'u'},
		{"verbose",       no_argument,       0, 'v'},
		{"version",       no_argument,       0, 'V'},
		{"nodelist",      required_argument, 0, 'w'},
		{"wait",          required_argument, 0, 'W'},
		{"exclude",       required_argument, 0, 'x'},
		{"disable-status", no_argument,      0, 'X'},
		{"no-allocate",   no_argument,       0, 'Z'},
		{"acctg-freq",       required_argument, 0, LONG_OPT_ACCTG_FREQ},
		{"alps",             required_argument, 0, LONG_OPT_ALPS},
		{"begin",            required_argument, 0, LONG_OPT_BEGIN},
		{"blrts-image",      required_argument, 0, LONG_OPT_BLRTS_IMAGE},
		{"checkpoint",       required_argument, 0, LONG_OPT_CHECKPOINT},
		{"checkpoint-dir",   required_argument, 0, LONG_OPT_CHECKPOINT_DIR},
		{"cnload-image",     required_argument, 0, LONG_OPT_LINUX_IMAGE},
		{"comment",          required_argument, 0, LONG_OPT_COMMENT},
		{"conn-type",        required_argument, 0, LONG_OPT_CONNTYPE},
		{"contiguous",       no_argument,       0, LONG_OPT_CONT},
		{"cores-per-socket", required_argument, 0, LONG_OPT_CORESPERSOCKET},
		{"cpu_bind",         required_argument, 0, LONG_OPT_CPU_BIND},
		{"cpu-freq",         required_argument, 0, LONG_OPT_CPU_FREQ},
		{"debugger-test",    no_argument,       0, LONG_OPT_DEBUG_TS},
		{"epilog",           required_argument, 0, LONG_OPT_EPILOG},
		{"exclusive",        no_argument,       0, LONG_OPT_EXCLUSIVE},
		{"get-user-env",     optional_argument, 0, LONG_OPT_GET_USER_ENV},
		{"gid",              required_argument, 0, LONG_OPT_GID},
		{"gres",             required_argument, 0, LONG_OPT_GRES},
		{"help",             no_argument,       0, LONG_OPT_HELP},
		{"hint",             required_argument, 0, LONG_OPT_HINT},
		{"ioload-image",     required_argument, 0, LONG_OPT_RAMDISK_IMAGE},
		{"jobid",            required_argument, 0, LONG_OPT_JOBID},
		{"linux-image",      required_argument, 0, LONG_OPT_LINUX_IMAGE},
		{"launch-cmd",       no_argument,       0, LONG_OPT_LAUNCH_CMD},
		{"launcher-opts",      required_argument, 0, LONG_OPT_LAUNCHER_OPTS},
		{"mail-type",        required_argument, 0, LONG_OPT_MAIL_TYPE},
		{"mail-user",        required_argument, 0, LONG_OPT_MAIL_USER},
		{"max-exit-timeout", required_argument, 0, LONG_OPT_XTO},
		{"max-launch-time",  required_argument, 0, LONG_OPT_LAUNCH},
		{"mem",              required_argument, 0, LONG_OPT_MEM},
		{"mem-per-cpu",      required_argument, 0, LONG_OPT_MEM_PER_CPU},
		{"mem_bind",         required_argument, 0, LONG_OPT_MEM_BIND},
		{"mincores",         required_argument, 0, LONG_OPT_MINCORES},
		{"mincpus",          required_argument, 0, LONG_OPT_MINCPUS},
		{"minsockets",       required_argument, 0, LONG_OPT_MINSOCKETS},
		{"minthreads",       required_argument, 0, LONG_OPT_MINTHREADS},
		{"mloader-image",    required_argument, 0, LONG_OPT_MLOADER_IMAGE},
		{"mpi",              required_argument, 0, LONG_OPT_MPI},
		{"msg-timeout",      required_argument, 0, LONG_OPT_TIMEO},
		{"multi-prog",       no_argument,       0, LONG_OPT_MULTI},
		{"network",          required_argument, 0, LONG_OPT_NETWORK},
		{"nice",             optional_argument, 0, LONG_OPT_NICE},
		{"ntasks-per-core",  required_argument, 0, LONG_OPT_NTASKSPERCORE},
		{"ntasks-per-node",  required_argument, 0, LONG_OPT_NTASKSPERNODE},
		{"ntasks-per-socket",required_argument, 0, LONG_OPT_NTASKSPERSOCKET},
		{"open-mode",        required_argument, 0, LONG_OPT_OPEN_MODE},
		{"prolog",           required_argument, 0, LONG_OPT_PROLOG},
		{"propagate",        optional_argument, 0, LONG_OPT_PROPAGATE},
		{"pty",              no_argument,       0, LONG_OPT_PTY},
		{"qos",		     required_argument, 0, LONG_OPT_QOS},
		{"ramdisk-image",    required_argument, 0, LONG_OPT_RAMDISK_IMAGE},
		{"reboot",           no_argument,       0, LONG_OPT_REBOOT},
		{"reservation",      required_argument, 0, LONG_OPT_RESERVATION},
		{"restart-dir",      required_argument, 0, LONG_OPT_RESTART_DIR},
		{"resv-ports",       optional_argument, 0, LONG_OPT_RESV_PORTS},
		{"runjob-opts",      required_argument, 0, LONG_OPT_LAUNCHER_OPTS},
		{"signal",	     required_argument, 0, LONG_OPT_SIGNAL},
		{"slurmd-debug",     required_argument, 0, LONG_OPT_DEBUG_SLURMD},
		{"sockets-per-node", required_argument, 0, LONG_OPT_SOCKETSPERNODE},
		{"switches",         required_argument, 0, LONG_OPT_REQ_SWITCH},
		{"task-epilog",      required_argument, 0, LONG_OPT_TASK_EPILOG},
		{"task-prolog",      required_argument, 0, LONG_OPT_TASK_PROLOG},
		{"tasks-per-node",   required_argument, 0, LONG_OPT_NTASKSPERNODE},
		{"test-only",        no_argument,       0, LONG_OPT_TEST_ONLY},
		{"time-min",         required_argument, 0, LONG_OPT_TIME_MIN},
		{"threads-per-core", required_argument, 0, LONG_OPT_THREADSPERCORE},
		{"tmp",              required_argument, 0, LONG_OPT_TMP},
		{"uid",              required_argument, 0, LONG_OPT_UID},
		{"usage",            no_argument,       0, LONG_OPT_USAGE},
		{"wckey",            required_argument, 0, LONG_OPT_WCKEY},
		{NULL,               0,                 0, 0}
	};
	char *opt_string = "+A:B:c:C:d:D:e:Eg:hHi:I::jJ:kK::lL:m:n:N:"
		"o:Op:P:qQr:Rst:T:uU:vVw:W:x:XZ";
	char *pos_delimit;
#ifdef HAVE_PTY_H
	char *tmp_str;
#endif
	struct option *optz = spank_option_table_create (long_options);

	if (!optz) {
		error("Unable to create option table");
		exit(error_exit);
	}

	if (opt_1->progname == NULL)
		opt_1->progname = xbasename(argv[0]);
	else
		error("opt_1.progname is already set.");
	pthread_mutex_lock(&opt_mutex);
	optind = 0;
	while((opt_char = getopt_long(argc, argv, opt_string,
				      optz, &option_index)) != -1) {
		switch (opt_char) {

		case (int)'?':
			fprintf(stderr,
				"Try \"srun --help\" for more information\n");
			exit(error_exit);
			break;
		case (int)'A':
		case (int)'U':	/* backwards compatibility */
			xfree(opt_1->account);
			opt_1->account = xstrdup(optarg);
			break;
		case (int)'B':
			opt_1->extra_set = verify_socket_core_thread_count(
						optarg,
						&opt_1->sockets_per_node,
						&opt_1->cores_per_socket,
						&opt_1->threads_per_core,
						&opt_1->cpu_bind_type);

			if (opt_1->extra_set == false) {
				error("invalid resource allocation -B `%s'",
					optarg);
				exit(error_exit);
			}
			break;
		case (int)'c':
			tmp_int = _get_int(optarg, "cpus-per-task", false);
			if (opt_1->cpus_set && (tmp_int > opt_1->cpus_per_task)) {
				info("Job step's --cpus-per-task value exceeds"
				     " that of job (%d > %d). Job step may "
				     "never run.", tmp_int, opt_1->cpus_per_task);
			}
			opt_1->cpus_set = true;
			opt_1->cpus_per_task = tmp_int;
			break;
		case (int)'C':
			xfree(opt_1->constraints);
			opt_1->constraints = xstrdup(optarg);
			break;
		case (int)'d':
			xfree(opt_1->dependency);
			opt_1->dependency = xstrdup(optarg);
			break;
		case (int)'D':
			opt_1->cwd_set = true;
			xfree(opt_1->cwd);
			opt_1->cwd = xstrdup(optarg);
			break;
		case (int)'e':
			if (opt_1->pty) {
				fatal("--error incompatible with --pty "
				      "option");
				exit(error_exit);
			}
			xfree(opt_1->efname);
			if (strcasecmp(optarg, "none") == 0)
				opt_1->efname = xstrdup("/dev/null");
			else
				opt_1->efname = xstrdup(optarg);
			break;
		case (int)'E':
			opt_1->preserve_env = true;
			break;
		case (int)'g':
			if (verify_geometry(optarg, opt_1->geometry))
				exit(error_exit);
			break;
		case (int)'H':
			opt_1->hold = true;
			break;
		case (int)'i':
			if (opt_1->pty) {
				fatal("--input incompatible with "
				      "--pty option");
				exit(error_exit);
			}
			xfree(opt_1->ifname);
			if (strcasecmp(optarg, "none") == 0)
				opt_1->ifname = xstrdup("/dev/null");
			else
				opt_1->ifname = xstrdup(optarg);
			break;
		case (int)'I':
			if (optarg)
				opt_1->immediate = strtol(optarg, NULL, 10);
			else
				opt_1->immediate = DEFAULT_IMMEDIATE;
			break;
		case (int)'j':
			opt_1->join = true;
			break;
		case (int)'J':
			opt_1->job_name_set_cmd = true;
			xfree(opt_1->job_name);
			opt_1->job_name = xstrdup(optarg);
			break;
		case (int)'k':
			opt_1->no_kill = true;
			break;
		case (int)'K':
			if (optarg)
				opt_1->kill_bad_exit = strtol(optarg, NULL, 10);
			else
				opt_1->kill_bad_exit = 1;
			break;
		case (int)'l':
			opt_1->labelio = true;
			break;
		case 'L':
			xfree(opt_1->licenses);
			opt_1->licenses = xstrdup(optarg);
			break;
		case (int)'m':
			opt_1->distribution = verify_dist_type(optarg,
							     &opt_1->plane_size);
			if (opt_1->distribution == SLURM_DIST_UNKNOWN) {
				error("distribution type `%s' "
				      "is not recognized", optarg);
				exit(error_exit);
			}
			break;
		case (int)'n':
			opt_1->ntasks_set = true;
			opt_1->ntasks =
				_get_int(optarg, "number of tasks", true);
			break;
		case (int)'N':
			opt_1->nodes_set_opt =
				get_resource_arg_range( optarg,
							"requested node count",
							&opt_1->min_nodes,
							&opt_1->max_nodes, true );

			if (opt_1->nodes_set_opt == false) {
				error("invalid resource allocation -N `%s'",
				      optarg);
				exit(error_exit);
			} else
				opt_1->nodes_set = opt_1->nodes_set_opt;
			break;
		case (int)'o':
			if (opt_1->pty) {
				error("--output incompatible with --pty "
				      "option");
				exit(error_exit);
			}
			xfree(opt_1->ofname);
			if (strcasecmp(optarg, "none") == 0)
				opt_1->ofname = xstrdup("/dev/null");
			else
				opt_1->ofname = xstrdup(optarg);
			break;
		case (int)'O':
			opt_1->overcommit = true;
			break;
		case (int)'p':
			xfree(opt_1->partition);
			opt_1->partition = xstrdup(optarg);
			break;
		case (int)'P':
			verbose("-P option is deprecated, use -d instead");
			xfree(opt_1->dependency);
			opt_1->dependency = xstrdup(optarg);
			break;
		case (int)'q':
			opt_1->quit_on_intr = true;
			break;
		case (int) 'Q':
			opt_1->quiet++;
			break;
		case (int)'r':
			opt_1->relative = _get_int(optarg, "relative", false);
			opt_1->relative_set = true;
			break;
		case (int)'R':
			opt_1->no_rotate = true;
			break;
		case (int)'s':
			opt_1->shared = 1;
			break;
		case (int)'t':
			xfree(opt_1->time_limit_str);
			opt_1->time_limit_str = xstrdup(optarg);
			break;
		case (int)'T':
			opt_1->max_threads =
				_get_int(optarg, "max_threads", true);
			pmi_server_max_threads(opt_1->max_threads);
			break;
		case (int)'u':
			opt_1->unbuffered = true;
			break;
		case (int)'v':
			_verbose++;
			break;
		case (int)'V':
			print_slurm_version();
			exit(0);
			break;
		case (int)'w':
			xfree(opt_1->nodelist);
			opt_1->nodelist = xstrdup(optarg);
			break;
		case (int)'W':
			opt_1->max_wait = _get_int(optarg, "wait", false);
			break;
		case (int)'x':
			xfree(opt_1->exc_nodes);
			opt_1->exc_nodes = xstrdup(optarg);
			if (!_valid_node_list(&opt_1->exc_nodes, opt_1))
				exit(error_exit);
			break;
		case (int)'X':
			opt_1->disable_status = true;
			break;
		case (int)'Z':
			opt_1->no_alloc = true;
			uname(&name);
			if (strcasecmp(name.sysname, "AIX") == 0)
				opt_1->network = xstrdup("ip");
			break;
		case LONG_OPT_CONT:
			opt_1->contiguous = true;
			break;
        case LONG_OPT_EXCLUSIVE:
			opt_1->exclusive = true;
			opt_1->shared = 0;
            break;
        case LONG_OPT_CPU_BIND:
			if (slurm_verify_cpu_bind(optarg, &opt_1->cpu_bind,
						  &opt_1->cpu_bind_type))
			exit(error_exit);
			break;
		case LONG_OPT_LAUNCH_CMD:
			opt_1->launch_cmd = true;
			break;
		case LONG_OPT_MEM_BIND:
			if (slurm_verify_mem_bind(optarg, &opt_1->mem_bind,
						  &opt_1->mem_bind_type))
				exit(error_exit);
			break;
		case LONG_OPT_MINCPUS:
			opt_1->pn_min_cpus = _get_int(optarg, "mincpus", true);
			break;
		case LONG_OPT_MINCORES:
			verbose("mincores option has been deprecated, use "
				"cores-per-socket");
			opt_1->cores_per_socket = _get_int(optarg,
							"mincores", true);
			if (opt_1->cores_per_socket < 0) {
				error("invalid mincores constraint %s",
				      optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_MINSOCKETS:
			verbose("minsockets option has been deprecated, use "
				"sockets-per-node");
			opt_1->sockets_per_node = _get_int(optarg,
							"minsockets",true);
			if (opt_1->sockets_per_node < 0) {
				error("invalid minsockets constraint %s",
				      optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_MINTHREADS:
			verbose("minthreads option has been deprecated, use "
				"threads-per-core");
			opt_1->threads_per_core = _get_int(optarg,
							"minthreads",true);
			if (opt_1->threads_per_core < 0) {
				error("invalid minthreads constraint %s",
				      optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_MEM:
			opt_1->pn_min_memory = (int) str_to_mbytes(optarg);
			if (opt_1->pn_min_memory < 0) {
				error("invalid memory constraint %s",
				      optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_MEM_PER_CPU:
			opt_1->mem_per_cpu = (int) str_to_mbytes(optarg);
			if (opt_1->mem_per_cpu < 0) {
				error("invalid memory constraint %s",
				      optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_MPI:
			xfree(opt_1->mpi_type);
			opt_1->mpi_type = xstrdup(optarg);
			if (mpi_hook_client_init((char *)optarg)
			    == SLURM_ERROR) {
				error("\"--mpi=%s\" -- long invalid MPI type, "
				      "--mpi=list for acceptable types.",
				      optarg);
				exit(error_exit);
			}
			//mpi_initialized = true;
			break;
		case LONG_OPT_RESV_PORTS:
			if (optarg)
				opt_1->resv_port_cnt = strtol(optarg, NULL, 10);
			else
				opt_1->resv_port_cnt = 0;
			break;
		case LONG_OPT_TMP:
			opt_1->pn_min_tmp_disk = str_to_mbytes(optarg);
			if (opt_1->pn_min_tmp_disk < 0) {
				error("invalid tmp value %s", optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_JOBID:
			opt_1->jobid = _get_int(optarg, "jobid", true);
			opt_1->jobid_set = true;
			break;
		case LONG_OPT_TIMEO:
			opt_1->msg_timeout =
				_get_int(optarg, "msg-timeout", true);
			break;
		case LONG_OPT_LAUNCH:
			opt_1->max_launch_time =
				_get_int(optarg, "max-launch-time", true);
			break;
		case LONG_OPT_XTO:
			opt_1->max_exit_timeout =
				_get_int(optarg, "max-exit-timeout", true);
			break;
		case LONG_OPT_UID:
			if (opt_1->euid != (uid_t) -1) {
				error("duplicate --uid option");
				exit(error_exit);
			}
			if (uid_from_string (optarg, &opt_1->euid) < 0) {
				error("--uid=\"%s\" invalid", optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_GID:
			if (opt_1->egid != (gid_t) -1) {
				error("duplicate --gid option");
				exit(error_exit);
			}
			if (gid_from_string (optarg, &opt_1->egid) < 0) {
				error("--gid=\"%s\" invalid", optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_DEBUG_SLURMD:
			opt_1->slurmd_debug =
				_get_int(optarg, "slurmd-debug", false);
			break;
		case LONG_OPT_DEBUG_TS:
			opt_1->debugger_test    = true;
			/* make other parameters look like debugger
			 * is really attached */
			opt_1->parallel_debug   = true;
			opt_1->max_launch_time = 120;
			opt_1->max_threads     = 1;
			pmi_server_max_threads(opt_1->max_threads);
			opt_1->msg_timeout     = 15;
			break;
		case 'h':
		case LONG_OPT_HELP:
			//_help();
			exit(0);
		case LONG_OPT_USAGE:
			//_usage();
			exit(0);
		case LONG_OPT_CONNTYPE:
			verify_conn_type(optarg, opt_1->conn_type);
			break;
		case LONG_OPT_TEST_ONLY:
			opt_1->test_only = true;
			break;
		case LONG_OPT_NETWORK:
			xfree(opt_1->network);
			opt_1->network = xstrdup(optarg);
			setenv("SLURM_NETWORK", opt_1->network, 1);
			break;
		case LONG_OPT_PROPAGATE:
			xfree(opt_1->propagate);
			if (optarg)
				opt_1->propagate = xstrdup(optarg);
			else
				opt_1->propagate = xstrdup("ALL");
			break;
		case LONG_OPT_PROLOG:
			xfree(opt_1->prolog);
			opt_1->prolog = xstrdup(optarg);
			break;
		case LONG_OPT_EPILOG:
			xfree(opt_1->epilog);
			opt_1->epilog = xstrdup(optarg);
			break;
		case LONG_OPT_BEGIN:
			opt_1->begin = parse_time(optarg, 0);
			if (errno == ESLURM_INVALID_TIME_VALUE) {
				error("Invalid time specification %s",
				      optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_MAIL_TYPE:
			opt_1->mail_type |= parse_mail_type(optarg);
			if (opt_1->mail_type == 0) {
				error("--mail-type=%s invalid", optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_MAIL_USER:
			xfree(opt_1->mail_user);
			opt_1->mail_user = xstrdup(optarg);
			break;
		case LONG_OPT_TASK_PROLOG:
			xfree(opt_1->task_prolog);
			opt_1->task_prolog = xstrdup(optarg);
			break;
		case LONG_OPT_TASK_EPILOG:
			xfree(opt_1->task_epilog);
			opt_1->task_epilog = xstrdup(optarg);
			break;
		case LONG_OPT_NICE:
			if (optarg)
				opt_1->nice = strtol(optarg, NULL, 10);
			else
				opt_1->nice = 100;
			if (abs(opt_1->nice) > NICE_OFFSET) {
				error("Invalid nice value, must be between "
				      "-%d and %d", NICE_OFFSET, NICE_OFFSET);
				exit(error_exit);
			}
			if (opt_1->nice < 0) {
				uid_t my_uid = getuid();
				if ((my_uid != 0) &&
				    (my_uid != slurm_get_slurm_user_id())) {
					error("Nice value must be non-negative, "
					      "value ignored");
					opt_1->nice = 0;
				}
			}
			break;
		case LONG_OPT_MULTI:
			opt_1->multi_prog = true;
			break;
		case LONG_OPT_COMMENT:
			xfree(opt_1->comment);
			opt_1->comment = xstrdup(optarg);
			break;
		case LONG_OPT_QOS:
			xfree(opt_1->qos);
			opt_1->qos = xstrdup(optarg);
			break;
		case LONG_OPT_SOCKETSPERNODE:
			max_val = 0;
			get_resource_arg_range( optarg, "sockets-per-node",
						&opt_1->sockets_per_node,
						&max_val, true );
			if ((opt_1->sockets_per_node == 1) &&
			    (max_val == INT_MAX))
				opt_1->sockets_per_node = NO_VAL;
			break;
		case LONG_OPT_CORESPERSOCKET:
			max_val = 0;
			get_resource_arg_range( optarg, "cores-per-socket",
						&opt_1->cores_per_socket,
						&max_val, true );
			if ((opt_1->cores_per_socket == 1) &&
			    (max_val == INT_MAX))
				opt_1->cores_per_socket = NO_VAL;
			break;
		case LONG_OPT_THREADSPERCORE:
			max_val = 0;
			get_resource_arg_range( optarg, "threads-per-core",
						&opt_1->threads_per_core,
						&max_val, true );
			if ((opt_1->threads_per_core == 1) &&
			    (max_val == INT_MAX))
				opt_1->threads_per_core = NO_VAL;
			break;
		case LONG_OPT_NTASKSPERNODE:
			opt_1->ntasks_per_node = _get_int(optarg, "ntasks-per-node",
				true);
			break;
		case LONG_OPT_NTASKSPERSOCKET:
			opt_1->ntasks_per_socket = _get_int(optarg,
				"ntasks-per-socket", true);
			break;
		case LONG_OPT_NTASKSPERCORE:
			opt_1->ntasks_per_core = _get_int(optarg, "ntasks-per-core",
				true);
			break;
		case LONG_OPT_HINT:
			/* Keep after other options filled in */
			if (verify_hint(optarg,
					&opt_1->sockets_per_node,
					&opt_1->cores_per_socket,
					&opt_1->threads_per_core,
					&opt_1->ntasks_per_core,
					&opt_1->cpu_bind_type)) {
				exit(error_exit);
			}
			break;
		case LONG_OPT_BLRTS_IMAGE:
			xfree(opt_1->blrtsimage);
			opt_1->blrtsimage = xstrdup(optarg);
			break;
		case LONG_OPT_LINUX_IMAGE:
			xfree(opt_1->linuximage);
			opt_1->linuximage = xstrdup(optarg);
			break;
		case LONG_OPT_MLOADER_IMAGE:
			xfree(opt_1->mloaderimage);
			opt_1->mloaderimage = xstrdup(optarg);
			break;
		case LONG_OPT_RAMDISK_IMAGE:
			xfree(opt_1->ramdiskimage);
			opt_1->ramdiskimage = xstrdup(optarg);
			break;
		case LONG_OPT_REBOOT:
			opt_1->reboot = true;
			break;
		case LONG_OPT_GET_USER_ENV:
			error("--get-user-env is no longer supported in srun, "
			      "use sbatch");
			break;
		case LONG_OPT_PTY:
#ifdef HAVE_PTY_H
			opt_1->pty = true;
			opt_1->unbuffered = true;	/* implicit */
			if (opt_1->ifname)
				tmp_str = "--input";
			else if (opt_1->ofname)
				tmp_str = "--output";
			else if (opt_1->efname)
				tmp_str = "--error";
			else
				tmp_str = NULL;
			if (tmp_str) {
				error("%s incompatible with --pty option",
				      tmp_str);
				exit(error_exit);
			}
#else
			error("--pty not currently supported on this system "
			      "type");
#endif
			break;
		case LONG_OPT_CHECKPOINT:
			xfree(opt_1->ckpt_interval_str);
			opt_1->ckpt_interval_str = xstrdup(optarg);
			break;
		case LONG_OPT_OPEN_MODE:
			if ((optarg[0] == 'a') || (optarg[0] == 'A'))
				opt_1->open_mode = OPEN_MODE_APPEND;
			else if ((optarg[0] == 't') || (optarg[0] == 'T'))
				opt_1->open_mode = OPEN_MODE_TRUNCATE;
			else {
				error("Invalid --open-mode argument: %s. Ignored",
				      optarg);
			}
			break;
		case LONG_OPT_ACCTG_FREQ:
			opt_1->acctg_freq = _get_int(optarg, "acctg-freq",
                                false);
			break;
		case LONG_OPT_CPU_FREQ:
		        if (cpu_freq_verify_param(optarg, &opt_1->cpu_freq))
				error("Invalid --cpu-freq argument: %s. Ignored",
				      optarg);
			break;
		case LONG_OPT_WCKEY:
			xfree(opt_1->wckey);
			opt_1->wckey = xstrdup(optarg);
			break;
		case LONG_OPT_RESERVATION:
			xfree(opt_1->reservation);
			opt_1->reservation = xstrdup(optarg);
			break;
		case LONG_OPT_LAUNCHER_OPTS:
			xfree(opt_1->launcher_opts);
			opt_1->launcher_opts = xstrdup(optarg);
			break;
		case LONG_OPT_CHECKPOINT_DIR:
			xfree(opt_1->ckpt_dir);
			opt_1->ckpt_dir = xstrdup(optarg);
			break;
		case LONG_OPT_RESTART_DIR:
			xfree(opt_1->restart_dir);
			opt_1->restart_dir = xstrdup(optarg);
			break;
		case LONG_OPT_SIGNAL:
			if (get_signal_opts(optarg, &opt_1->warn_signal,
					    &opt_1->warn_time)) {
				error("Invalid signal specification: %s",
				      optarg);
				exit(error_exit);
			}
			break;
		case LONG_OPT_TIME_MIN:
			xfree(opt_1->time_min_str);
			opt_1->time_min_str = xstrdup(optarg);
			break;
		case LONG_OPT_GRES:
			if (!strcasecmp(optarg, "help") ||
			    !strcasecmp(optarg, "list")) {
				print_gres_help();
				exit(0);
			}
			xfree(opt_1->gres);
			opt_1->gres = xstrdup(optarg);
			break;
		case LONG_OPT_ALPS:
			verbose("Not running ALPS. --alps option ignored.");
			break;
		case LONG_OPT_REQ_SWITCH:
			pos_delimit = strstr(optarg,"@");
			if (pos_delimit != NULL) {
				pos_delimit[0] = '\0';
				pos_delimit++;
				opt_1->wait4switch = time_str2secs(pos_delimit);
			}
			opt_1->req_switch = _get_int(optarg, "switches",
				true);
			break;
		default:
			if (spank_process_option (opt_char, optarg) < 0) {
				exit(error_exit);
			}
			break;
		}
	}

	pthread_mutex_unlock(&opt_mutex);
	spank_option_table_destroy (optz);
}

int launch_g_setup_srun_opt_1(char **rest, struct srun_options *opt_1)
{
	if (opt_1->debugger_test && opt_1->parallel_debug)
		MPIR_being_debugged = 1;
	opt_1->argv = (char**)xmalloc((opt_1->argc + 2) * sizeof(char*));
	return 0;
}

void _opt_args_1(int argc, char **argv, struct srun_options *opt_1)
{
	int i, command_pos = 0, command_args = 0;
	char **rest = NULL;

	set_options_1(argc, argv, opt_1);

	if ((opt_1->pn_min_memory > -1) && (opt_1->mem_per_cpu > -1)) {
		if (opt_1->pn_min_memory < opt_1->mem_per_cpu) {
			info("mem < mem-per-cpu - resizing mem to be equal "
			     "to mem-per-cpu");
			opt_1->pn_min_memory = opt_1->mem_per_cpu;
		}
	}

	/* Check to see if user has specified enough resources to
	 * satisfy the plane distribution with the specified
	 * plane_size.
	 * if (n/plane_size < N) and ((N-1) * plane_size >= n) -->
	 * problem Simple check will not catch all the problem/invalid
	 * cases.
	 * The limitations of the plane distribution in the cons_res
	 * environment are more extensive and are documented in the
	 * SLURM reference guide.  */
	if (opt_1->distribution == SLURM_DIST_PLANE && opt_1->plane_size) {
		if ((opt_1->ntasks/opt_1->plane_size) < opt_1->min_nodes) {
			if (((opt_1->min_nodes-1)*opt_1->plane_size) >= opt_1->ntasks) {
#if(0)
				info("Too few processes ((n/plane_size) %d < N %d) "
				     "and ((N-1)*(plane_size) %d >= n %d)) ",
				     opt_1->ntasks/opt_1->plane_size, opt_1->min_nodes,
				     (opt_1->min_nodes-1)*opt_1->plane_size, opt_1->ntasks);
#endif
				error("Too few processes for the requested "
				      "{plane,node} distribution");
				exit(error_exit);
			}
		}
	}

#ifdef HAVE_AIX
	if (opt_1->network == NULL) {
		opt_1->network = "us,sn_all,bulk_xfer";
		setenv("SLURM_NETWORK", opt_1->network, 1);
	}
#endif
	if (opt_1->dependency)
		setenvfs("SLURM_JOB_DEPENDENCY=%s", opt_1->dependency);

	if (opt_1->nodelist && (!opt_1->test_only)) {
#ifdef HAVE_BG
		info("\tThe nodelist option should only be used if\n"
		     "\tthe block you are asking for can be created.\n"
		     "\tIt should also include all the midplanes you\n"
		     "\twant to use, partial lists will not work correctly.\n"
		     "\tPlease consult smap before using this option\n"
		     "\tor your job may be stuck with no way to run.");
#endif
	}

	opt_1->argc = 0;
	if (optind < argc)
	{
		rest = argv + optind;
		while (rest[opt_1->argc] != NULL)
		{
			opt_1->argc++;
		}
	}

	command_args = opt_1->argc;

	if (!rest)
	{
		pthread_mutex_lock(&num_job_fail_mutex);
		num_job_fail++;
		pthread_mutex_unlock(&num_job_fail_mutex);
		pthread_exit(NULL);
	}

#if defined HAVE_BG && !defined HAVE_BG_L_P
	/* Since this is needed on an emulated system don't put this code in
	 * the launch plugin.
	 */
	//bg_figure_nodes_tasks();
#endif

	/*if (launch_init() != SLURM_SUCCESS) {
		fatal("Unable to load launch plugin, check LaunchType "
		      "configuration");
	}*/
	command_pos = launch_g_setup_srun_opt_1(rest, opt_1);

	/* Since this is needed on an emulated system don't put this code in
	 * the launch plugin.
	 */
#if defined HAVE_BG && !defined HAVE_BG_L_P
	if (opt_1->test_only && !opt_1->jobid_set && (opt_1->jobid != NO_VAL)) {
		/* Do not perform allocate test, only disable use of "runjob" */
		opt_1->test_only = false;
	}

#endif
	/* make sure we have allocated things correctly */
	xassert((command_pos + command_args) <= opt_1->argc);

	for (i = command_pos; i < opt_1->argc; i++) {
		if (!rest[i-command_pos])
			break;
		opt_1->argv[i] = xstrdup(rest[i-command_pos]);
	}
	opt_1->argv[i] = NULL;	/* End of argv's (for possible execv) */
	/*if (!launch_g_handle_multi_prog_verify(command_pos)
	    && (opt_1->argc > command_pos)) {
		char *fullpath;

		if ((fullpath = search_path(opt_1->cwd,
						opt_1->argv[command_pos],
					    false, X_OK))) {
			xfree(opt_1->argv[command_pos]);
			opt_1->argv[command_pos] = fullpath;
		}
	}
	/* for (i=0; i<opt.argc; i++) */
	/* 	info("%d is '%s'", i, opt.argv[i]); */
}

int initialize_and_process_args_1(int argc, char *argv[], opt_t* opt_1)
{
	_opt_default_1(opt_1);
	_opt_env_1(opt_1);
	_opt_args_1(argc, argv, opt_1);
	return 1;
}

int launch_tasks(launch_tasks_request_msg_t *launch_msg, char *nodelist)
{
#ifdef HAVE_FRONT_END
	slurm_cred_arg_t cred_args;
#endif
	slurm_msg_t msg;
	List ret_list = NULL;
	ListIterator ret_itr;
	ret_data_info_t *ret_data = NULL;
	int rc = SLURM_SUCCESS;
	int tot_rc = SLURM_SUCCESS;

	slurm_msg_t_init(&msg);
	msg.msg_type = REQUEST_LAUNCH_TASKS;
	msg.data = launch_msg;

#ifdef HAVE_FRONT_END
	slurm_cred_get_args(ctx->step_resp->cred, &cred_args);
	//info("hostlist=%s", cred_args.step_hostlist);
	ret_list = slurm_send_recv_msgs(cred_args.step_hostlist, &msg, timeout,
					false);
	slurm_cred_free_args(&cred_args);
#else
	ret_list = slurm_send_recv_msgs(nodelist,
					&msg, 0, false);
#endif
	if (ret_list == NULL) {
		error("slurm_send_recv_msgs failed miserably: %m");
		return SLURM_ERROR;
	}
	ret_itr = list_iterator_create(ret_list);
	while ((ret_data = list_next(ret_itr)) != NULL)
	{
		rc = slurm_get_return_code(ret_data->type,
					   ret_data->data);
		debug3("launch returned msg_rc=%d err=%d type=%d",
		      rc, ret_data->err, ret_data->type);
		if (rc != SLURM_SUCCESS) {
			if (ret_data->err)
				tot_rc = ret_data->err;
			else
				tot_rc = rc;


			errno = tot_rc;
			tot_rc = SLURM_ERROR;
		} else {
#if 0 /* only for debugging, might want to make this a callback */
			errno = ret_data->err;
			info("Launch success on node %s",
			     ret_data->node_name);
#endif
		}
	}

	/* Below updating the ZHT */
	hostlist_t hl = hostlist_create(nodelist);
	//KVStr *kv_str_value, *kv_str_new_value;
	int tmp_count, i;
	char** tmp_name;
again:
	/*kv_str_value = _lookup_zht_unpacking(kv_str_key);
	if (kv_str_value == NULL)
	{
		printf("Hello, what's going on here?\n");
	}
	tmp_count = kv_str_value->num_item + hostlist_count(hl);
	tmp_name = (char**)malloc(sizeof(char*) * tmp_count);
	for (i = 0; i < kv_str_value->num_item; i++)
	{
		tmp_name[i] = kv_str_value->name[i];
	}
	for (i = kv_str_value->num_item; i < tmp_count; i++)
	{
		tmp_name[i] = hostlist_shift(hl);
	}
	kv_str_new_value = _init_kv_str(kv_str_value->has_job_id, kv_str_value->job_id,
				kv_str_value->has_num_item, tmp_count, tmp_count);
	if (!_compare_and_swap(kv_str_key, kv_str_value, kv_str_new_value))
	{
		free(kv_str_value);
		free(kv_str_new_value);
		free(tmp_name);
		goto again;
	}*/
	list_iterator_destroy(ret_itr);
	list_destroy(ret_list);

	if (tot_rc != SLURM_SUCCESS)
		return tot_rc;
	return rc;
}

int
_compute_task_count_1(allocation_info_t *ainfo, struct srun_options *opt_1)
{
	int i, cnt = 0;
#if defined HAVE_BGQ
//#if defined HAVE_BGQ && HAVE_BG_FILES
	/* always return the ntasks here for Q */
	return opt_1->ntasks;
#endif
	if (opt_1->cpus_set) {
		for (i = 0; i < ainfo->num_cpu_groups; i++)
			cnt += ( ainfo->cpu_count_reps[i] *
				 (ainfo->cpus_per_node[i]/opt_1->cpus_per_task));
	} else if (opt_1->ntasks_per_node != NO_VAL)
		cnt = ainfo->nnodes * opt_1->ntasks_per_node;

	return (cnt < ainfo->nnodes) ? ainfo->nnodes : cnt;
}

void
_set_ntasks_1(allocation_info_t *ai, struct srun_options *opt_1)
{
	if (!opt_1->ntasks_set) {
		opt_1->ntasks = _compute_task_count_1(ai, opt_1);
		if (opt_1->cpus_set)
			opt_1->ntasks_set = true;	/* implicit */
	}
}

fname_t *fname_create_1(srun_job_t *job, char *format, int ntasks)
{
	unsigned int wid     = 0;
	unsigned long int taskid  = 0;
	fname_t *fname = NULL;
	char *p, *q, *name;

	fname = xmalloc(sizeof(*fname));
	fname->type = IO_ALL;
	fname->name = NULL;
	fname->taskid = -1;

	/* Handle special  cases
	 */

	if ((format == NULL)
	    || (strncasecmp(format, "all", (size_t) 3) == 0)
	    || (strncmp(format, "-", (size_t) 1) == 0)       ) {
		 /* "all" explicitly sets IO_ALL and is the default */
		return fname;
	}

	if (strcasecmp(format, "none") == 0) {
		/*
		 * Set type to IO_PER_TASK so that /dev/null is opened
		 *  on every node, which should be more efficient
		 */
		fname->type = IO_PER_TASK;
		fname->name = xstrdup ("/dev/null");
		return fname;
	}

	taskid = strtoul(format, &p, 10);
	if ((*p == '\0') && ((int) taskid < ntasks)) {
		fname->type   = IO_ONE;
		fname->taskid = (uint32_t) taskid;
		/* Set the name string to pass to slurmd
		 *  to the taskid requested, so that tasks with
		 *  no IO can open /dev/null.
		 */
		fname->name   = xstrdup (format);
		return fname;
	}

	name = NULL;
	q = p = format;
	while (*p != '\0') {
		if (*p == '%') {
			if (isdigit(*(++p))) {
				unsigned long in_width = 0;
				xmemcat(name, q, p - 1);
				if ((in_width = strtoul(p, &p, 10)) > MAX_WIDTH)
					wid = MAX_WIDTH;
				else
					wid = in_width;
				q = p - 1;
				if (*p == '\0')
					break;
			}

			switch (*p) {
			 case 't':  /* '%t' => taskid         */
			 case 'n':  /* '%n' => nodeid         */
			 case 'N':  /* '%N' => node name      */

				 fname->type = IO_PER_TASK;
				 if (wid)
					 xstrcatchar(name, '%');
				 p++;
				 break;

			 case 'J':  /* '%J' => "jobid.stepid" */
			 case 'j':  /* '%j' => jobid          */

				 xmemcat(name, q, p - 1);
				 xstrfmtcat(name, "%0*d", wid, job->jobid);

				 if ((*p == 'J') && (job->stepid != NO_VAL))
					 xstrfmtcat(name, ".%d", job->stepid);
				 q = ++p;
				 break;

			 case 's':  /* '%s' => stepid         */
				 xmemcat(name, q, p - 1);
				 xstrfmtcat(name, "%0*d", wid, job->stepid);
				 q = ++p;
				 break;

			 default:
				 break;
			}
			wid = 0;
		} else
			p++;
	}

	if (q != p)
		xmemcat(name, q, p);

	fname->name = name;
	return fname;
}

void
job_update_io_fnames_1(srun_job_t *job, struct srun_options *opt_1)
{
	job->ifname = fname_create_1(job, opt_1->ifname, opt_1->ntasks);
	job->ofname = fname_create_1(job, opt_1->ofname, opt_1->ntasks);
	job->efname = opt_1->efname ? fname_create_1(job, opt_1->efname, opt_1->ntasks) : job->ofname;
}

srun_job_t *
_job_create_structure_1(allocation_info_t *ainfo, struct srun_options *opt_1)
{
	srun_job_t *job = xmalloc(sizeof(srun_job_t));
	int i;

	_set_ntasks_1(ainfo, opt_1);
	debug2("creating job with %d tasks", opt_1->ntasks);

	slurm_mutex_init(&job->state_mutex);
	pthread_cond_init(&job->state_cond, NULL);
	job->state = SRUN_JOB_INIT;

 	job->alias_list = xstrdup(ainfo->alias_list);
 	job->nodelist = xstrdup(ainfo->nodelist);
	job->stepid  = ainfo->stepid;

#if defined HAVE_BG && !defined HAVE_BG_L_P
//#if defined HAVE_BGQ && defined HAVE_BG_FILES
	/* Since the allocation will have the correct cnode count get
	   it if it is available.  Else grab it from opt.min_nodes
	   (meaning the allocation happened before).
	*/
	if (ainfo->select_jobinfo)
		select_g_select_jobinfo_get(ainfo->select_jobinfo,
					    SELECT_JOBDATA_NODE_CNT,
					    &job->nhosts);
	else
		job->nhosts   = opt_1->min_nodes;
	/* If we didn't ask for nodes set it up correctly here so the
	   step allocation does the correct thing.
	*/
	if (!opt_1->nodes_set) {
		opt_1->min_nodes = opt_1->max_nodes = job->nhosts;
		opt_1->nodes_set = true;
		opt_1->ntasks_per_node = NO_VAL;
		//bg_figure_nodes_tasks();

#if defined HAVE_BG_FILES
		/* Replace the runjob line with correct information. */
		int i, matches = 0;
		for (i = 0; i < opt_1->argc; i++) {
			if (!strcmp(opt_1->argv[i], "-p")) {
				i++;
				xfree(opt_1->argv[i]);
				opt_1->argv[i]  = xstrdup_printf(
					"%d", opt_1->ntasks_per_node);
				matches++;
			} else if (!strcmp(opt_1->argv[i], "--np")) {
				i++;
				xfree(opt_1->argv[i]);
				opt_1->argv[i]  = xstrdup_printf(
					"%d", opt_1->ntasks);
				matches++;
			}
			if (matches == 2)
				break;
		}
		xassert(matches == 2);
#endif
	}

#elif defined HAVE_FRONT_END	/* Limited job step support */
	opt_1->overcommit = true;
	job->nhosts = 1;
#else
	job->nhosts   = ainfo->nnodes;
#endif

#if !defined HAVE_FRONT_END || (defined HAVE_BGQ)
//#if !defined HAVE_FRONT_END || (defined HAVE_BGQ && defined HAVE_BG_FILES)
	if (opt_1->min_nodes > job->nhosts) {
		error("Only allocated %d nodes asked for %d",
		      job->nhosts, opt_1->min_nodes);
		if (opt_1->exc_nodes) {
			/* When resources are pre-allocated and some nodes
			 * are explicitly excluded, this error can occur. */
			error("Are required nodes explicitly excluded?");
		}
		xfree(job);
		return NULL;
	}
	if ((ainfo->cpus_per_node == NULL) ||
	    (ainfo->cpu_count_reps == NULL)) {
		error("cpus_per_node array is not set");
		xfree(job);
		return NULL;
	}
#endif
	job->select_jobinfo = ainfo->select_jobinfo;
	job->jobid   = ainfo->jobid;

	job->ntasks  = opt_1->ntasks;
	for (i=0; i<ainfo->num_cpu_groups; i++) {
		job->cpu_count += ainfo->cpus_per_node[i] *
			ainfo->cpu_count_reps[i];
	}

	job->rc       = -1;

	job_update_io_fnames_1(job, opt_1);

	return (job);
}

srun_job_t *_job_create_1(struct srun_options *opt_1)
{
	srun_job_t *job = NULL;

	/* Create job allocation message */
	allocation_info_t *ai = xmalloc(sizeof(allocation_info_t));
	uint16_t cpn = 1;
	hostlist_t hl = hostlist_create(opt_1->nodelist);

	if (!hl)
	{
		error("Invalid node list '%s' specified", opt_1->nodelist);
		goto error;
	}
	srand48(pthread_self());
	ai->jobid = MIN_NOALLOC_JOBID + ((uint32_t)lrand48() %
					(MAX_NOALLOC_JOBID - MIN_NOALLOC_JOBID + 1));
	ai->stepid = (uint32_t)(lrand48());
	ai->nodelist = opt_1->nodelist;
	ai->nnodes = hostlist_count(hl);

	hostlist_destroy(hl);
	cpn = (opt_1->ntasks + ai->nnodes - 1) / ai->nnodes;
	ai->cpus_per_node = &cpn;
	ai->cpu_count_reps = &ai->nnodes;

	/* Create job, then fill in host addresses */
	job = _job_create_structure_1(ai, opt_1);

	if (job != NULL)
	{
		job_update_io_fnames_1(job, opt_1);
	}

error:
	xfree(ai);
	return job;
}

/*void _signal_while_allocating(int signo)
{
	uint32_t pending_job_id = 0;
	debug("Got signal %d", signo);
	if (signo == SIGCONT)
		return;

	*destroy_job = 1;
	if (pending_job_id != 0) {
		slurm_complete_job(pending_job_id, NO_VAL);
		info("Job allocation %u has been revoked.", pending_job_id);

	}
}*/

int create_job_step_1(srun_job_t *job, bool use_all_cpus, struct srun_options *opt_1)
{
	sig_atomic_t *destroy_job = NULL;
	int i, rc;
	unsigned long my_sleep = 0;
	time_t begin_time;
	destroy_job = (sig_atomic_t*)malloc(sizeof(sig_atomic_t));
	*destroy_job = 0;
	if (!job) {
		error("launch_common_create_job_step: no job given");
		return SLURM_ERROR;
	}
	slurm_step_ctx_params_t_init(&job->ctx_params);
	job->ctx_params.job_id = job->jobid;
	job->ctx_params.uid = opt_1->uid;

	/* Validate minimum and maximum node counts */
	if (opt_1->min_nodes && opt_1->max_nodes &&
	    (opt_1->min_nodes > opt_1->max_nodes)) {
		error ("Minimum node count > maximum node count (%d > %d)",
				opt_1->min_nodes, opt_1->max_nodes);
		return SLURM_ERROR;
	}
#if !defined HAVE_FRONT_END || (defined HAVE_BGQ)
//#if !defined HAVE_FRONT_END || (defined HAVE_BGQ && defined HAVE_BG_FILES)
	if (opt_1->min_nodes && (opt_1->min_nodes > job->nhosts)) {
		error ("Minimum node count > allocated node count (%d > %d)",
				opt_1->min_nodes, job->nhosts);
		return SLURM_ERROR;
	}
#endif

	job->ctx_params.min_nodes = job->nhosts;
	if (opt_1->min_nodes && (opt_1->min_nodes < job->ctx_params.min_nodes))
		job->ctx_params.min_nodes = opt_1->min_nodes;
	job->ctx_params.max_nodes = job->nhosts;
	if (opt_1->max_nodes && (opt_1->max_nodes < job->ctx_params.max_nodes))
		job->ctx_params.max_nodes = opt_1->max_nodes;

	if (!opt_1->ntasks_set && (opt_1->ntasks_per_node != NO_VAL))
		job->ntasks = opt_1->ntasks = job->nhosts * opt_1->ntasks_per_node;
	job->ctx_params.task_count = opt_1->ntasks;

	if (opt_1->mem_per_cpu != NO_VAL)
		job->ctx_params.mem_per_cpu = opt_1->mem_per_cpu;
	if (opt_1->gres)
		job->ctx_params.gres = opt_1->gres;
	else
		job->ctx_params.gres = getenv("SLURM_STEP_GRES");

	if (use_all_cpus)
		job->ctx_params.cpu_count = job->cpu_count;
	else if (opt_1->overcommit)
		job->ctx_params.cpu_count = job->ctx_params.min_nodes;
	else if (opt_1->cpus_set)
		job->ctx_params.cpu_count = opt_1->ntasks * opt_1->cpus_per_task;
	else
		job->ctx_params.cpu_count = opt_1->ntasks;

	job->ctx_params.cpu_freq = opt_1->cpu_freq;
	job->ctx_params.relative = (uint16_t)opt_1->relative;
	job->ctx_params.ckpt_interval = (uint16_t)opt_1->ckpt_interval;
	job->ctx_params.ckpt_dir = opt_1->ckpt_dir;
	job->ctx_params.exclusive = (uint16_t)opt_1->exclusive;
	if (opt_1->immediate == 1)
		job->ctx_params.immediate = (uint16_t)opt_1->immediate;
	if (opt_1->time_limit != NO_VAL)
		job->ctx_params.time_limit = (uint32_t)opt_1->time_limit;
	job->ctx_params.verbose_level = (uint16_t)_verbose;
	if (opt_1->resv_port_cnt != NO_VAL)
		job->ctx_params.resv_port_cnt = (uint16_t) opt_1->resv_port_cnt;

	switch (opt_1->distribution) {
	case SLURM_DIST_BLOCK:
	case SLURM_DIST_ARBITRARY:
	case SLURM_DIST_CYCLIC:
	case SLURM_DIST_CYCLIC_CYCLIC:
	case SLURM_DIST_CYCLIC_BLOCK:
	case SLURM_DIST_BLOCK_CYCLIC:
	case SLURM_DIST_BLOCK_BLOCK:
		job->ctx_params.task_dist = opt_1->distribution;
		break;
	case SLURM_DIST_PLANE:
		job->ctx_params.task_dist = SLURM_DIST_PLANE;
		job->ctx_params.plane_size = opt_1->plane_size;
		break;
	default:
		job->ctx_params.task_dist = (job->ctx_params.task_count <=
					     job->ctx_params.min_nodes)
			? SLURM_DIST_CYCLIC : SLURM_DIST_BLOCK;
		opt_1->distribution = job->ctx_params.task_dist;
		break;

	}
	job->ctx_params.overcommit = opt_1->overcommit ? 1 : 0;

	job->ctx_params.node_list = opt_1->nodelist;

	job->ctx_params.network = opt_1->network;
	job->ctx_params.no_kill = opt_1->no_kill;
	if (opt_1->job_name_set_cmd && opt_1->job_name)
		job->ctx_params.name = opt_1->job_name;
	else
		job->ctx_params.name = opt_1->cmd_name;
	job->ctx_params.features = opt_1->constraints;

	debug("requesting job %u, user %u, nodes %u including (%s)",
	      job->ctx_params.job_id, job->ctx_params.uid,
	      job->ctx_params.min_nodes, job->ctx_params.node_list);
	debug("cpus %u, tasks %u, name %s, relative %u",
	      job->ctx_params.cpu_count, job->ctx_params.task_count,
	      job->ctx_params.name, job->ctx_params.relative);
	job->step_ctx = slurm_step_ctx_create_no_alloc(&job->ctx_params, job->stepid);
	slurm_step_ctx_get(job->step_ctx, SLURM_STEP_CTX_STEPID, &job->stepid);
	/*  Number of hosts in job may not have been initialized yet if
	 *    --jobid was used or only SLURM_JOB_ID was set in user env.
	 *    Reset the value here just in case.
	 */
	slurm_step_ctx_get(job->step_ctx, SLURM_STEP_CTX_NUM_HOSTS,
			   &job->nhosts);
	/*
	 * Recreate filenames which may depend upon step id
	 */
	job_update_io_fnames_1(job, opt_1);
	return SLURM_SUCCESS;
}

_task_user_managed_io_handler(struct step_launch_state *sls,
			      slurm_msg_t *user_io_msg)
{
	task_user_managed_io_msg_t *msg =
		(task_user_managed_io_msg_t *) user_io_msg->data;

	pthread_mutex_lock(&sls->lock);

	debug("task %d user managed io stream established", msg->task_id);
	/* sanity check */
	if (msg->task_id >= sls->tasks_requested) {
		error("_task_user_managed_io_handler:"
		      " bad task ID %u (of %d tasks)",
		      msg->task_id, sls->tasks_requested);
	}

	sls->io.user->connected++;
	fd_set_blocking(user_io_msg->conn_fd);
	sls->io.user->sockets[msg->task_id] = user_io_msg->conn_fd;

	/* prevent the caller from closing the user managed IO stream */
	user_io_msg->conn_fd = -1;

	pthread_cond_broadcast(&sls->cond);
	pthread_mutex_unlock(&sls->lock);
}

void _step_step_signal(struct step_launch_state *sls, slurm_msg_t *signal_msg)
{
	job_step_kill_msg_t *step_signal = signal_msg->data;
	debug2("Signal %u requested for step %u.%u", step_signal->signal,
	       step_signal->job_id, step_signal->job_step_id);
	if (sls->callback.step_signal)
		(sls->callback.step_signal)(step_signal->signal);

}

void
_exit_handler(struct step_launch_state *sls, slurm_msg_t *exit_msg)
{
	task_exit_msg_t *msg = (task_exit_msg_t *) exit_msg->data;
	int i;

	int task_exit_signal = 0;
	if ((msg->job_id != sls->mpi_info->jobid) ||
	    (msg->step_id != sls->mpi_info->stepid)) {
		debug("Received MESSAGE_TASK_EXIT from wrong job: %u.%u",
		      msg->job_id, msg->step_id);
		return;
	}

	/* Record SIGTERM and SIGKILL termination codes to
	 * recognize abnormal termination */
	if (WIFSIGNALED(msg->return_code)) {
		i = WTERMSIG(msg->return_code);
		if ((i == SIGKILL) || (i == SIGTERM))
			task_exit_signal = i;
	}

	pthread_mutex_lock(&sls->lock);

	for (i = 0; i < msg->num_tasks; i++) {
		debug("task %u done", msg->task_id_list[i]);
		bit_set(sls->tasks_exited, msg->task_id_list[i]);
	}

	if (sls->callback.task_finish != NULL)
		(sls->callback.task_finish)(msg);

	pthread_cond_broadcast(&sls->cond);
	pthread_mutex_unlock(&sls->lock);
}

void
_node_fail_handler(struct step_launch_state *sls, slurm_msg_t *fail_msg)
{
	srun_node_fail_msg_t *nf = fail_msg->data;
	hostset_t fail_nodes, all_nodes;
	hostlist_iterator_t fail_itr;
	int num_node_ids;
	int *node_ids;
	int i, j;
	int node_id, num_tasks;

	error("Node failure on %s", nf->nodelist);

	fail_nodes = hostset_create(nf->nodelist);
	fail_itr = hostset_iterator_create(fail_nodes);
	num_node_ids = hostset_count(fail_nodes);
	node_ids = xmalloc(sizeof(int) * num_node_ids);

	pthread_mutex_lock(&sls->lock);
	all_nodes = hostset_create(sls->layout->node_list);
	/* find the index number of each down node */
	for (i = 0; i < num_node_ids; i++) {
#ifdef HAVE_FRONT_END
		node_id = 0;
#else
		char *node = hostlist_next(fail_itr);
		node_id = node_ids[i] = hostset_find(all_nodes, node);
		if (node_id < 0) {
			error(  "Internal error: bad SRUN_NODE_FAIL message. "
				"Node %s not part of this job step", node);
			free(node);
			continue;
		}
		free(node);
#endif

		/* find all of the tasks that should run on this node and
		 * mark them as having started and exited.  If they haven't
		 * started yet, they never will, and likewise for exiting.
		 */
		num_tasks = sls->layout->tasks[node_id];
		for (j = 0; j < num_tasks; j++) {
			debug2("marking task %d done on failed node %d",
			       sls->layout->tids[node_id][j], node_id);
			bit_set(sls->tasks_started,
				sls->layout->tids[node_id][j]);
			bit_set(sls->tasks_exited,
				sls->layout->tids[node_id][j]);
		}
	}

	if (!sls->user_managed_io) {
		client_io_handler_downnodes(sls->io.normal, node_ids,
					    num_node_ids);
	}
	pthread_cond_broadcast(&sls->cond);
	pthread_mutex_unlock(&sls->lock);

	xfree(node_ids);
	hostlist_iterator_destroy(fail_itr);
	hostset_destroy(fail_nodes);
	hostset_destroy(all_nodes);
}
static void *
_check_io_timeout(void *_sls)
{
	int ii;
	time_t now, next_deadline;
	struct timespec ts = {0, 0};
	step_launch_state_t *sls = (step_launch_state_t *)_sls;

	pthread_mutex_lock(&sls->lock);

	while (1) {
		if (sls->halt_io_test || sls->abort)
			break;

		now = time(NULL);
		next_deadline = (time_t)NO_VAL;

		for (ii = 0; ii < sls->layout->node_cnt; ii++) {
			if (sls->io_deadline[ii] == (time_t)NO_VAL)
				continue;

			if (sls->io_deadline[ii] <= now) {
				sls->abort = true;
				pthread_cond_broadcast(&sls->cond);
				error(  "Cannot communicate with node %d.  "
					"Aborting job.", ii);
				break;
			} else if (next_deadline == (time_t)NO_VAL ||
				   sls->io_deadline[ii] < next_deadline) {
				next_deadline = sls->io_deadline[ii];
			}
		}
		if (sls->abort)
			break;

		if (next_deadline == (time_t)NO_VAL) {
			debug("io timeout thread: no pending deadlines, "
			      "sleeping indefinitely");
			pthread_cond_wait(&sls->cond, &sls->lock);
		} else {
			debug("io timeout thread: sleeping %lds until deadline",
			       (long)(next_deadline - time(NULL)));
			ts.tv_sec = next_deadline;
			pthread_cond_timedwait(&sls->cond, &sls->lock, &ts);
		}
	}
	pthread_mutex_unlock(&sls->lock);
	return NULL;
}

int
_start_io_timeout_thread(step_launch_state_t *sls)
{
	int rc = SLURM_SUCCESS;
	pthread_attr_t attr;
	slurm_attr_init(&attr);

	if (pthread_create(&sls->io_timeout_thread, &attr,
			   _check_io_timeout, (void *)sls) != 0) {
		error("pthread_create of io timeout thread: %m");
		sls->io_timeout_thread = 0;
		rc = SLURM_ERROR;
	} else {
		sls->io_timeout_thread_created = true;
	}
	slurm_attr_destroy(&attr);
	return rc;
}

void
_step_missing_handler(struct step_launch_state *sls, slurm_msg_t *missing_msg)
{
	srun_step_missing_msg_t *step_missing = missing_msg->data;
	hostset_t fail_nodes, all_nodes;
	hostlist_iterator_t fail_itr;
	char *node;
	int num_node_ids;
	int i, j;
	int node_id;
	client_io_t *cio = sls->io.normal;
	bool  test_message_sent;
	int   num_tasks;
	bool  active;

	debug("Step %u.%u missing from node(s) %s",
	      step_missing->job_id, step_missing->step_id,
	      step_missing->nodelist);

	/* Ignore this message in the unusual "user_managed_io" case.  No way
	   to confirm a bad connection, since a test message goes straight to
	   the task.  Aborting without checking may be too dangerous.  This
	   choice may cause srun to not exit even though the job step has
	   ended. */
	if (sls->user_managed_io)
		return;

	pthread_mutex_lock(&sls->lock);

	if (!sls->io_timeout_thread_created) {
		if (_start_io_timeout_thread(sls)) {
			/*
			 * Should I abort here, because of the inability to
			 * make a thread to verify the connection?
			 */
			error("Cannot create thread to verify I/O "
			      "connections.");

			sls->abort = true;
			pthread_cond_broadcast(&sls->cond);
			pthread_mutex_unlock(&sls->lock);
			return;
		}
	}

	fail_nodes = hostset_create(step_missing->nodelist);
	fail_itr = hostset_iterator_create(fail_nodes);
	num_node_ids = hostset_count(fail_nodes);

	all_nodes = hostset_create(sls->layout->node_list);

	for (i = 0; i < num_node_ids; i++) {
		node = hostlist_next(fail_itr);
		node_id = hostset_find(all_nodes, node);
		if (node_id < 0) {
			error("Internal error: bad SRUN_STEP_MISSING message. "
			      "Node %s not part of this job step", node);
			free(node);
			continue;
		}
		free(node);

		/*
		 * If all tasks for this node have either not started or already
		 * exited, ignore the missing step message for this node.
		 */
		num_tasks = sls->layout->tasks[node_id];
		active = false;
		for (j = 0; j < num_tasks; j++) {
			if (bit_test(sls->tasks_started,
				     sls->layout->tids[node_id][j]) &&
			    !bit_test(sls->tasks_exited,
				      sls->layout->tids[node_id][j])) {
				active = true;
				break;
			}
		}
		if (!active)
			continue;

		/* If this is true, an I/O error has already occurred on the
		 * stepd for the current node, and the job should abort */
		if (bit_test(sls->node_io_error, node_id)) {
			error("Aborting, step missing and io error on node %d",
			      node_id);
			sls->abort = true;
			pthread_cond_broadcast(&sls->cond);
			break;
		}

		/*
		 * A test is already is progress. Ignore message for this node.
		 */
		if (sls->io_deadline[node_id] != NO_VAL) {
			debug("Test in progress for node %d, ignoring message",
			      node_id);
			continue;
		}

		sls->io_deadline[node_id] = time(NULL) + sls->io_timeout;

		debug("Testing connection to node %d", node_id);
		if (client_io_handler_send_test_message(cio, node_id,
							&test_message_sent)) {
			/*
			 * If unable to test a connection, assume the step
			 * is having problems and abort.  If unable to test,
			 * the system is probably having serious problems, so
			 * aborting the step seems reasonable.
			 */
			error("Aborting, can not test connection to node %d.",
			      node_id);
			sls->abort = true;
			pthread_cond_broadcast(&sls->cond);
			break;
		}

		/*
		 * test_message_sent should be true unless this node either
		 * hasn't started or already finished.  Poke the io_timeout
		 * thread to make sure it will abort the job if the deadline
		 * for receiving a response passes.
		 */
		if (test_message_sent) {
			pthread_cond_broadcast(&sls->cond);
		} else {
			sls->io_deadline[node_id] = (time_t)NO_VAL;
		}
	}
	pthread_mutex_unlock(&sls->lock);

	hostlist_iterator_destroy(fail_itr);
	hostset_destroy(fail_nodes);
	hostset_destroy(all_nodes);
}

void
_timeout_handler(struct step_launch_state *sls, slurm_msg_t *timeout_msg)
{
	srun_timeout_msg_t *step_msg =
		(srun_timeout_msg_t *) timeout_msg->data;

	if (sls->callback.step_timeout)
		(sls->callback.step_timeout)(step_msg);

	pthread_mutex_lock(&sls->lock);
	pthread_cond_broadcast(&sls->cond);
	pthread_mutex_unlock(&sls->lock);
}

void
_launch_handler(struct step_launch_state *sls, slurm_msg_t *resp)
{
	launch_tasks_response_msg_t *msg = resp->data;
	int i;

	pthread_mutex_lock(&sls->lock);
	if ((msg->count_of_pids > 0) &&
	    bit_test(sls->tasks_started, msg->task_ids[0])) {
		debug3("duplicate launch response received from node %s. "
		       "this is not an error", msg->node_name);
		pthread_mutex_unlock(&sls->lock);
		return;
	}

	if (msg->return_code) {
		for (i = 0; i < msg->count_of_pids; i++) {
			error("task %u launch failed: %s",
			      msg->task_ids[i],
			      slurm_strerror(msg->return_code));
			bit_set(sls->tasks_started, msg->task_ids[i]);
			bit_set(sls->tasks_exited, msg->task_ids[i]);
		}
	} else {
		for (i = 0; i < msg->count_of_pids; i++)
			bit_set(sls->tasks_started, msg->task_ids[i]);
	}
	if (sls->callback.task_start != NULL)
		(sls->callback.task_start)(msg);

	pthread_cond_broadcast(&sls->cond);
	pthread_mutex_unlock(&sls->lock);

}

void
_exec_prog(slurm_msg_t *msg)
{
	pid_t  srun_ppid = (pid_t) 0;
	pid_t child;
	int pfd[2], status, exit_code = 0, i;
	ssize_t len;
	char *argv[4], buf[256] = "";
	time_t now = time(NULL);
	bool checkpoint = false;
	srun_exec_msg_t *exec_msg = msg->data;

	if (exec_msg->argc > 2) {
		verbose("Exec '%s %s' for %u.%u",
			exec_msg->argv[0], exec_msg->argv[1],
			exec_msg->job_id, exec_msg->step_id);
	} else {
		verbose("Exec '%s' for %u.%u",
			exec_msg->argv[0],
			exec_msg->job_id, exec_msg->step_id);
	}

	if (strcmp(exec_msg->argv[0], "ompi-checkpoint") == 0) {
		if (srun_ppid)
			checkpoint = true;
		else {
			error("Can not create checkpoint, no srun_ppid set");
			exit_code = EINVAL;
			goto fini;
		}
	}
	if (checkpoint) {
		/* OpenMPI specific checkpoint support */
		info("Checkpoint started at %s", ctime(&now));
		for (i=0; (exec_msg->argv[i] && (i<2)); i++) {
			argv[i] = exec_msg->argv[i];
		}
		snprintf(buf, sizeof(buf), "%ld", (long) srun_ppid);
		argv[i] = buf;
		argv[i+1] = NULL;
	}

	if (pipe(pfd) == -1) {
		snprintf(buf, sizeof(buf), "pipe: %s", strerror(errno));
		error("%s", buf);
		exit_code = errno;
		goto fini;
	}

	child = fork();
	if (child == 0) {
		int fd = open("/dev/null", O_RDONLY);
		dup2(fd, 0);		/* stdin from /dev/null */
		dup2(pfd[1], 1);	/* stdout to pipe */
		dup2(pfd[1], 2);	/* stderr to pipe */
		close(pfd[0]);
		close(pfd[1]);
		if (checkpoint)
			execvp(exec_msg->argv[0], argv);
		else
			execvp(exec_msg->argv[0], exec_msg->argv);
		error("execvp(%s): %m", exec_msg->argv[0]);
	} else if (child < 0) {
		snprintf(buf, sizeof(buf), "fork: %s", strerror(errno));
		error("%s", buf);
		exit_code = errno;
		goto fini;
	} else {
		close(pfd[1]);
		len = read(pfd[0], buf, sizeof(buf));
		if (len >= 1)
			close(pfd[0]);
		waitpid(child, &status, 0);
		exit_code = WEXITSTATUS(status);
	}

fini:	if (checkpoint) {
		now = time(NULL);
		if (exit_code) {
			info("Checkpoint completion code %d at %s",
				exit_code, ctime(&now));
		} else {
			info("Checkpoint completed successfully at %s",
				ctime(&now));
		}
		if (buf[0])
			info("Checkpoint location: %s", buf);
		slurm_checkpoint_complete(exec_msg->job_id, exec_msg->step_id,
			time(NULL), (uint32_t) exit_code, buf);
	}
}

void
_job_complete_handler(struct step_launch_state *sls, slurm_msg_t *complete_msg)
{
	bool   force_terminated_job = false;
	srun_job_complete_msg_t *step_msg =
		(srun_job_complete_msg_t *) complete_msg->data;

	if (step_msg->step_id == NO_VAL) {
		verbose("Complete job %u received",
			step_msg->job_id);
	} else {
		verbose("Complete job step %u.%u received",
			step_msg->job_id, step_msg->step_id);
	}

	if (sls->callback.step_complete)
		(sls->callback.step_complete)(step_msg);

	force_terminated_job = true;
	pthread_mutex_lock(&sls->lock);
	sls->abort = true;
	pthread_cond_broadcast(&sls->cond);
	pthread_mutex_unlock(&sls->lock);
}

void
_handle_msg(void *arg, slurm_msg_t *msg)
{
	struct step_launch_state *sls = (struct step_launch_state *)arg;
	uid_t req_uid = g_slurm_auth_get_uid(msg->auth_cred, NULL);
	uid_t uid = getuid();
	srun_user_msg_t *um;
	int rc;

	switch (msg->msg_type) {
	case RESPONSE_LAUNCH_TASKS:
		debug2("received task launch");
		_launch_handler(sls, msg);
		slurm_free_launch_tasks_response_msg(msg->data);
		break;
	case MESSAGE_TASK_EXIT:
		debug2("received task exit");
		_exit_handler(sls, msg);
		slurm_free_task_exit_msg(msg->data);
		break;
	case SRUN_PING:
		debug3("slurmctld ping received");
		slurm_send_rc_msg(msg, SLURM_SUCCESS);
		slurm_free_srun_ping_msg(msg->data);
		break;
	case SRUN_EXEC:
		_exec_prog(msg);
		slurm_free_srun_exec_msg(msg->data);
		break;
	case SRUN_JOB_COMPLETE:
		debug2("received job step complete message");
		_job_complete_handler(sls, msg);
		slurm_free_srun_job_complete_msg(msg->data);
		break;
	case SRUN_TIMEOUT:
		debug2("received job step timeout message");
		_timeout_handler(sls, msg);
		slurm_free_srun_timeout_msg(msg->data);
		break;
	case SRUN_USER_MSG:
		um = msg->data;
		info("%s", um->msg);
		slurm_free_srun_user_msg(msg->data);
		break;
	case SRUN_NODE_FAIL:
		debug2("received srun node fail");
		_node_fail_handler(sls, msg);
		slurm_free_srun_node_fail_msg(msg->data);
		break;
	case SRUN_STEP_MISSING:
		debug2("received notice of missing job step");
		_step_missing_handler(sls, msg);
		slurm_free_srun_step_missing_msg(msg->data);
		break;
	case SRUN_STEP_SIGNAL:
		debug2("received step signal RPC");
		_step_step_signal(sls, msg);
		slurm_free_job_step_kill_msg(msg->data);
		break;
	case PMI_KVS_PUT_REQ:
		debug2("PMI_KVS_PUT_REQ received");
		rc = pmi_kvs_put((struct kvs_comm_set *) msg->data);
		slurm_send_rc_msg(msg, rc);
		break;
	case PMI_KVS_GET_REQ:
		debug2("PMI_KVS_GET_REQ received");
		rc = pmi_kvs_get((kvs_get_msg_t *) msg->data);
		slurm_send_rc_msg(msg, rc);
		slurm_free_get_kvs_msg((kvs_get_msg_t *) msg->data);
		break;
	case TASK_USER_MANAGED_IO_STREAM:
		debug2("TASK_USER_MANAGED_IO_STREAM");
		_task_user_managed_io_handler(sls, msg);
		break;
	default:
		error("received spurious message type: %u",
		      msg->msg_type);
		break;
	}
	return;
}

void *_msg_thr_internal(void *arg)
{
	struct step_launch_state *sls = (struct step_launch_state *)arg;
	eio_handle_mainloop(sls->msg_handle);

	return NULL;
}

inline int
_estimate_nports(int nclients, int cli_per_port)
{
	div_t d;
	d = div(nclients, cli_per_port);
	return d.rem > 0 ? d.quot + 1 : d.quot;
}

int _msg_thr_create(struct step_launch_state *sls, int num_nodes)
{
	int sock = -1;
	short port = -1;
	eio_obj_t *obj;
	int i, rc = SLURM_SUCCESS;
	pthread_attr_t attr;

	debug("Entering _msg_thr_create()");

	sls->msg_handle = eio_handle_create();
	sls->num_resp_port = _estimate_nports(num_nodes, 48);
	sls->resp_port = xmalloc(sizeof(uint16_t) * sls->num_resp_port);

	/* multiple jobs (easily induced via no_alloc) and highly
	 * parallel jobs using PMI sometimes result in slow message
	 * responses and timeouts. Raise the default timeout for srun. */
	if(!message_socket_ops.timeout)
		message_socket_ops.timeout = slurm_get_msg_timeout() * 8000;

	for (i = 0; i < sls->num_resp_port; i++) {
		if (net_stream_listen(&sock, &port) < 0) {
			error("unable to initialize step launch listening "
			      "socket: %m");
			return SLURM_ERROR;
		}
		sls->resp_port[i] = port;
		obj = eio_obj_create(sock, &message_socket_ops, (void *)sls);
		eio_new_initial_obj(sls->msg_handle, obj);
	}
	/* finally, add the listening port that we told the slurmctld about
	   eariler in the step context creation phase */
	if (sls->slurmctld_socket_fd > -1) {
		obj = eio_obj_create(sls->slurmctld_socket_fd,
				     &message_socket_ops, (void *)sls);
		eio_new_initial_obj(sls->msg_handle, obj);
	}
	slurm_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&sls->msg_thread, &attr, _msg_thr_internal, (void *)sls) != 0) {
		error("pthread_create of message thread: %m");
		/* make sure msg_thread is 0 so we don't wait on it. */
		sls->msg_thread = 0;
		rc = SLURM_ERROR;
	}
	slurm_attr_destroy(&attr);
	return rc;
}
