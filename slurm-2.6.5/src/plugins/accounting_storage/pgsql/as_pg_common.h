/*****************************************************************************\
 *  as_pg_common.h - accounting interface to pgsql - common functions.
 *
 *  $Id: as_pg_common.h 13061 2008-01-22 21:23:56Z da $
 *****************************************************************************
 *  Copyright (C) 2004-2007 The Regents of the University of California.
 *  Copyright (C) 2008 Lawrence Livermore National Security.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Danny Auble <da@llnl.gov>
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/
#ifndef _HAVE_AS_PGSQL_COMMON_H
#define _HAVE_AS_PGSQL_COMMON_H

#include <strings.h>
#include <stdlib.h>
#include "src/common/slurm_xlator.h"
#include "src/database/pgsql_common.h"
#include "src/slurmdbd/read_config.h"
#include "src/common/slurmdbd_defs.h"
#include "src/common/slurm_jobacct_gather.h"
#include "src/common/uid.h"
#include "src/plugins/accounting_storage/common/common_as.h"

#include "accounting_storage_pgsql.h"
#include "as_pg_acct.h"
#include "as_pg_archive.h"
#include "as_pg_assoc.h"
#include "as_pg_cluster.h"
#include "as_pg_event.h"
#include "as_pg_job.h"
#include "as_pg_problem.h"
#include "as_pg_qos.h"
#include "as_pg_resv.h"
#include "as_pg_rollup.h"
#include "as_pg_txn.h"
#include "as_pg_usage.h"
#include "as_pg_user.h"
#include "as_pg_wckey.h"

#define DELETE_SEC_BACK (3600*24)


/*
 * To save typing and avoid wrapping long lines
 */
#define DEF_VARS char *query = NULL; \
	PGresult *result = NULL;

#define DEF_VARS2 char *query2 = NULL; \
	PGresult *result2 = NULL;

#define DEBUG_QUERY do { \
		debug3("as/pg(%s:%d) query\n%s", __FILE__, __LINE__, query); \
	} while (0)

/* Debug, Execute, Free query, and RETurn result */
#define DEF_QUERY_RET ({			\
	PGresult *_res; \
	DEBUG_QUERY; \
	_res = pgsql_db_query_ret(pg_conn->db_conn, query);	\
	xfree(query); \
	_res; })

/* Debug, Execute, Free query, and RETurn error code */
#define DEF_QUERY_RET_RC ({\
	int _rc; \
	DEBUG_QUERY; \
	_rc = pgsql_db_query(pg_conn->db_conn, query);	\
	xfree(query); \
	_rc; })

/* Debug, Execute, Free query, and RETurn object id */
#define DEF_QUERY_RET_ID ({\
	int _id; \
	DEBUG_QUERY; \
	_id = pgsql_query_ret_id(pg_conn->db_conn, query);	\
	xfree(query); \
	_id; })

/* XXX: special variable name 'result' */
#define PG_VAL(col) PQgetvalue(result, 0, col)
#define PG_NULL(col) PQgetisnull(result, 0, col)
#define PG_EMPTY(col) (PQgetvalue(result, 0, col)[0] == '\0')

#define FOR_EACH_ROW do { \
	int _row, _num; \
	_num = PQntuples(result); \
	for (_row = 0; _row < _num; _row ++)
#define END_EACH_ROW } while (0)
#define ROW(col) PQgetvalue(result, _row, col)
#define ISNULL(col) PQgetisnull(result, _row, col)
#define ISEMPTY(col) (PQgetvalue(result, _row, col)[0] == '\0')

#define FOR_EACH_ROW2 do { \
	int _row2, _num2; \
	_num2 = PQntuples(result2); \
	for (_row2 = 0; _row2 < _num2; _row2 ++)
#define END_EACH_ROW2 } while (0)
#define ROW2(col) PQgetvalue(result2, _row2, col)
#define ISNULL2(col) PQgetisnull(result2, _row2, col)
#define ISEMPTY2(col) (PQgetvalue(result2, _row2, col)[0] == '\0')


/* use identifier cluster_name */
#define FOR_EACH_CLUSTER(_list) do {				\
	List _cluster_list = NULL;				\
	ListIterator _itr = NULL;				\
	char *cluster_name = NULL;				\
	if ((_list) && list_count((_list)))			\
		_cluster_list = (_list);			\
	else {							\
		slurm_mutex_lock(&as_pg_cluster_list_lock);	\
		_cluster_list = as_pg_cluster_list;		\
	}							\
	_itr = list_iterator_create(_cluster_list);		\
	while((cluster_name = list_next(_itr)))			\
		
#define END_EACH_CLUSTER 					\
	list_iterator_destroy(_itr);				\
	if (_cluster_list == as_pg_cluster_list)		\
		slurm_mutex_unlock(&as_pg_cluster_list_lock);	\
	} while (0)


extern slurm_dbd_conf_t *slurmdbd_conf;



/* data structures */
typedef struct {
	hostlist_t hl;
	time_t start;
	time_t end;
	bitstr_t *asked_bitmap;
} local_cluster_t;

typedef struct {
	List cluster_list;
	local_cluster_t *curr_cluster;
} cluster_nodes_t;

extern char *default_qos_str;

/* functions */
extern int create_function_xfree(PGconn *db_conn, char *query);

extern void concat_cond_list(List cond_list, char *prefix,
			     char *col, char **cond);
extern void concat_node_state_cond_list(List cond_list, char *prefix,
			     char *col, char **cond);
extern void concat_like_cond_list(List cond_list, char *prefix,
				  char *col, char **cond);
extern void concat_limit_32(char *col, uint32_t limit, char **rec, char **txn);
extern void concat_limit_64(char *col, uint64_t limit, char **rec, char **txn);

extern int pgsql_modify_common(pgsql_conn_t *pg_conn, uint16_t type, time_t now,
			       char *cluster, char *user_name, char *table,
			       char *name_char, char *vals);

extern int check_db_connection(pgsql_conn_t *pg_conn);
extern int check_table(PGconn *db_conn, char *schema, char *table,
		       storage_field_t *fields, char *constraint);

extern cluster_nodes_t * setup_cluster_nodes(pgsql_conn_t *pg_conn,
					     slurmdb_job_cond_t *job_cond);
extern void destroy_cluster_nodes(cluster_nodes_t *cnodes);
extern int good_nodes_from_inx(cluster_nodes_t *cnodes, char *node_inx,
			       int submit);

extern List get_assoc_ids(pgsql_conn_t *pg_conn, char *cond);
extern int group_concat_assoc_field(pgsql_conn_t *pg_conn, char *field,
				    char *cond, char **val);

extern void reset_pgsql_conn(pgsql_conn_t *pg_conn);

extern int check_user_op(pgsql_conn_t *pg_conn, uid_t uid, uint16_t private,
			 int *is_admin, slurmdb_user_rec_t *user);

extern int cluster_in_db(pgsql_conn_t *pg_conn, char *cluster_name);
extern int validate_cluster_list(List cluster_list);

#endif /* _HAVE_AS_PGSQL_COMMON_H */
