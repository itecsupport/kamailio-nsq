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

#ifndef _NSQ_FUNCS_H_
#define _NSQ_FUNCS_H_

#include "../../parser/msg_parser.h"
#include "../../lib/kcore/faked_msg.h"

int nsq_query(struct sip_msg* msg, char* topic, char* payload, char* dst);
int nsq_publish(struct sip_msg* msg, char* topic, char* payload);
void nsq_consumer_proc(int child_no);
int nsq_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

#endif
