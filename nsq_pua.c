#include <stdio.h>
#include <json-c/json.h>

#include "../../sr_module.h"
#include "defs.h"
#include "const.h"
#include "../presence/bind_presence.h"
#include "../../pvar.h"
#include "../pua/pua.h"
#include "../pua/pua_bind.h"
#include "../pua/send_publish.h"

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../cfg/cfg_struct.h"
#include "../../lib/srdb1/db.h"


extern db1_con_t *knsq_pa_db;
extern db_func_t knsq_pa_dbf;
extern str knsq_presentity_table;

int nsq_pua_update_presentity(str* event, str* realm, str* user, str* etag, str* sender, str* body, int expires, int reset)
{
	db_key_t query_cols[12];
	db_op_t  query_ops[12];
	db_val_t query_vals[12];
	int n_query_cols = 0;
	int ret = -1;
	int use_replace = 1;

	query_cols[n_query_cols] = &str_event_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *event;
	n_query_cols++;

	query_cols[n_query_cols] = &str_domain_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *realm;
	n_query_cols++;

	query_cols[n_query_cols] = &str_username_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_etag_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *etag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_sender_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *sender;
	n_query_cols++;

	query_cols[n_query_cols] = &str_body_col;
	query_vals[n_query_cols].type = DB1_BLOB;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *body;
	n_query_cols++;

	query_cols[n_query_cols] = &str_received_time_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = (int)time(NULL);
	n_query_cols++;

	query_cols[n_query_cols] = &str_expires_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = expires;
	n_query_cols++;

	if (knsq_pa_dbf.use_table(knsq_pa_db, &knsq_presentity_table) < 0)
	{
		LM_ERR("unsuccessful use_table\n");
		goto error;
	}

	if (knsq_pa_dbf.replace == NULL || reset > 0)
	{
		use_replace = 0;
		LM_DBG("using delete/insert instead of replace\n");
	}

	if (knsq_pa_dbf.start_transaction)
	{
		if (knsq_pa_dbf.start_transaction(knsq_pa_db, DB_LOCKING_WRITE) < 0)
		{
			LM_ERR("in start_transaction\n");
			goto error;
		}
	}

	if(use_replace) {
		if (knsq_pa_dbf.replace(knsq_pa_db, query_cols, query_vals, n_query_cols, 4, 0) < 0)
		{
			LM_ERR("replacing record in database\n");
			if (knsq_pa_dbf.abort_transaction)
			{
				if (knsq_pa_dbf.abort_transaction(knsq_pa_db) < 0)
					LM_ERR("in abort_transaction\n");
			}
			goto error;
		}
	} else {
		if (knsq_pa_dbf.delete(knsq_pa_db, query_cols, query_ops, query_vals, 4-reset) < 0)
		{
			LM_ERR("deleting record in database\n");
			if (knsq_pa_dbf.abort_transaction)
			{
				if (knsq_pa_dbf.abort_transaction(knsq_pa_db) < 0)
					LM_ERR("in abort_transaction\n");
			}
			goto error;
		}
		if (knsq_pa_dbf.insert(knsq_pa_db, query_cols, query_vals, n_query_cols) < 0)
		{
			LM_ERR("replacing record in database\n");
			if (knsq_pa_dbf.abort_transaction)
			{
				if (knsq_pa_dbf.abort_transaction(knsq_pa_db) < 0)
					LM_ERR("in abort_transaction\n");
			}
			goto error;
		}
	}

	if (knsq_pa_dbf.end_transaction)
	{
		if (knsq_pa_dbf.end_transaction(knsq_pa_db) < 0)
		{
			LM_ERR("in end_transaction\n");
			goto error;
		}
	}

	error:

	return ret;
}

int nsq_pua_publish_dialoginfo_to_presentity(struct json_object *json_obj) {

	struct json_object *obj;
	int ret = 1;
	str from = {0, 0}, to = {0, 0}, pres = {0, 0};
	str from_user = {0, 0}, to_user = {0, 0}, pres_user = {0, 0};
	str from_realm = {0, 0}, to_realm = {0, 0}, pres_realm = {0, 0};
	str from_uri = {0, 0}, to_uri = {0, 0};
	str callid = {0, 0}, fromtag = {0, 0}, totag = {0, 0};
	str state = {0, 0};
	str direction = {0, 0};
	char sender_buf[1024];
	str sender = {0, 0};
	str dialoginfo_body = {0, 0};
	int expires = 0;
	str event = str_init("dialog");
	int reset = 0;
	char to_tag_buffer[100];
	char from_tag_buffer[100];
	char *obj_name = NULL;

	char *body = (char *)pkg_malloc(DIALOGINFO_BODY_BUFFER_SIZE);
	if(body == NULL) {
		LM_ERR("Error allocating buffer for publish\n");
		ret = -1;
		goto error;
	}

	obj = json_object_object_get(json_obj, BLF_JSON_PRES);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		pres.s = obj_name;
		pres.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_PRES_USER);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		pres_user.s = obj_name;
		pres_user.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_PRES_REALM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		pres_realm.s = obj_name;
		pres_realm.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_FROM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from.s = obj_name;
		from.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_FROM_USER);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from_user.s = obj_name;
		from_user.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_FROM_REALM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from_realm.s = obj_name;
		from_realm.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_FROM_URI);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from_uri.s = obj_name;
		from_uri.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TO);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to.s = obj_name;
		to.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TO_USER);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to_user.s = obj_name;
		to_user.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TO_REALM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to_realm.s = obj_name;
		to_realm.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TO_URI);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to_uri.s = obj_name;
		to_uri.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_CALLID);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		callid.s = obj_name;
		callid.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_FROMTAG);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		fromtag.s = obj_name;
		fromtag.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TOTAG);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		totag.s = obj_name;
		totag.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_DIRECTION);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		direction.s = obj_name;
		direction.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_STATE);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		state.s = obj_name;
		state.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_EXPIRES);
	if (obj) {
		expires = json_object_get_int(obj);
		LM_ERR("%s:%d BLF_JSON_EXPIRES %d\n", __FUNCTION__, __LINE__, expires);
		if(expires > 0)
			expires += (int)time(NULL);
		LM_ERR("%s:%d BLF_JSON_EXPIRES %d\n", __FUNCTION__, __LINE__, expires);
	}

	obj = json_object_object_get(json_obj, "Flush-Level");
	if(obj != NULL) {
		reset = json_object_get_int(obj);
		LM_ERR("%s:%d Flush-Level %d\n", __FUNCTION__, __LINE__, reset);
	}

	if (!from.len || !to.len || !state.len) {
		LM_ERR("missing one of From / To / State\n");
		goto error;
	}

	if(!pres.len || !pres_user.len || !pres_realm.len) {
		pres = from;
		pres_user = from_user;
		pres_realm = from_realm;
	}

	if(!from_uri.len)
		from_uri = from;

	if(!to_uri.len)
		to_uri = to;

	if(fromtag.len > 0) {
		fromtag.len = sprintf(from_tag_buffer, LOCAL_TAG, fromtag.len, fromtag.s);
		fromtag.s = from_tag_buffer;
	}

	if(totag.len > 0) {
		totag.len = sprintf(to_tag_buffer, REMOTE_TAG, totag.len, totag.s);
		totag.s = to_tag_buffer;
	}

	if(callid.len) {
		sprintf(body, DIALOGINFO_BODY,
				pres.len, pres.s,
				callid.len, callid.s,
				callid.len, callid.s,
				fromtag.len, fromtag.s,
				totag.len, totag.s,
				direction.len, direction.s,
				state.len, state.s,
				from_user.len, from_user.s,
				from.len, from.s,
				from_uri.len, from_uri.s,
				to_user.len, to_user.s,
				to.len, to.s,
				to_uri.len, to_uri.s
		);
	} else {
		sprintf(body, DIALOGINFO_EMPTY_BODY, pres.len, pres.s);
	}

	LM_ERR("%s:%d body %s\n", __FUNCTION__, __LINE__, body);

	sprintf(sender_buf, "sip:%s",callid.s);
	sender.s = sender_buf;
	sender.len = strlen(sender_buf);

	dialoginfo_body.s = body;
	dialoginfo_body.len = strlen(body);

	nsq_pua_update_presentity(&event, &pres_realm, &pres_user, &callid, &sender, &dialoginfo_body, expires, reset);

	error:

	if(body)
		pkg_free(body);

	return ret;


}


int nsq_pua_publish_mwi_to_presentity(struct json_object *json_obj) {
    int ret = 1;
    struct json_object *obj;
    char *obj_name = NULL;
    str direction = {0, 0};

    str event = str_init("message-summary");
    str from = { 0, 0 }, to = { 0, 0 };
    str from_user = { 0, 0 }, to_user = { 0, 0 };
    str from_realm = { 0, 0 }, to_realm = { 0, 0 };
    str callid = { 0, 0 }, fromtag = { 0, 0 }, totag = { 0, 0 };
    str mwi_user = { 0, 0 }, mwi_waiting = { 0, 0 },
        mwi_new = { 0, 0 }, mwi_saved = { 0, 0 },
        mwi_urgent = { 0, 0 }, mwi_urgent_saved = { 0, 0 },
        mwi_account = { 0, 0 }, mwi_body = { 0, 0 };
    int expires = 0;

    char *body = (char *)pkg_malloc(MWI_BODY_BUFFER_SIZE);
    if(body == NULL) {
    	LM_ERR("Error allocating buffer for publish\n");
    	ret = -1;
    	goto error;
    }

	obj = json_object_object_get(json_obj, BLF_JSON_FROM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from.s = obj_name;
		from.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_FROM_USER);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from_user.s = obj_name;
		from_user.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_FROM_REALM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from_realm.s = obj_name;
		from_realm.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_REALM %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TO);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to.s = obj_name;
		to.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_TO %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TO_USER);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to_user.s = obj_name;
		to_user.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_TO_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TO_REALM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to_realm.s = obj_name;
		to_realm.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_TO_REALM %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_CALLID);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		callid.s = obj_name;
		callid.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_CALLID %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_FROMTAG);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		fromtag.s = obj_name;
		fromtag.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROMTAG %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TOTAG);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		totag.s = obj_name;
		totag.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_TOTAG %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, MWI_JSON_TO);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		mwi_user.s = obj_name;
		mwi_user.len = strlen(obj_name);
		LM_ERR("%s:%d MWI_JSON_TO %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, MWI_JSON_WAITING);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		mwi_waiting.s = obj_name;
		mwi_waiting.len = strlen(obj_name);
		LM_ERR("%s:%d MWI_JSON_WAITING %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, MWI_JSON_NEW);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		mwi_new.s = obj_name;
		mwi_new.len = strlen(obj_name);
		LM_ERR("%s:%d MWI_JSON_NEW %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, MWI_JSON_SAVED);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		mwi_saved.s = obj_name;
		mwi_saved.len = strlen(obj_name);
		LM_ERR("%s:%d MWI_JSON_SAVED %s\n", __FUNCTION__, __LINE__, obj_name);
	}
	obj = json_object_object_get(json_obj, MWI_JSON_URGENT);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		mwi_urgent.s = obj_name;
		mwi_urgent.len = strlen(obj_name);
		LM_ERR("%s:%d MWI_JSON_URGENT %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, MWI_JSON_URGENT_SAVED);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		mwi_urgent_saved.s = obj_name;
		mwi_urgent_saved.len = strlen(obj_name);
		LM_ERR("%s:%d MWI_JSON_URGENT_SAVED %s\n", __FUNCTION__, __LINE__, obj_name);
	}
	obj = json_object_object_get(json_obj, MWI_JSON_ACCOUNT);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		mwi_account.s = obj_name;
		mwi_account.len = strlen(obj_name);
		LM_ERR("%s:%d MWI_JSON_ACCOUNT %s\n", __FUNCTION__, __LINE__, obj_name);
	}




    struct json_object* ExpiresObj =  json_object_object_get(json_obj, BLF_JSON_EXPIRES);
    if(ExpiresObj != NULL) {
    	expires = json_object_get_int(ExpiresObj);
    	if(expires > 0)
    		expires += (int)time(NULL);
    }

    sprintf(body, MESSAGE_SAMMURY, mwi_waiting.len, mwi_waiting.s,
	    mwi_account.len, mwi_account.s, mwi_new.len, mwi_new.s,
	    mwi_saved.len, mwi_saved.s, mwi_urgent.len, mwi_urgent.s,
	    mwi_urgent_saved.len, mwi_urgent_saved.s);

    mwi_body.s = body;
    mwi_body.len = strlen(body);

    nsq_pua_update_presentity(&event, &from_realm, &from_user, &callid, &from, &mwi_body, expires, 1);


 error:

   if(body)
	  pkg_free(body);


   return ret;
}





int nsq_pua_publish_presence_to_presentity(struct json_object *json_obj) {
    int ret = 1;
    str from = { 0, 0 }, to = { 0, 0 };
    str from_user = { 0, 0 }, to_user = { 0, 0 };
    str from_realm = { 0, 0 }, to_realm = { 0, 0 };
    str callid = { 0, 0 }, fromtag = { 0, 0 }, totag = { 0, 0 };
    str state = { 0, 0 };
    str direction = { 0, 0 };
    str event = str_init("presence");
    str presence_body = { 0, 0 };
    str activity = str_init("");
    str note = str_init("Idle");
    str status = str_presence_status_online;
    int expires = 0;
    struct json_object *obj;
     char *obj_name = NULL;

    char *body = (char *)pkg_malloc(PRESENCE_BODY_BUFFER_SIZE);
    if(body == NULL) {
    	LM_ERR("Error allocating buffer for publish\n");
    	ret = -1;
    	goto error;
    }


	obj = json_object_object_get(json_obj, BLF_JSON_FROM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from.s = obj_name;
		from.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM %s\n", __FUNCTION__, __LINE__, obj_name);
	}
	obj = json_object_object_get(json_obj, BLF_JSON_FROM_USER);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from_user.s = obj_name;
		from_user.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}
	obj = json_object_object_get(json_obj, BLF_JSON_FROM_REALM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		from_realm.s = obj_name;
		from_realm.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROM_REALM %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TO);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to.s = obj_name;
		to.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_TO %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_TO_USER);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to_user.s = obj_name;
		to_user.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_TO_USER %s\n", __FUNCTION__, __LINE__, obj_name);
	}
	obj = json_object_object_get(json_obj, BLF_JSON_TO_REALM);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		to_realm.s = obj_name;
		to_realm.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_TO_REALM %s\n", __FUNCTION__, __LINE__, obj_name);
	}
	obj = json_object_object_get(json_obj, BLF_JSON_CALLID);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		callid.s = obj_name;
		callid.len = strlen(obj_name);;
		LM_ERR("%s:%d BLF_JSON_CALLID %s\n", __FUNCTION__, __LINE__, obj_name);
	}
	obj = json_object_object_get(json_obj, BLF_JSON_FROMTAG);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		fromtag.s = obj_name;
		fromtag.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_FROMTAG %s\n", __FUNCTION__, __LINE__, obj_name);
	}
	obj = json_object_object_get(json_obj, BLF_JSON_TOTAG);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		totag.s = obj_name;
		totag.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_TOTAG %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_DIRECTION);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		direction.s = obj_name;
		direction.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_DIRECTION %s\n", __FUNCTION__, __LINE__, obj_name);
	}

	obj = json_object_object_get(json_obj, BLF_JSON_STATE);
	obj_name = json_object_get_string(obj);
	if (obj_name) {
		state.s = obj_name;
		state.len = strlen(obj_name);
		LM_ERR("%s:%d BLF_JSON_STATE %s\n", __FUNCTION__, __LINE__, obj_name);
	}


    struct json_object* ExpiresObj =  json_object_object_get(json_obj, BLF_JSON_EXPIRES);
    if(ExpiresObj != NULL) {
    	expires = json_object_get_int(ExpiresObj);
    	if(expires > 0)
    		expires += (int)time(NULL);
    }

    if (!from_user.len || !to_user.len || !state.len) {
    	LM_ERR("missing one of From / To / State\n");
    	goto error;
    }

    if (!strcmp(state.s, "early")) {
    	note = str_presence_note_busy;
    	activity = str_presence_act_busy;

    } else if (!strcmp(state.s, "confirmed")) {
    	note = str_presence_note_otp;
    	activity = str_presence_act_otp;

    } else if (!strcmp(state.s, "offline")) {
    	note = str_presence_note_offline;
    	status = str_presence_status_offline;

    } else {
    	note = str_presence_note_idle;
    }


    sprintf(body, PRESENCE_BODY, from_user.s, callid.s, status.s, note.s, activity.s, note.s);

    presence_body.s = body;
    presence_body.len = strlen(body);

    nsq_pua_update_presentity(&event, &from_realm, &from_user, &callid, &from, &presence_body, expires, 1);

 error:

 if(body)
	  pkg_free(body);

 return ret;

}



int nsq_pua_publish(struct sip_msg* msg, char *payload) {
	struct json_object *json_obj = NULL, *obj = NULL;
	struct json_tokener* tok;
	char *obj_name = NULL, *jsonp= NULL;
	int ret = 1;
	str json ;


	LM_ERR("%s:%d, payload %s\n", __FUNCTION__, __LINE__, payload);

	if(fixup_get_svalue(msg, (gparam_p)payload, &json) != 0) {
		LM_ERR("cannot get payload string value\n");
		json.s = payload;
		json.len = strlen(payload);
	}

	LM_ERR("%s:%d, payload %s\n", __FUNCTION__, __LINE__, json.s);

	tok = json_tokener_new();
	if (!tok) {
		LM_ERR("Error parsing json: cpuld not allocate tokener\n");
		return NULL;
	}

	json_obj = json_tokener_parse_ex(tok, json.s, json.len);
	if (!json_obj) {
		LM_ERR("%s: error parsing JSON\n", __FUNCTION__);
		json_tokener_free(tok);
		return;
	}


	obj = json_object_object_get(json_obj, BLF_JSON_EVENT_NAME);
	obj_name = json_object_get_string(obj);

	if (obj_name) {
		if (strlen(obj_name) == 6 && strncmp(obj_name, "update", 6) == 0) {
			LM_ERR("%s: first obj_name %s\n", __FUNCTION__, obj_name);
			obj = json_object_object_get(json_obj, BLF_JSON_EVENT_PKG);
			obj_name = json_object_get_string(obj);
			if (obj_name) {
				if (strlen(obj_name) == str_event_dialog.len
						&& strncmp(obj_name, str_event_dialog.s, strlen(obj_name)) == 0) {
					LM_ERR("%s: second obj_name %s =>> going to nsq_pua_publish_dialoginfo_to_presentity \n", __FUNCTION__, obj_name);
					ret = nsq_pua_publish_dialoginfo_to_presentity(json_obj);
				}
				 else if (strlen(obj_name) == str_event_message_summary.len
						&& strncmp(obj_name, str_event_message_summary.s,  strlen(obj_name)) == 0) {
					ret = nsq_pua_publish_mwi_to_presentity(json_obj);
				} else if (strlen(obj_name) == str_event_presence.len
						&& strncmp(obj_name, str_event_presence.s, strlen(obj_name)) == 0) {
					ret = nsq_pua_publish_presence_to_presentity(json_obj);
				}
			}
		}
	}

	return ret;
}



