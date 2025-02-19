#! /bin/bash

# use this script in modules/ftm/test

# Monitoring output
echo -n 'dev/wbm0 datamaster host time: '; ssh root@fel0101.acc.gsi.de 'date -Iseconds'
echo -n 'dev/wbm0 datamaster IP: '; ssh root@fel0101.acc.gsi.de 'eb-mon -i dev/wbm0'
echo -n 'dev/wbm0 datamaster WR sync status: '; ssh root@fel0101.acc.gsi.de 'eb-mon -y dev/wbm0'
echo -n 'dev/wbm0 datamaster link status: '; ssh root@fel0101.acc.gsi.de 'eb-mon -l dev/wbm0'
# Test that the tools version and the firmware version are compatible
LD_LIBRARY_PATH=../lib ../bin/dm-cmd tcp/fel0101.acc.gsi.de
# Do  not run tests on this environment
# OPTIONS='-k zzz' make -C ~/bel_projects/dev/modules/ftm/tests remote
