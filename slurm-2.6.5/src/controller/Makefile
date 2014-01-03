#../../src/srun/libsrun
INCS=-I. \
	-I../../ \
	-I../common/ \
	-I../sreport/ \
	-I../slurmctld/ \
	-I../sbatch \
	-I../controller \
	-I../sbcast \
	-I../database \
	-I../srun/libsrun \
	-I../srun \
	-I../sshare \
	-I../strigger \
	-I../sview \
	-I../slurmdbd \
	-I../sprio \
	-I../scancel \
	-I../squeue \
	-I../sinfo \
	-I../api \
	-I../slurmd/common \
	-I../slurmd/slurmstepd \
	-I../slurmd/slurmd \
	-I../sattach \
	-I../salloc \
	-I../sacct \
	-I../ZHT/src \
	-I../sstat \
	-I../scontrol \
	-I../sacctmgr \
	-I../smap \
	-I../plugins/topology/3d_torus/ \
	-I../plugins/select/bluegene/ \
	-I../plugins/select/cons_res \
	-I../plugins/select/cray \
	-I../plugins/select/cray/libalps \
	-I../plugins/select/cray/libalps \
	-I../plugins/select/linear \
	-I../plugins/select/serial \
	-I../plugins/acct_gather_energy/rapl \
	-I../plugins/accounting_storage/common \
	-I../plugins/accounting_storage/filetxt \
	-I../plugins/accounting_storage/pgsql \
	-I../plugins/accounting_storage/mysql \
	-I../plugins/jobcomp/filetxt \
	-I../plugins/jobcomp/pgsql \
	-I../plugins/jobcomp/mysql \
	-I../plugins/mpi/mvapich \
	-I../plugins/mpi/mpichgm \
	-I../plugins/mpi/pmi2 \
	-I../plugins/mpi/mpichmx \
	-I../plugins/task/affinity \
	-I../plugins/task/cgroup \
	-I../plugins/jobacct_gather/cgroup \
	-I../plugins/launch/slurm \
	-I../plugins/launch/runjob \
	-I../plugins/sched/wiki \
	-I../plugins/sched/builtin \
	-I../plugins/sched/wiki2 \
	-I../plugins/sched/backfill \
	-I../plugins/switch/nrt \
	-I../plugins/proctrack/linuxproc

CFLAGS=-g -pg -O2 -pthread -fno-gcse -Wall -fno-strict-aliasing -lc
LIBFLAGS= -lstdc++ -lrt -lm -lc -lpthread -lprotobuf -lprotobuf-c -lslurm -lpmi -L/home/kwang/workspace/distjoblaunch_4/slurm-2.5.3/src/ZHT/src -lzht 
#CFLAGS=-g -O2 -pthread 
OBJ= controller.o datastr.o kvstr.pb-c.o pro_req.o slurmuse.o 
SRC= controller.c datastr.c kvstr.pb-c.c pro_req.c slurmuse.c
CC=gcc
all: controller

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBFLAGS) $(INCS) 

controller: $(OBJ) ../ZHT/src/c_zhtclient.o ../ZHT/src/c_zhtclientStd.o ../ZHT/src/cpp_zhtclient.o ../ZHT/src/lru_cache.o ../ZHT/src/meta.pb.o ../ZHT/src/meta.pb-c.o ../ZHT/src/novoht.o ../ZHT/src/lock_guard.o ../ZHT/src/Const.o ../common/log.o ../common/slurm_protocol_defs.o ../common/timers.o ../common/net.o ../common/read_config.o ../common/fd.o ../common/hostlist.o ../common/plugstack.o ../common/slurm_auth.o ../common/slurm_jobacct_gather.o ../common/slurm_protocol_api.o ../common/slurm_rlimits_info.o ../common/switch.o ../common/uid.o ../common/xmalloc.o ../common/xsignal.o ../srun/libsrun/launch.o ../srun/libsrun/allocate.o ../srun/srun_pty.o ../srun/libsrun/multi_prog.o ../api/pmi_server.o ../api/step_launch.o ../plugins/launch/slurm/task_state.o ../srun/libsrun/opt.o ../common/slurm_protocol_socket_implementation.o ../common/forward.o ../common/parse_config.o ../common/pack.o ../common/xstring.o ../common/env.o ../srun/srun.o ../srun/libsrun/fname.o ../srun/libsrun/debugger.o ../srun/libsrun/srun_job.o ../common/mpi.o ../common/job_options.o ../common/list.o ../api/step_io.o ../common/cpu_frequency.o ../common/proc_args.o ../common/parse_time.o ../common/xassert.o ../common/cbuf.o ../common/safeopen.o ../common/node_select.o ../common/job_resources.o ../common/slurm_acct_gather_energy.o ../common/slurm_cred.o ../common/node_conf.o ../common/global_defaults.o ../common/slurm_accounting_storage.o ../common/strlcpy.o ../common/slurm_selecttype_info.o ../common/util-net.o ../common/strnatcmp.o ../common/plugin.o ../common/optz.o ../common/arg_desc.o ../common/slurmdbd_defs.o ../common/slurmdb_defs.o ../common/slurm_protocol_util.o ../common/bitstring.o ../common/eio.o ../common/io_hdr.o ../common/write_labelled_message.o ../common/gres.o ../common/slurm_protocol_pack.o ../common/plugrack.o ../common/assoc_mgr.o ../common/slurm_step_layout.o ../common/slurm_priority.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBFLAGS) 
	#$(CC) -o $@ $^ -DILOG $(CFLAGS) $(LIBFLAGS)
