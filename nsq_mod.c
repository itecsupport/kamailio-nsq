/*
 * NSQ module interface
 *
 * Copyright (C) 2016 Weave Communications
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2016-03  first version (Weave Communications)
 */

#include <sys/types.h>
#include <sys/wait.h>

#include "../../cfg/cfg_struct.h"
#include "../../lib/srdb1/db.h"
#include "nsq_reader.h"
#include "nsq_trans.h"
#include "nsq_pua.h"

MODULE_VERSION

#define DBN_DEFAULT_NO_CONSUMERS 1
#define DBN_DEFAULT_NO_WORKERS 8

#define LOOKUPD_ADDRESS "127.0.0.1"
#define CONSUMER_EVENT_KEY "Event-Category"
#define CONSUMER_EVENT_SUB_KEY "Event-Name"
#define CONSUMER_CHANNEL "Kamailio-Channel"
#define CONSUMER_TOPIC "Kamailio-Topic"
#define NSQD_ADDRESS "127.0.0.1"
#define PRESENTITY_TABLE "presentity"

int nsq_workers = 1;
int nsq_max_in_flight = 1;
int consumer_use_nsqd = 0;
str nsq_lookupd_address = str_init(LOOKUPD_ADDRESS);
int lookupd_port = 4161;
str nsq_event_key = str_init(CONSUMER_EVENT_KEY);
str nsq_event_sub_key = str_init(CONSUMER_EVENT_SUB_KEY);
str nsq_channel = str_init(CONSUMER_CHANNEL);
str nsq_topic = str_init(CONSUMER_TOPIC);
str nsqd_address = str_init(NSQD_ADDRESS);
int nsqd_port = 4150;
int dbn_pua_mode = 1;
int dbn_include_entity = 1;

str nsq_json_escape_str = str_init("%");
char nsq_json_escape_char = '%';

int nsq_server_counter = 0;
int dbn_consumer_processes = DBN_DEFAULT_NO_CONSUMERS;
int dbn_consumer_workers = DBN_DEFAULT_NO_WORKERS;
int startup_time = 0;
int *nsq_worker_pipes_fds = NULL;
int *nsq_worker_pipes = NULL;
int nsq_cmd_pipe = 0;
int nsq_cmd_pipe_fds[2] = {-1,-1};

/* database connection */
db1_con_t *nsq_pa_db = NULL;
db_func_t nsq_pa_dbf;
str nsq_presentity_table = str_init(PRESENTITY_TABLE);
str nsq_db_url = {NULL, 0};

static int mod_init(void);
static int mod_child_init(int);
static void mod_destroy(void);

int nsq_pv_get_event_payload(struct sip_msg*, pv_param_t*, pv_value_t*);

static tr_export_t mod_trans[] = {
	{ {"nsq", sizeof("nsq")-1}, nsq_tr_parse},
	{ { 0, 0 }, 0 }
};

static pv_export_t nsq_mod_pvs[] = {
	{{"nsqE", (sizeof("nsqE")-1)}, PVT_OTHER, nsq_pv_get_event_payload, 0, 0, 0, 0, 0},
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[] = {
	{"nsq_pua_publish", (cmd_function) nsq_pua_publish, 1, 0, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]=
{
	{"consumer_processes", INT_PARAM, &dbn_consumer_processes},
	{"consumer_workers", INT_PARAM, &dbn_consumer_workers},
	{"max_in_flight", INT_PARAM, &nsq_max_in_flight},
	{"lookupd_address", PARAM_STR, &nsq_lookupd_address},
	{"lookupd_port", INT_PARAM, &lookupd_port},
	{"consumer_use_nsqd", INT_PARAM, &consumer_use_nsqd}, // consume messages from nsqd instead of lookupd
	{"consumer_channel", PARAM_STR, &nsq_channel},
	{"consumer_topic", PARAM_STR, &nsq_topic},
	{"nsqd_address", PARAM_STR, &nsqd_address},
	{"nsqd_port", INT_PARAM, &nsqd_port},
	{"consumer_event_key", PARAM_STR, &nsq_event_key},
	{"consumer_event_subkey", PARAM_STR, &nsq_event_sub_key},
	{"pua_include_entity", INT_PARAM, &dbn_include_entity},
	{"presentity_table", PARAM_STR, &nsq_presentity_table},
	{"db_url", PARAM_STR, &nsq_db_url},
	{"pua_mode", INT_PARAM, &dbn_pua_mode},
	{"json_escape_char", PARAM_STR, &nsq_json_escape_str},
	{ 0, 0, 0 }
};

struct module_exports exports = {
	"nsq",
	DEFAULT_DLFLAGS,		/* dlopen flags */
	cmds,					/* Exported functions */
	params,					/* Exported parameters */
	0,						/* exported statistics */
	0,                      /* exported MI functions */
	nsq_mod_pvs,            /* exported pseudo-variables */
	0,						/* extra processes */
	mod_init,				/* module initialization function */
	0,						/* response function*/
	mod_destroy,			/* destroy function */
	mod_child_init			/* per-child init function */
};

static int fire_init_event(int rank)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;

	LM_DBG("rank is (%d)\n", rank);
	if (rank!=PROC_INIT)
		return 0;

	rt = route_get(&event_rt, "nsq:mod-init");
	if (rt>=0 && event_rt.rlist[rt]!=NULL) {
		LM_DBG("executing event_route[nsq:mod-init] (%d)\n", rt);
		if (faked_msg_init()<0)
			return -1;
		fmsg = faked_msg_next();
		rtb = get_route_type();
		set_route_type(REQUEST_ROUTE);
		init_run_actions_ctx(&ctx);
		run_top_route(event_rt.rlist[rt], fmsg, &ctx);
		if (ctx.run_flags&DROP_R_F) {
			LM_ERR("exit due to 'drop' in event route\n");
			return -1;
		}
		set_route_type(rtb);
	}

	return 0;
}

static int mod_init(void)
{
	int i;
	startup_time = (int) time(NULL);

	if (dbn_pua_mode == 1) {
		nsq_db_url.len = nsq_db_url.s ? strlen(nsq_db_url.s) : 0;
		LM_DBG("db_url=%s/%d/%p\n", ZSW(nsq_db_url.s), nsq_db_url.len,nsq_db_url.s);
		nsq_presentity_table.len = strlen(nsq_presentity_table.s);

		if (nsq_db_url.len > 0) {

			/* binding to database module  */
			if (db_bind_mod(&nsq_db_url, &nsq_pa_dbf)) {
				LM_ERR("Database module not found\n");
				return -1;
			}

			if (!DB_CAPABILITY(nsq_pa_dbf, DB_CAP_ALL)) {
				LM_ERR("Database module does not implement all functions"
						" needed by NSQ module\n");
				return -1;
			}

			nsq_pa_db = nsq_pa_dbf.init(&nsq_db_url);
			if (!nsq_pa_db) {
				LM_ERR("Connection to database failed\n");
				return -1;
			}

			nsq_pa_dbf.close(nsq_pa_db);
			nsq_pa_db = NULL;
		}
	}

	int total_workers = dbn_consumer_workers + (dbn_consumer_processes * nsq_server_counter) + 2;

	register_procs(total_workers);
	cfg_register_child(total_workers);

	if (pipe(nsq_cmd_pipe_fds) < 0) {
		LM_ERR("cmd pipe() failed\n");
		return -1;
	}

	nsq_worker_pipes_fds = (int*) shm_malloc(sizeof(int) * (dbn_consumer_workers) * 2 );
	nsq_worker_pipes = (int*) shm_malloc(sizeof(int) * dbn_consumer_workers);
	for(i=0; i < dbn_consumer_workers; i++) {
		nsq_worker_pipes_fds[i*2] = nsq_worker_pipes_fds[i*2+1] = -1;
		if (pipe(&nsq_worker_pipes_fds[i*2]) < 0) {
			LM_ERR("worker pipe(%d) failed\n", i);
			return -1;
		}
	}

	nsq_cmd_pipe = nsq_cmd_pipe_fds[1];
	for(i=0; i < dbn_consumer_workers; i++) {
		nsq_worker_pipes[i] = nsq_worker_pipes_fds[i*2+1];
	}

	return 0;
}

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if (nsq_tr_init_buffers() < 0) {
		LM_ERR("failed to initialize transformations buffers\n");
		return -1;
	}
	return register_trans_mod(path, mod_trans);
}


int set_non_blocking(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -1;

	return 0;
}

/**
 *
 */
int nsq_consumer_worker_proc(int cmd_pipe)
{
	struct ev_loop *loop;
	loop = ev_default_loop(0);
	struct NSQReader *rdr;
	void *ctx = NULL; //(void *)(new TestNsqMsgContext());
	static char channel[128];
	static char topic[128];
	static char address[128];

	if (loop==NULL) {
		LM_ERR("cannot get libev loop\n");
	}
	set_non_blocking(cmd_pipe);

	snprintf(channel, 128, "%.*s", nsq_channel.len, nsq_channel.s);
	snprintf(topic, 128, "%.*s", nsq_topic.len, nsq_topic.s);

	// setup the reader
	rdr = new_nsq_reader(loop, topic, channel, (void *)ctx, NULL, NULL, nsq_message_handler);

	if (consumer_use_nsqd == 0) {
		snprintf(address, 128, "%.*s", nsq_lookupd_address.len, nsq_lookupd_address.s);
		nsq_reader_add_nsqlookupd_endpoint(rdr, address, lookupd_port);
	} else {
		snprintf(address, 128, "%.*s", nsqd_address.len, nsqd_address.s);
		nsq_reader_connect_to_nsqd(rdr, address, nsqd_port);
	}

	nsq_run(loop);
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int mod_child_init(int rank)
{
	int pid;
	int i;

	fire_init_event(rank);

	if (rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0;

	if (rank==PROC_MAIN) {
		for(i=0; i < dbn_consumer_workers; i++) {
			pid=fork_process(i+1, "NSQ Consumer Worker", 1);
			if (pid<0)
				return -1; /* error */
			if (pid==0){
				close(nsq_worker_pipes_fds[i*2+1]);
				return(nsq_consumer_worker_proc(nsq_worker_pipes_fds[i*2]));

			}
		}

		return 0;
	}

	if (dbn_pua_mode == 1) {
		if (nsq_pa_dbf.init == 0) {
			LM_CRIT("child_init: database not bound\n");
			return -1;
		}
		nsq_pa_db = nsq_pa_dbf.init(&nsq_db_url);
		if (!nsq_pa_db) {
			LM_ERR("child %d: unsuccessful connecting to database\n", rank);
			return -1;
		}

		if (nsq_pa_dbf.use_table(nsq_pa_db, &nsq_presentity_table) < 0) {
			LM_ERR( "child %d:unsuccessful use_table presentity_table\n", rank);
			return -1;
		}
		LM_DBG("child %d: Database connection opened successfully\n", rank);
	}

	return 0;
}


/**
 * destroy module function
 */
static void mod_destroy(void) {
	shm_free(nsq_worker_pipes_fds);
	shm_free(nsq_worker_pipes);
}
