#include <stdio.h>
#include <string.h>

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../cfg/cfg_struct.h"
#include "../../lib/srdb1/db.h"

#include "nsq_pua.h"
#include "nsq.h"

MODULE_VERSION

static int fixup_get_field(void** param, int param_no);
static int fixup_get_field_free(void** param, int param_no);

str lookupd_address = {0,0};
str consumer_topic = {0,0};
str consumer_channel = {0,0};
str nsqd_address = {0,0};
str consumer_event_key = {0,0};
str consumer_event_subkey = {0,0};
/* database connection */
str knsq_db_url = {0,0};
str knsq_presentity_table = str_init("presentity");
db_func_t knsq_pa_dbf;
db1_con_t *knsq_pa_db = NULL;
char* eventData = NULL;

str nsqA = {0, 0};
str nsqE = {0, 0};

static int
init(void)
{
	int ret = daemon_status_send(1);

	if (ret == -1) {
		LM_DBG("Can't send to daemon");
	}

	int total_workers = 1;

	register_procs(total_workers);
	cfg_register_child(total_workers);

	LM_ERR("nsq init() done");

	return 0;
}

static void
message_handler(struct NSQReader *rdr, struct NSQDConnection *conn,
	struct NSQMessage *msg, void *ctx)
{
	char buf[256];
	int route = 0;

	LM_ERR("message_handler called\n");

	sprintf(buf, "nsq-test:phone-registration");
	route = route_get(&event_rt, buf);
	LM_ERR("return from route_get is %d\n", route);
	nsqA.s = calloc(sizeof(char), msg->body_length+1);
	memcpy(nsqA.s, msg->body, msg->body_length);
	

	buffer_reset(conn->command_buf);

	nsq_finish(conn->command_buf, msg->id);
	buffered_socket_write_buffer(conn->bs, conn->command_buf);

	buffer_reset(conn->command_buf);
	nsq_ready(conn->command_buf, rdr->max_in_flight);
	buffered_socket_write_buffer(conn->bs, conn->command_buf);

	free_nsq_message(msg);
}

int
nsq_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	return eventData == NULL ? pv_get_null(msg, param, res) : pv_get_strzval(msg, param, res, eventData);
}

/* module child initialization function */
int
child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0;

	if (rank==PROC_MAIN) {
	
		char buf[256];
		int ret;
		void *ctx = NULL;
		struct ev_loop *loop;
		struct NSQReader *rdr;

		sprintf(buf, "nsq-test:%s", consumer_topic.s);
		LM_ERR("Getting route %s\n", buf);
		ret = route_get(&event_rt, buf);
		LM_ERR("Return from route_get is %d\n", ret);

		loop = ev_default_loop(0);
		rdr = new_nsq_reader(loop, consumer_topic.s, consumer_channel.s,
			(void *)ctx, NULL, NULL, message_handler);
		nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
		nsq_run(loop);
	}


	return 0;
}

static param_export_t params[]=
{
		{"lookupd_address", STR_PARAM, &lookupd_address.s},
		{"consumer_topic", STR_PARAM, &consumer_topic.s},
		{"consumer_channel", STR_PARAM, &consumer_channel.s},
		{"nsqd_address", STR_PARAM, &nsqd_address.s},
		{"consumer_event_key", STR_PARAM, &consumer_event_key.s},
		{"consumer_event_subkey", STR_PARAM, &consumer_event_subkey.s},
		{"db_url", STR_PARAM, &knsq_db_url.s},
		{ 0, 0, 0 }
};

static pv_export_t nsq_mod_pvs[] = {
	{{"nqE", (sizeof("nsqE")-1)}, PVT_OTHER, nsq_pv_get_event_payload, 0, 0, 0, 0, 0},
	{{"nqA", (sizeof("nsqA")-1)}, PVT_OTHER, nsq_pv_get_event_payload, 0, 0, 0, 0, 0},
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
		"nsq-test",
		DEFAULT_DLFLAGS, 	/* dlopen flags */
		0,			 	    /* Exported functions */
		params,		 		/* Exported parameters */
		0,		 			/* exported statistics */
		0,	             	/* exported MI functions */
		nsq_mod_pvs,		/* exported pseudo-variables */
		0,				 	/* extra processes */
		init,        		/* module initialization function */
		0,				 	/* response function*/
		0,	 				/* destroy function */
		child_init       	/* per-child init function */
};
