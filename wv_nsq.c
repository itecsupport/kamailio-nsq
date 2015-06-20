#include <json-c/json.h>
#include "nsq.h"
#include "http.h"
#include "utlist.h"
#include "wv_nsq.h"

MODULE_VERSION

void message_handler(struct HttpRequest *req, struct HttpResponse *resp, void *arg)
{
	struct json_object *jsobj, *data, *producers, *producer, *broadcast_address_obj, *tcp_port_obj;
	struct json_tokener *jstok;
	const char *broadcast_address;
	int i, tcp_port;

	LM_ERR("%s: status_code %d, body %.*s\n", __FUNCTION__, resp->status_code,
			(int)BUFFER_HAS_DATA(resp->data), resp->data->data);

	if (resp->status_code != 200) {
		free_http_response(resp);
		free_http_request(req);
		return;
	}

	jstok = json_tokener_new();
	jsobj = json_tokener_parse_ex(jstok, resp->data->data, (int)BUFFER_HAS_DATA(resp->data));
	if (!jsobj) {
		LM_ERR("%s: error parsing JSON\n", __FUNCTION__);
		json_tokener_free(jstok);
		return;
	}

	data = json_object_object_get(jsobj, "data");
	if (!jsobj) {
		LM_ERR("%s: error getting 'data' key\n", __FUNCTION__);
		json_object_put(jsobj);
		json_tokener_free(jstok);
		return;
	}
	producers = json_object_object_get(data, "producers");
	if (!producers) {
		LM_ERR("%s: error getting 'producers' key\n", __FUNCTION__);
		json_object_put(jsobj);
		json_tokener_free(jstok);
		return;
	}

	LM_ERR("%s: num producers %d\n", __FUNCTION__, json_object_array_length(producers));
	for (i = 0; i < json_object_array_length(producers); i++) {
		producer = json_object_array_get_idx(producers, i);
		broadcast_address_obj = json_object_object_get(producer, "broadcast_address");
		tcp_port_obj = json_object_object_get(producer, "tcp_port");

		broadcast_address = json_object_get_string(broadcast_address_obj);
		tcp_port = json_object_get_int(tcp_port_obj);

		LM_ERR("%s: broadcast_address %s, port %d\n", __FUNCTION__, broadcast_address, tcp_port);

	}

	json_object_put(jsobj);
	json_tokener_free(jstok);

	free_http_response(resp);
	free_http_request(req);
}

static int nsq_query(struct sip_msg* msg)
{
	LM_ERR("Hello from NSQ module\n");
    struct HttpRequest *req;
	struct HttpClient * http_client;
    struct ev_loop *loop;
    char buf[256];

    loop = ev_default_loop(0);
    sprintf(buf, "http://%s:%d/lookup?topic=%s", "127.0.0.1", 4161, "test");
	http_client = new_http_client(loop);
    req = new_http_request(buf, message_handler, buf);
    http_client_get(http_client, req);
	nsq_run(loop);

	return 0;
}

static cmd_export_t cmds[]=
{
	/* nsq.c */
	{ "nsq_query", (cmd_function) nsq_query,
	  0, 0, 0,
	  ANY_ROUTE }
};

static int mod_init(void)
{
	LM_ERR("nsq loaded\n");
	return 0;
}

static void destroy(void)
{
	LM_ERR("nsq destroyed\n");
}

static param_export_t params[]=
{
	{ 0, 0, 0 }
};

struct module_exports exports = {
	"nsq",
	DEFAULT_DLFLAGS, 	/* dlopen flags */
	cmds,       		/* Exported functions */
	params,     		/* Exported parameters */
	0,          		/* exported statistics */
	0,          		/* exported MI functions */
	0,         			/* exported pseudo-variables */
	0,          		/* extra processes */
	mod_init,   		/* module initialization function */
	0,          		/* response function */
	destroy,          	/* destroy function */
	0           		/* child initialization function */
};



