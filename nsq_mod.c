#include "../../sr_module.h"

#include "nsq.h"

MODULE_VERSION
    
static int init(void);

struct module_exports exports = {
                "nsq",
                DEFAULT_DLFLAGS,        /* dlopen flags */
                0,                                  /* Exported functions */
                0, // params,                         /* Exported parameters */
                0,                                      /* exported statistics */
                0,                      /* exported MI functions */
                0, // nsq_mod_pvs,            /* exported pseudo-variables */
                0,                                      /* extra processes */
                init,                   /* module initialization function */
                0,                                      /* response function*/
                0,                                      /* destroy function */
                0 // child_init         /* per-child init function */
};

str default_ch = {0,0};
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
    struct ev_loop *loop;
    void *ctx = NULL;


        /*
        ** 0 is reserved
        */
        for (j = 1; j < (&event_rt)->entries; j++) {
                struct action* a = (&event_rt)->rlist[j];
                if (!a)
                        continue;
                LM_ERR("Looking at route %s\n", a->rname);
                while (a) {
                        char *cp1, *cp2;
                        cp1 = strchr(a->rname, ':');
                        if (!cp1) {
                                LM_ERR("Don't care about route %s\n", a->rname);
                                a = a->next;
                                continue;
                        }
                        *cp1 = '\0';
                        cp1++;
                        cp2 = a->rname;
                        if (strcmp(cp2, default_ch.s) == 0) {
				loop = ev_default_loop(0);
                                LM_ERR("I want %s/%s\n", cp2, cp1);
                                rdr = new_nsq_reader(loop, cp1, cp2, (void*)ctx,
					NULL, NULL, message_handler);
				nsq_reader_add_nsqlookup_endpoint, rdr, "127.0.0.1", 4161);
				nsq_run(loop);
                        } else {
                                LM_ERR("Route %s/%s doesn't match %s\n", cp1, cp2, default_ch.s);
                        }
                        a = a->next;
                }
        }


    loop = ev_default_loop(0);
    rdr = new_nsq_reader(loop, "test2", "ch2", (void *)ctx,
        NULL, NULL, message_handler);
    nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
    nsq_run(loop);

    return 0;
}
