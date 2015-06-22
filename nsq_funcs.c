/**
 * $Id$
 *
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include "nsq.h"
#include "http.h"
#include "utlist.h"

#include "../../mod_fix.h"
#include "../../lvalue.h"

#include "nsq_funcs.h"

extern str lookupd_address;
extern str consumer_topic;
extern str nsqd_address;

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

int nsq_query(struct sip_msg* msg, char* json, char* field, char* dst)
{
    /*struct HttpRequest *req;
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
	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);*/

	return 1;
}

void publish_handler(struct HttpRequest *req, struct HttpResponse *resp, void *arg)
{
	LM_ERR("%s: status_code %d, body %.*s\n", __FUNCTION__, resp->status_code,
			(int)BUFFER_HAS_DATA(resp->data), resp->data->data);
	return;
}

int nsq_publish(struct sip_msg* msg, char* topic, char* payload)
{
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
    sprintf(buf, "http://%s/put?topic=%s", nsqd_address.s, topic_s.s);
	http_client = new_http_client(loop);

	LM_ERR("%s:%d, payload %s\n", __FUNCTION__, __LINE__, payload_s.s);
	/*len = strlen(payload_s.s);
	char *data;
	data = pkg_malloc(len+1);
	memcpy(data, "data", len);*/
    req = new_http_request(buf, publish_handler, buf, payload_s.s);
    http_client_get(http_client, req);
	nsq_run(loop);

	return 1;
}
