#include <stdio.h>
#include <string.h>

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../cfg/cfg_struct.h"
#include "../../lib/srdb1/db.h"

#include "nsq_funcs.h"
#include "nsq_pua.h"

MODULE_VERSION

static int fixup_get_field(void** param, int param_no);
static int fixup_get_field_free(void** param, int param_no);

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

str nsqA = {0, 0};
str nsqE = {0, 0};

static int init(void)
{
	int ret = daemon_status_send(1);

	if (ret == -1) {
		LM_DBG("Can't send to daemon");
	}

	knsq_db_url.len = knsq_db_url.s ? strlen(knsq_db_url.s) : 0;
	LM_DBG("db_url=%s/%d/%p\n", ZSW(knsq_db_url.s), knsq_db_url.len, knsq_db_url.s);
	knsq_presentity_table.len = strlen(knsq_presentity_table.s);

	if(knsq_db_url.len > 0) {

		/* binding to database module  */
		if (db_bind_mod(&knsq_db_url, &knsq_pa_dbf))
		{
			LM_ERR("Database module not found\n");
			return -1;
		}


		if (!DB_CAPABILITY(knsq_pa_dbf, DB_CAP_ALL))
		{
			LM_ERR("Database module does not implement all functions"
					" needed by kazoo module\n");
			return -1;
		}

		knsq_pa_db = knsq_pa_dbf.init(&knsq_db_url);
		if (!knsq_pa_db)
		{
			LM_ERR("Connection to database failed\n");
			return -1;
		}

		knsq_pa_dbf.close(knsq_pa_db);
		knsq_pa_db = NULL;
	}

	int total_workers = 10;

	register_procs(total_workers);
	cfg_register_child(total_workers);

	LM_DBG("nsq init() done");

	return 0;
}

/* module child initialization function */
int child_init(int rank)
{
	int pid;

	if (knsq_pa_dbf.init==0)
	{
		LM_CRIT("child_init: database not bound\n");
		return -1;
	}
	knsq_pa_db = knsq_pa_dbf.init(&knsq_db_url);
	if (!knsq_pa_db)
	{
		LM_ERR("child %d: unsuccessful connecting to database\n", rank);
		return -1;
	}

	if (knsq_pa_dbf.use_table(knsq_pa_db, &knsq_presentity_table) < 0)
	{
		LM_ERR( "child %d:unsuccessful use_table presentity_table\n", rank);
		return -1;
	}

	LM_DBG("child %d: Database connection opened successfully\n", rank);

	nsqd_subscribe(rank);

	return 0;
}

/* Exported functions */
static cmd_export_t cmds[]={
		{"nsq_query", (cmd_function)nsq_query, 3, fixup_get_field, fixup_get_field_free, ANY_ROUTE},
		{"nsq_publish", (cmd_function)nsq_publish, 2, fixup_get_field, fixup_get_field_free, ANY_ROUTE},
		{"nsq_pua_publish", (cmd_function) nsq_pua_publish, 1, fixup_get_field, fixup_get_field, ANY_ROUTE},
		{"nsq_encode", (cmd_function) nsq_encode, 2, fixup_nsq_encode, fixup_nsq_encode_free, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]=
{
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
		cmds,			 	/* Exported functions */
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

static int fixup_get_field(void** param, int param_no)
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

static int fixup_get_field_free(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2) {
		LM_WARN("free function has not been defined for spve\n");
		return 0;
	}

	if (param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}
