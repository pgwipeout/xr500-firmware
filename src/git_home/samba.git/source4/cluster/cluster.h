/* 
   Unix SMB/CIFS implementation.

   structures for clustering

   Copyright (C) Andrew Tridgell 2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CLUSTER_H__
#define __CLUSTER_H__ 

#include "librpc/gen_ndr/server_id.h"

/*
  test for same cluster id
*/
#define cluster_id_equal(id_1, id_2) ((id_1)->pid == (id_2)->pid \
				    && (id_1)->task_id == (id_2)->task_id \
				    && (id_1)->vnn == (id_2)->vnn)

/*
  test for same cluster node
*/
#define cluster_node_equal(id1, id2) ((id1)->vnn == (id2)->vnn)

struct imessaging_context;
typedef void (*cluster_message_fn_t)(struct imessaging_context *, DATA_BLOB);

/* prototypes */
struct server_id cluster_id(uint64_t id, uint32_t task_id);
struct db_context *cluster_db_tmp_open(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx, const char *dbbase, int flags);
void *cluster_backend_handle(void);

NTSTATUS cluster_message_init(struct imessaging_context *msg, struct server_id server,
			      cluster_message_fn_t handler);
NTSTATUS cluster_message_send(struct server_id server, DATA_BLOB *data);

#endif
