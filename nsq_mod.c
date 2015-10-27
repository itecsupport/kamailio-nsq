#include "../../sr_module.h"
#include "../../daemonize.h"
#include "../../fmsg.h"

#include "nsq.h"

#include <sys/types.h>
#include <sys/wait.h>

MODULE_VERSION
    

str default_channel = {0,0};
str lookupd_address = {0,0};
str consumer_topic = {0,0};
str consumer_channel = {0,0};
str nsqd_address = {0,0};
str consumer_event_key = {0,0};
str consumer_event_subkey = {0,0};
/* database connection */
char* eventData = NULL;

str nsqA = {0, 0};
str nsqE = {0, 0};

/*
** forward decls
*/
static int init(void);
int nsq_pv_get_event_payload(struct sip_msg*, pv_param_t*, pv_value_t*);

static param_export_t params[]=
{
		{"default_channel", STR_PARAM, &default_channel.s},
                {"lookupd_address", STR_PARAM, &lookupd_address.s},
                {"consumer_topic", STR_PARAM, &consumer_topic.s},
                {"consumer_channel", STR_PARAM, &consumer_channel.s},
                {"nsqd_address", STR_PARAM, &nsqd_address.s},
                {"consumer_event_key", STR_PARAM, &consumer_event_key.s},
                {"consumer_event_subkey", STR_PARAM, &consumer_event_subkey.s},
                { 0, 0, 0 }
};

static pv_export_t nsq_mod_pvs[] = {
	{{"nqE", (sizeof("nsqE")-1)}, PVT_OTHER, nsq_pv_get_event_payload, 0, 0, 0, 0, 0},
	{{"nqA", (sizeof("nsqA")-1)}, PVT_OTHER, nsq_pv_get_event_payload, 0, 0, 0, 0, 0},
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
                "nsq",
                DEFAULT_DLFLAGS,        /* dlopen flags */
                0,                                  /* Exported functions */
                params,                         /* Exported parameters */
                0,                                      /* exported statistics */
                0,                      /* exported MI functions */
                nsq_mod_pvs,            /* exported pseudo-variables */
                0,                                      /* extra processes */
                init,                   /* module initialization function */
                0,                                      /* response function*/
                0,                                      /* destroy function */
                0			/* per-child init function */
};

int
nsq_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	return eventData == NULL ? pv_get_null(msg, param, res) :
		pv_get_strzval(msg, param, res, eventData);
}

int
nsq_consumer_fire_event(char *routename)
{
        struct sip_msg *fmsg;
        struct run_act_ctx ctx;
        int rtb, rt;

        rt = route_get(&event_rt, routename);
        if (rt < 0 || event_rt.rlist[rt] == NULL)
        {
                LM_ERR("route %s does not exist\n", routename);
                return -2;
        }
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

static void
message_handler(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg, void *ctx)
{
    int ret = 0;
    char buf[256];

    snprintf(buf, 255, "%s:%s", rdr->channel, rdr->topic);
    ret = nsq_consumer_fire_event(buf);

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

static int
init(void)
{
	struct NSQReader *rdr;
	void *ctx = NULL;
	struct ev_loop *loop = ev_default_loop(0);
	int j;

        for (j = 0; j < (&event_rt)->entries; j++) {
                struct action* a = (&event_rt)->rlist[j];
                if (!a)
                        continue;
                while (a) {
                        char *cp1, *cp2, *cp3;

			cp3 = strdup(a->rname);
                        cp1 = strchr(cp3, ':');
                        if (!cp1) {
				free(cp3);
                                a = a->next;
                        }
                        *cp1 = '\0';
                        cp1++;
                        cp2 = cp3;
                        if (strcmp(cp2, default_channel.s) == 0) {
				rdr = new_nsq_reader(loop, cp1, cp2, (void*)ctx, NULL, NULL,
					message_handler);
				nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
                        }
			free(cp3);
			a = a->next;
                }
        }
	nsq_run(loop);

	return 0;
}
