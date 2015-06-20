# $Id: $
#
# NSQ
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=nsq.so
LIBS=-lnsq -lev -levbuffsock -lcurl -ljson-c
DEFS+=-I$(LOCALBASE)/include

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules
