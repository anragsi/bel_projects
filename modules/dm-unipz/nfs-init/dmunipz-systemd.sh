#!/bin/sh
# deployment script: systemd
#
# @history:
# autogenerated dmunipz-systemd.sh
# by dbeck @ asl752 on Fri Feb 10 03:12:04 PM CET 2023


# @generic head start
MOUNTPOINT=$1
INFO="$2 $0"
ARCH=$(/bin/uname -m)
HOSTNAME=$(/bin/hostname -s)

# info
logger "$INFO: start"
logger "$INFO: copying systemd unit configuration"

# @generic head end


# specific 
SERVICEA=dmunipz-logger.service
cp -a $MOUNTPOINT/systemd/$SERVICEA /lib/systemd/system
systemctl daemon-reload

systemctl start $SERVICEA


# @tail
# info
logger "$INFO: done"
