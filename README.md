# NSQ Module for Kamailio

## 1. Overview

NSQ is a realtime distributed messaging platform designed to operate at scale, handling billions of messages per day.
It promotes distributed and decentralized topologies without single points of failure, enabling fault tolerance and high availability coupled with a reliable message delivery guarantee.

From a high-level, the purpose of the module might be for things like:

* Integrate to an application to make real-time routing decisions (instead of using, say, a SQL database)
* Provide a real-time integration into your program, instead of your database, so you can overlay additional logic in your preferred language while also utilizing a message bus
* Utilize messaging to have a distributed messaging layer, such that machines processing requests/responses/events can go up/down or share the workload and your Kamailio node will still be happy

Supported operations are:

* publish json payloads to nsq topics
* publish json payloads to nsq topics and wait for correlated response message
* subscribe to an nsq topic and channel and handle events from that channel

The NSQ module also has support to publish updates to presence module thru the nsq_pua_publish function.

This module is heavily based on the Kazoo module from 2600hz.

## 2. How it works

The module works with a main forked process that does the communication with your nsq system for issuing publishes, waiting for replies, and consuming messages. When it consumes a message it defers the process to a worker thread so it doesn't block the main process (uses libev).

### 2.1. Event Routes

The worker process issues an event-route where we can act on the received payload. The name of the event-route is composed by values extracted from the payload.

NSQ module will try to execute the event route from most significant to less significant. define the event route like event_route[nsq:consumer-event[-payload_key_value[-payload_subkey_value]]]

#### Example
```
...
modparam("nsq", "consumer_event_key", "Event-Type")
modparam("nsq", "consumer_event_subkey", "Event-Name")
...

event_route[nsq:consumer-event-presence-update]
{
# presence is the value extracted from Event-Category field in json payload 
# update is the value extracted from Event-Name field in json payload 
xlog("L_INFO", "received $(kzE{kz.json,Event-Package}) update for $(kzE{kz.json,From})");
...
}

event_route[nsq:consumer-event-presence]
{
# presence is the value extracted from Event-Category field in json payload 
xlog("L_INFO", "received $(kzE{kz.json,Event-Package}) update for $(kzE{kz.json,From})");
...
}

event_route[nsq:consumer-event]
{
# this event route is executed if we can't find the previous 
}

```

### 2.2 Acknowledge Messages

Consumed messages have the option of being acknowledged in two ways:

* immediately when received
* after processing by the worker


## 3. Dependencies

### 3.1. Kamailio Modules

The following modules must be loaded before this module:

* none

### 3.2. External Libraries or Applications

* libev
* libjson
* libuuid

## 4. Parameters

### 4.1. NSQ Client

#### 4.1.1. nsqd_address(str)

The http address of the nsqd to post messages to

Default value is Null. You must set this parameter value for the module to work

__Example__
```
...
modparam("nsq", "nsqd_address", "127.0.0.1:4151")
...
```

#### 4.1.2. lookupd_address(str)

The http address of the nsq lookupd servers ( _comma seperated_ )

Default value is Null. You must set this parameter value for the module to work

__Example__
```
...
modparam("nsq", "lookupd_address", "10.10.10.1:4161,10.10.10.2:4161")
...
```

#### 4.1.3. max_in_flight(int)

Number of messages the nsq client will handle concurrently

Default value is 5.

__Example__
```
...
modparam("nsq", "max_in_flight", 5)
...
```

#### 4.1.4. listen_topic(str)

The topic to listen on for inbound events

__Example__
```
...
modparam("nsq", "lookupd", "10.10.10.1:4161,10.10.10.2:4161")
...
```

#### 4.1.4. listen_channel(str)

The channel to listen on for inbound events

## 5. Functions

## 6. Exported pseudo-variables

## 7. Transformations





