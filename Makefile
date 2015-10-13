# $Id: $
#
# NSQ
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=nsq.so
JLIB=json
LIBS=-lnsq -lev -levbuffsock -lcurl -ljson-c
DEFS+=-I$(LOCALBASE)/include -I/usr/local/include

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules
