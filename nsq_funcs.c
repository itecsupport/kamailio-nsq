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

#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../lib/kmi/mi.h"
#include "../../cfg/cfg_struct.h"

#include "nsq_funcs.h"
#include "nsq_pua.h"

extern str lookupd_address;
extern str consumer_topic;
extern str consumer_channel;
extern str nsqd_address;
extern str consumer_event_key;
extern str consumer_event_subkey;

struct nsq_cb_data {
	struct sip_msg *msg;
	struct ev_loop *loop;
	char *dst;
};

int dbk_include_entity = 1;
int dbk_pua_mode = 1;

/* database connection */
db1_con_t *nsq_pa_db = NULL;
db_func_t nsq_pa_dbf;
str nsq_presentity_table = str_init("presentity");
str nsq_db_url = {0,0};

void query_handler(struct HttpRequest *req, struct HttpResponse *resp, void *arg)
{
	struct json_object *jsobj, *data, *producers, *producer, *broadcast_address_obj, *tcp_port_obj;
	struct json_tokener *jstok;
	const char *broadcast_address;
	int i, tcp_port;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;
	struct nsq_cb_data *nsq_cb_data = (struct nsq_cb_data *)arg;

	ev_unloop(nsq_cb_data->loop, EVUNLOOP_ALL);

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

	for (i = 0; i < json_object_array_length(producers); i++) {
		producer = json_object_array_get_idx(producers, i);
		broadcast_address_obj = json_object_object_get(producer, "broadcast_address");
		tcp_port_obj = json_object_object_get(producer, "tcp_port");
		broadcast_address = json_object_get_string(broadcast_address_obj);
		tcp_port = json_object_get_int(tcp_port_obj);
		LM_ERR("%s: broadcast_address %s, port %d\n", __FUNCTION__, broadcast_address, tcp_port);

	}

	dst_pv = (pv_spec_t *)nsq_cb_data->dst;
	dst_val.rs.s = resp->data->data;
	dst_val.rs.len = strlen(resp->data->data);
	dst_val.flags = PV_VAL_STR;
	dst_pv->setf(nsq_cb_data->msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

	json_object_put(jsobj);
	json_tokener_free(jstok);

	free_http_response(resp);
	free_http_request(req);
}

int nsq_query(struct sip_msg* msg, char* topic, char* payload, char* dst)
{
    struct HttpRequest *req;
	struct HttpClient * http_client;
    struct ev_loop *loop;
    char buf[256];
	str topic_s;
	str payload_s;
	struct nsq_cb_data *nsq_cb_data;

	if (fixup_get_svalue(msg, (gparam_p)topic, &topic_s) != 0) {
		LM_ERR("cannot get topic string value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)payload, &payload_s) != 0) {
		LM_ERR("cannot get payload string value\n");
		return -1;
	}

    loop = ev_loop_new(0);
    sprintf(buf, "http://%s/lookup?topic=%s", lookupd_address.s, topic_s.s);
	http_client = new_http_client(loop);

	nsq_cb_data = (struct nsq_cb_data *)pkg_malloc(sizeof(struct nsq_cb_data *));
	nsq_cb_data->msg = msg;
	nsq_cb_data->loop = loop;
	nsq_cb_data->dst = dst;
    req = new_http_request(buf, query_handler, nsq_cb_data, NULL);
    http_client_get(http_client, req);
	nsq_run(loop);

	return 1;
}

void publish_handler(struct HttpRequest *req, struct HttpResponse *resp, void *arg)
{
	struct ev_loop *loop = (struct ev_loop *)arg;
	ev_unloop(loop, EVUNLOOP_ALL);
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

    loop = ev_loop_new(0);
    sprintf(buf, "http://%s/put?topic=%s", nsqd_address.s, topic_s.s);
	http_client = new_http_client(loop);
    req = new_http_request(buf, publish_handler, loop, payload_s.s);
    http_client_get(http_client, req);
	nsq_run(loop);

	return 1;
}

char* eventData = NULL;

int nsq_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	return eventData == NULL ? pv_get_null(msg, param, res) : pv_get_strzval(msg, param, res, eventData);
}

int nsq_consumer_fire_event(char *key_obj_fire)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;

	LM_DBG("searching event_route[%s]\n", key_obj_fire);
	rt = route_get(&event_rt, key_obj_fire);
	if (rt < 0 || event_rt.rlist[rt] == NULL)
	{
		LM_DBG("route %s does not exist\n", key_obj_fire);
		return -2;
	}
	LM_DBG("executing event_route[%s] (%d)\n", key_obj_fire, rt);
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

    eventData = payload;

	jstok = json_tokener_new();
	jsobj = json_tokener_parse_ex(jstok, payload, strlen(payload));
	if (!jsobj) {
		LM_ERR("%s: error parsing JSON\n", __FUNCTION__);
		json_tokener_free(jstok);
		return;
	}

	key_obj = json_object_object_get(jsobj, key);
	subkey_obj = json_object_object_get(jsobj, subkey);
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
    rdr = new_nsq_reader(loop, consumer_topic.s, consumer_channel.s, (void *)ctx,
        NULL, NULL, nsq_consumer_handler);
	sscanf(lookupd_address.s, "%99[^:]:%99d", ip, &port);
    nsq_reader_add_nsqlookupd_endpoint(rdr, ip, port);
    nsq_run(loop);

	return;
}



