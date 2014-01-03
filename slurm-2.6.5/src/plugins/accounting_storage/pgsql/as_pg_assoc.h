/*****************************************************************************\
 *  as_pg_assoc.h - accounting interface to pgsql - association related 
 *  functions.
 *
 *  $Id: as_pg_assoc.h 13061 2008-01-22 21:23:56Z da $
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
#ifndef _HAVE_AS_PGSQL_ASSOC_H
#define _HAVE_AS_PGSQL_ASSOC_H

#include "as_pg_common.h"

extern char *assoc_table;

extern int check_assoc_tables(PGconn *db_conn, char *cluster);

extern int as_pg_add_associations(pgsql_conn_t *pg_conn, uint32_t uid,
				  List association_list);
extern List as_pg_modify_associations(
	pgsql_conn_t *pg_conn, uint32_t uid,
	slurmdb_association_cond_t *assoc_cond,
	slurmdb_association_rec_t *assoc);
extern List as_pg_remove_associations(
	pgsql_conn_t *pg_conn, uint32_t uid,
	slurmdb_association_cond_t *assoc_cond);
extern List as_pg_get_associations(pgsql_conn_t *pg_conn, uid_t uid,
				   slurmdb_association_cond_t *assoc_cond);

extern int add_cluster_root_assoc(pgsql_conn_t *pg_conn, time_t now,
				  slurmdb_cluster_rec_t *cluster, char **txn_info);

extern char * get_user_from_associd(pgsql_conn_t *pg_conn, char *cluster,
				    uint32_t associd);
extern int pgsql_get_modified_lfts(pgsql_conn_t *pg_conn, char *cluster,
				   uint32_t start_lft);

#endif /* _HAVE_AS_PGSQL_ASSOC_H */
