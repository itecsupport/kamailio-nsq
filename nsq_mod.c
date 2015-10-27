#include "../../sr_module.h"

#include "nsq.h"

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

static int init(void);

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

struct module_exports exports = {
                "nsq",
                DEFAULT_DLFLAGS,        /* dlopen flags */
                0,                                  /* Exported functions */
                params,                         /* Exported parameters */
                0,                                      /* exported statistics */
                0,                      /* exported MI functions */
                0, // nsq_mod_pvs,            /* exported pseudo-variables */
                0,                                      /* extra processes */
                init,                   /* module initialization function */
                0,                                      /* response function*/
                0,                                      /* destroy function */
                0 // child_init         /* per-child init function */
};

static void
message_handler(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg, void *ctx)
{
    int ret = 0;

    fprintf(stdout, "In message_handler\n");

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
    fprintf(stdout, "Message freed\n");
}

static int
init(void)
{
    struct NSQReader *rdr;
    struct ev_loop **loops;
    void *ctx = NULL;
    int j;

	LM_ERR("Going over %d entries\n", (&event_rt)->entries);
	loops = calloc(sizeof(struct ev_loop*), (&event_rt)->entries);

        for (j = 0; j < (&event_rt)->entries; j++) {
                struct action* a = (&event_rt)->rlist[j];
		LM_ERR("Entry %d\n", j);
                if (!a)
                        continue;
                LM_ERR("Looking at route %s\n", a->rname);
                while (a) {
                        char *cp1, *cp2, *cp3;
			cp3 = strdup(a->rname);
                        cp1 = strchr(cp3, ':');
                        if (!cp1) {
				LM_ERR("Can't find : in %s\n", a->rname);
                                LM_ERR("Don't care about route %s\n", cp3);
				free(cp3);
                                break;
                        }
                        *cp1 = '\0';
                        cp1++;
			LM_ERR("cp1 = %s\n", cp1);
                        cp2 = cp3;
			LM_ERR("cp2 = %s\n", cp2);
                        if (strcmp(cp2, default_channel.s) == 0) {
				loops[j] = calloc(sizeof(struct ev_loop*), 1);
				loops[j] = ev_default_loop(0);
				struct ev_loop *loop = loops[j];
                                LM_ERR("I want %s:%s\n", cp2, cp1);
                                rdr = new_nsq_reader(loop, cp1, cp2, (void*)ctx,
					NULL, NULL, message_handler);
				nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
				nsq_run(loop);
                        } else {
                                LM_ERR("Route %s:%s (%s) doesn't match %s\n", cp1, cp2, a->rname,
					default_channel.s);
                        }
			free(cp3);
			a = a->next;
                }
		LM_ERR("%p should be NULL\n", a);
		if (a) {
			LM_ERR("It's not, a->next is %p\n", a->next);
		}
        }
	LM_ERR("Final value of j is %d\n", j);


/*
    struct ev_loop *loop = ev_default_loop(0);
    rdr = new_nsq_reader(loop, "test", "ch", (void *)ctx,
        NULL, NULL, message_handler);
    nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
    nsq_run(loop);
*/

    return 0;
}
