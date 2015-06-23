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

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../cfg/cfg_struct.h"

#include "nsq_funcs.h"

MODULE_VERSION

static int fixup_get_field(void** param, int param_no);
static int fixup_get_field_free(void** param, int param_no);

str lookupd_address = {0,0};
str consumer_topic = {0,0};
str nsqd_address = {0,0};


static int init(void)
{
	int total_workers = 1;

	register_procs(total_workers);
	cfg_register_child(total_workers);

    return 0;
}

/* module child initialization function */
static int child_init(int rank)
{
	int pid;

	if (rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0;

	if (rank==PROC_MAIN) {
		pid=fork_process(2, "NSQ Consumer", 1);
		LM_ERR("%s:%d, pid %d\n", __FUNCTION__, __LINE__, pid);
		if (pid<0)
			return -1; /* error */
		if(pid==0){
			nsq_consumer_proc(1);
		}
	}
	return 0;
}

/* Exported functions */
static cmd_export_t cmds[]={
		{"nsq_query", (cmd_function)nsq_query, 3, fixup_get_field, fixup_get_field_free, ANY_ROUTE},
		{"nsq_publish", (cmd_function)nsq_publish, 2, fixup_get_field, fixup_get_field_free, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]=
{
		{"lookupd_address", STR_PARAM, &lookupd_address.s},
		{"consumer_topic", STR_PARAM, &consumer_topic.s},
		{"nsqd_address", STR_PARAM, &nsqd_address.s},
		{ 0, 0, 0 }
};

struct module_exports exports = {
		"nsq",
		DEFAULT_DLFLAGS, 	/* dlopen flags */
		cmds,			 	/* Exported functions */
		params,		 		/* Exported parameters */
		0,		 			/* exported statistics */
		0,	             	/* exported MI functions */
		0,				 	/* exported pseudo-variables */
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
