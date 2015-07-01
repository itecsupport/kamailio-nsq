#ifndef _NSQ_FUNCS_H_
#define _NSQ_FUNCS_H_

#include "../../parser/msg_parser.h"
#include "../../lib/kcore/faked_msg.h"

int nsq_query(struct sip_msg* msg, char* topic, char* payload, char* dst);
int nsq_publish(struct sip_msg* msg, char* topic, char* payload);
void nsq_consumer_proc(int child_no);
int nsq_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int nsq_encode(struct sip_msg* msg, char* unencoded, char* encoded);
int fixup_nsq_encode(void** param, int param_no);
int fixup_nsq_encode_free(void** param, int param_no);

#endif
