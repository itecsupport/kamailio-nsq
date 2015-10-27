#include "../../sr_module.h"
#include "../../daemonize.h"

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
    void *ctx = NULL;
    struct ev_loop *loop = ev_default_loop(0);
    int j;

#if 1
	LM_ERR("Going over %d entries\n", (&event_rt)->entries);

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
                                a = a->next;
                        }
                        *cp1 = '\0';
                        cp1++;
			LM_ERR("cp1 = %s\n", cp1);
                        cp2 = cp3;
			LM_ERR("cp2 = %s\n", cp2);
                        if (strcmp(cp2, default_channel.s) == 0) {
                                LM_ERR("I want %s:%s\n", cp2, cp1);
				LM_ERR("Creating reader\n");
				rdr = new_nsq_reader(loop, cp1, cp2, (void*)ctx, NULL, NULL, message_handler);
				LM_ERR("Creating new endpoint\n");
				nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
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
	nsq_run(loop);
	LM_ERR("Final value of j is %d\n", j);

#else

    struct ev_loop *loop = ev_default_loop(0);
    struct ev_loop *loop2 = ev_default_loop(0);

    LM_ERR("Going into 1st loop\n");
    rdr = new_nsq_reader(loop, "test", "ch", (void*)ctx,
	NULL, NULL, message_handler);
    nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
    // nsq_run(loop);

    LM_ERR("Going into 2nd loop\n");
    rdr = new_nsq_reader(loop, "test1", "ch", (void*)ctx,
	NULL, NULL, message_handler);
    nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
    nsq_run(loop);

#endif

    return 0;
}
