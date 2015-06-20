#ifndef _NSQ_H
#define _NSQ_H

#include "../../sr_module.h"

static int nsq_query(struct sip_msg* msg, char* topic, char* payload, char* dst);

#endif /* _NSQ_H */
