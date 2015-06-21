#include <json-c/json.h>
#include "nsq.h"
#include "http.h"
#include "utlist.h"
#include "wv_nsq.h"
#include "../../mod_fix.h"
#include "../../lvalue.h"

str lookupd_address = {0,0};
str consumer_topic = {0,0};


MODULE_VERSION

void query_handler(struct HttpRequest *req, struct HttpResponse *resp, void *arg)
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


void publish_handler(struct HttpRequest *req, struct HttpResponse *resp, void *arg)
{
	LM_ERR("%s: status_code %d, body %.*s\n", __FUNCTION__, resp->status_code,
			(int)BUFFER_HAS_DATA(resp->data), resp->data->data);
	return;
}

int fixup_wv_nsq(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 3) {
		if (fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

int fixup_wv_nsq_free(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2 || param_no == 3) {
		return fixup_free_spve_null(param, 1);
	}

	if (param_no == 4) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int nsq_query(struct sip_msg* msg, char* topic, char* payload, char* dst)
{
    struct HttpRequest *req;
	struct HttpClient * http_client;
    struct ev_loop *loop;
    char buf[256];
	str topic_s;
	str payload_s;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;
	char* last_payload_result = NULL;


	if (fixup_get_svalue(msg, (gparam_p)topic, &topic_s) != 0) {
		LM_ERR("cannot get topic string value\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_p)payload, &payload_s) != 0) {
		LM_ERR("cannot get payload string value\n");
		return -1;
	}

    loop = ev_default_loop(0);
    sprintf(buf, "http://%s/lookup?topic=%s", lookupd_address.s, topic_s.s);
	http_client = new_http_client(loop);

    req = new_http_request(buf, query_handler, buf, NULL);
    http_client_get(http_client, req);
	nsq_run(loop);

	int len = strlen("200");
	last_payload_result = pkg_malloc(len+1);
	memcpy(last_payload_result, "200", len);
	last_payload_result[len] = '\0';
	LM_ERR("last_payload_result %s\n", last_payload_result);

	dst_pv = (pv_spec_t *)dst;
	dst_val.rs.s = last_payload_result;
	dst_val.rs.len = strlen(last_payload_result);
	dst_val.flags = PV_VAL_STR;
	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

	return 0;
}

static int nsq_publish(struct sip_msg* msg, char* topic, char* payload){

    struct HttpRequest *req;
	struct HttpClient * http_client;
    struct ev_loop *loop;
    char buf[256];
	str topic_s;
	str payload_s;

	if (fixup_get_svalue(msg, (gparam_p)topic, &topic_s) != 0) {
		LM_ERR("cannot get topic string value\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_p)payload, &payload_s) != 0) {
		LM_ERR("cannot get payload string value\n");
		return -1;
	}

    loop = ev_default_loop(0);
    sprintf(buf, "http://%s/put?topic=%s", "127.0.0.1:4151", topic_s.s);
	http_client = new_http_client(loop);

	int len = strlen("data");
	char *data;
	data = pkg_malloc(len+1);
	memcpy(data, "data", len);
    req = new_http_request(buf, publish_handler, buf, data);
    http_client_get(http_client, req);
	nsq_run(loop);

	return 0;
}

static cmd_export_t cmds[]=
{
	/* wv_nsq.c */
	{ "nsq_query", (cmd_function) nsq_query, 3, fixup_wv_nsq, fixup_wv_nsq_free, ANY_ROUTE},
	{ "nsq_publish", (cmd_function) nsq_publish, 2, fixup_wv_nsq, fixup_wv_nsq_free, ANY_ROUTE},
	{ 0, 0, 0, 0, 0, 0}
};

static int mod_init(void)
{
	LM_ERR("nsq loaded\n");
	LM_ERR("lookupd_address %s\n", lookupd_address.s);
	LM_ERR("consumer_topic %s\n", consumer_topic.s);
	return 0;
}

static void destroy(void)
{
	LM_ERR("nsq destroyed\n");
}

static param_export_t params[]=
{
		{"lookupd_address", STR_PARAM, &lookupd_address.s},
		{"consumer_topic", STR_PARAM, &consumer_topic.s},
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



