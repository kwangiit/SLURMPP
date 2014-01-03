/*
 * slurmuse.h
 *
 *  Created on: Apr 3, 2013
 *      Author: kwang
 */

#ifndef SLURMUSE_H_
#define SLURMUSE_H_

#include "src/common/fd.h"
#include "src/common/hostlist.h"
#include "src/common/log.h"
#include "src/common/net.h"
#include "src/common/plugstack.h"
#include "src/common/read_config.h"
#include "src/common/slurm_auth.h"
#include "src/common/slurm_jobacct_gather.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_rlimits_info.h"
#include "src/common/switch.h"
#include "src/common/uid.h"
#include "src/common/xmalloc.h"
#include "src/common/xsignal.h"

#include "src/srun/libsrun/launch.h"
#include "src/srun/libsrun/allocate.h"
#include "src/srun/srun_pty.h"
#include "src/srun/libsrun/multi_prog.h"
#include "src/api/pmi_server.h"
#include "src/api/step_ctx.h"
#include "src/api/step_launch.h"
#include "src/plugins/launch/slurm/task_state.h"
#include "src/ZHT/src/c_zhtclient.h"
#include "datastr.h"

typedef struct allocation_info
{
	char					*alias_list;
	uint16_t				*cpus_per_node;
	uint32_t				*cpu_count_reps;
	uint32_t				jobid;
	uint32_t				nnodes;
	char					*nodelist;
	uint32_t				num_cpu_groups;
	dynamic_plugin_data_t	*select_jobinfo;
	uint32_t				stepid;
} allocation_info_t;

void _pty_restore(void);

void _enhance_env(env_t *env, srun_job_t *job,
		slurm_step_launch_callbacks_t *step_callbacks, struct srun_options *opt_1);

char* _uint16_array_to_str(int array_len, const uint16_t *array);

slurm_step_launch_params_t
*_get_step_launch_param
(
	srun_job_t *job,
	slurm_step_io_fds_t *cio_fds,
	uint32_t *global_rc,
	slurm_step_launch_callbacks_t *step_callbacks,
	struct srun_options *opt_1
);

slurm_step_launch_params_t
*get_step_launch_param
(
	srun_job_t *job,
	slurm_step_io_fds_t *cio_fds,
	uint32_t *global_rc,
	slurm_step_launch_callbacks_t *step_callbacks,
	struct srun_options *opt_1
);

launch_tasks_request_msg_t
*_get_task_msg
(
	slurm_step_ctx_t *ctx,
	const slurm_step_launch_params_t *params,
	const slurm_step_launch_callbacks_t *callbacks
);

slurm_msg_t *_gen_task_launch_msg(launch_tasks_request_msg_t *launch_msg);

void launch_common_set_stdio_fds_1(srun_job_t *job,
					slurm_step_io_fds_t *cio_fds, struct srun_options *opt_1);

int launch_g_step_launch_1(srun_job_t *job, slurm_step_io_fds_t *cio_fds,
						   uint32_t *global_rc,
						   slurm_step_launch_callbacks_t *step_callbacks,
						   struct srun_options *opt_1);

int initialize_and_process_args_1(int argc, char *argv[], struct srun_options* opt_1);

int launch_tasks(launch_tasks_request_msg_t *launch_msg, char *nodelist);

srun_job_t *_job_create_1(struct srun_options *opt_1);

int create_job_step_1(srun_job_t *job, bool use_all_cpus, struct srun_options *opt_1);

//static int _msg_thr_create(struct step_launch_state *sls, int num_nodes);

#endif /* SLURMUSE_H_ */
