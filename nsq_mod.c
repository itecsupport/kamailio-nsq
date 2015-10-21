#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <json-c/json.h>

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../cfg/cfg_struct.h"
#include "../../lib/srdb1/db.h"
#include "../../daemonize.h"
#include "../../fmsg.h"

#include "nsq_pua.h"
#include "nsq.h"

MODULE_VERSION

str local_hn = {0,0};
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

static void
message_handler(struct NSQReader *rdr, struct NSQDConnection *conn,
	struct NSQMessage *msg, void *ctx)
{
	LM_ERR("resetting buffer\n");
	buffer_reset(conn->command_buf);
	LM_ERR("reset buffer\n");
	LM_ERR("finihing buffer\n");
	nsq_finish(conn->command_buf, msg->id);
	LM_ERR("finished\n");
	LM_ERR("writing buffer %s\n", conn->command_buf->data);
	buffered_socket_write_buffer(conn->bs, conn->command_buf);
	LM_ERR("buffer written\n");
	LM_ERR("resetting buffer #2\n");
	buffer_reset(conn->command_buf);
	LM_ERR("indicating ready max\n");
	nsq_ready(conn->command_buf, rdr->max_in_flight);
	LM_ERR("indicated ready\n");
	LM_ERR("writing buffer %s\n", conn->command_buf->data);
	buffered_socket_write_buffer(conn->bs, conn->command_buf);
	LM_ERR("wrote buffer\n");
	LM_ERR("freeing message\n");
	free_nsq_message(msg);
	LM_ERR("message freed\n");
}

static int
init(void)
{
        int total_workers = 1;
	register_procs(total_workers);
	cfg_register_child(total_workers);

	LM_ERR("nsq init() done\n");

	return 0;
}

int
nsq_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	return eventData == NULL ? pv_get_null(msg, param, res) : pv_get_strzval(msg, param, res, eventData);
}


int nsq_consumer_fire_event(char *key_obj_fire)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;

	LM_ERR("searching event_route[%s]\n", key_obj_fire);
	rt = route_get(&event_rt, key_obj_fire);
	if (rt < 0 || event_rt.rlist[rt] == NULL)
	{
		LM_ERR("route %s does not exist\n", key_obj_fire);
		return -2;
	}
	LM_ERR("executing event_route[%s] (%d)\n", key_obj_fire, rt);
	if(faked_msg_init()<0)
		return -2;
	fmsg = faked_msg_next();
	rtb = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	set_route_type(rtb);

	return 0;
}

void nsq_consumer_event(char *payload)
{
	struct json_object *jsobj, *key_obj, *subkey_obj;
	struct json_tokener *jstok;
	char buffer[512];
	char *p;
	char *key =  consumer_event_key.s ;
	char *subkey = consumer_event_subkey.s;
	const char *key_obj_value, *subkey_obj_value;

	LM_ERR("Got a consumer event\n");

	eventData = payload;

	jstok = json_tokener_new();
	jsobj = json_tokener_parse_ex(jstok, payload, strlen(payload));
	if (!jsobj) {
		LM_ERR("%s: error parsing JSON\n", __FUNCTION__);
		json_tokener_free(jstok);
		return;
	}

	json_object_object_get_ex(jsobj, key, &key_obj);
	json_object_object_get_ex(jsobj, subkey, &subkey_obj);
	key_obj_value = json_object_get_string(key_obj);
	subkey_obj_value = json_object_get_string(subkey_obj);

	if (key_obj_value && subkey_obj_value) {
		sprintf(buffer, "nsq:consumer-event-%.*s-%.*s",
				(int)strlen(key_obj_value), key_obj_value,
				(int)strlen(subkey_obj_value), subkey_obj_value);
		for (p=buffer ; *p; ++p) *p = tolower(*p);
		for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
		if(nsq_consumer_fire_event(buffer) != 0) {
			sprintf(buffer, "nsq:consumer-event-%.*s",
					(int)strlen(key_obj_value), key_obj_value);
			for (p=buffer ; *p; ++p) *p = tolower(*p);
			for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
			if(nsq_consumer_fire_event(buffer) != 0) {
				sprintf(buffer, "nsq:consumer-event-%.*s-%.*s",
						(int)strlen(key), key,
						(int)strlen(subkey), subkey);
				for (p=buffer ; *p; ++p) *p = tolower(*p);
				for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
				if(nsq_consumer_fire_event(buffer) != 0) {
					sprintf(buffer, "nsq:consumer-event-%.*s",
							(int)strlen(key), key);
					for (p=buffer ; *p; ++p) *p = tolower(*p);
					for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
					if(nsq_consumer_fire_event(buffer) != 0) {
						sprintf(buffer, "nsq:consumer-event");
						if(nsq_consumer_fire_event(buffer) != 0) {
							LM_ERR("nsq:consumer-event not found");
						}
					}

				}
			}
		}
	}
	else if (key_obj_value) {
		sprintf(buffer, "nsq:consumer-event-%.*s",
				(int)strlen(key_obj_value), key_obj_value);
		for (p=buffer ; *p; ++p) *p = tolower(*p);
		for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
		if(nsq_consumer_fire_event(buffer) != 0) {
			sprintf(buffer, "nsq:consumer-event-%.*s-%.*s",
					(int)strlen(key), key,
					(int)strlen(subkey), subkey);
			for (p=buffer ; *p; ++p) *p = tolower(*p);
			for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
			if(nsq_consumer_fire_event(buffer) != 0) {
				sprintf(buffer, "nsq:consumer-event-%.*s",
						(int)strlen(key), key);
				for (p=buffer ; *p; ++p) *p = tolower(*p);
				for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
				if(nsq_consumer_fire_event(buffer) != 0) {
					sprintf(buffer, "nsq:consumer-event");
					if(nsq_consumer_fire_event(buffer) != 0) {
						LM_ERR("nsq:consumer-event not found");
					}
				}

			}
		}
	}
	else {
		sprintf(buffer, "nsq:consumer-event-%.*s-%.*s",
				(int)strlen(key), key,
				(int)strlen(subkey), subkey);
		for (p=buffer ; *p; ++p) *p = tolower(*p);
		for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
		if(nsq_consumer_fire_event(buffer) != 0) {
			sprintf(buffer, "nsq:consumer-event-%.*s",
					(int)strlen(key), key);
			for (p=buffer ; *p; ++p) *p = tolower(*p);
			for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
			if(nsq_consumer_fire_event(buffer) != 0) {
				sprintf(buffer, "nsq:consumer-event");
				if(nsq_consumer_fire_event(buffer) != 0) {
					LM_ERR("nsq:consumer-event not found");
				}
			}

		}
	}

    eventData = NULL;

	return;
}

void nsq_consumer_handler(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg, void *ctx)
{
    int ret = 0;
    nsq_consumer_event(msg->body);
    buffer_reset(conn->command_buf);
    if(ret < 0){
        nsq_requeue(conn->command_buf, msg->id, 100);
    }else{
        nsq_finish(conn->command_buf, msg->id);
    }
    buffered_socket_write_buffer(conn->bs, conn->command_buf);
    buffer_reset(conn->command_buf);
    nsq_ready(conn->command_buf, rdr->max_in_flight);
    buffered_socket_write_buffer(conn->bs, conn->command_buf);

    free_nsq_message(msg);
}

void nsq_consumer_proc(int child_no)
{
    struct NSQReader *rdr;
    struct ev_loop *loop;
    void *ctx = NULL;
    char ip[20];
    int port;

    loop = ev_default_loop(0);
    rdr = new_nsq_reader(loop, "phone-registration", local_hn.s, (void *)ctx,
        NULL, NULL, nsq_consumer_handler);
	sscanf(nsqd_address.s, "%99[^:]:%99d", ip, &port);
    nsq_reader_add_nsqlookupd_endpoint(rdr, ip, port);
    nsq_run(loop);

	return;
}

/* module child initialization function */
int
child_init(int rank)
{
        struct NSQReader *rdr;
        struct ev_loop *loop;
        void *ctx = NULL;
        int ret;
	int pid;

	if (rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0;

	if (rank==PROC_MAIN) {
		pid=fork_process(2, "NSQ Consumer", 1);
		LM_ERR("%s:%d, pid %d\n", __FUNCTION__, __LINE__, pid);
		if (pid<0) {
			LM_ERR("Can't fork\n");
			return -1; /* error */
		} if(pid==0){
			LM_ERR("Starting consumer proc\n");
			nsq_consumer_proc(1);
		}
	}

	LM_ERR("child_init enter\n");
        LM_ERR("Setting up loop\n");
        loop = ev_default_loop(0);
        LM_ERR("Creating new reader for %s/%s\n", consumer_topic.s, local_hn.s);
        rdr = new_nsq_reader(loop, "phone-registration", "nsq",
                (void *)ctx, NULL, NULL, message_handler);
        LM_ERR("Connecting to nsqd\n");
        ret = nsq_reader_connect_to_nsqd(rdr, "127.0.0.1", 4151);
        LM_ERR("connect to nsqd returned %d\n", ret);
        LM_ERR("Adding new endpoint\n");
        nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
        LM_ERR("Running loop\n");
        nsq_run(loop);

        LM_ERR("Off looping\n");

	LM_ERR("child_init leave\n");
	return 0;
}

static param_export_t params[]=
{
		{"hostname", STR_PARAM, &local_hn.s},
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
		"nsq",
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
