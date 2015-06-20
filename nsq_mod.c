

#include "nsq_mod.h"
#include "nsq.h"
#include "nsq.c"

MODULE_VERSION


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

