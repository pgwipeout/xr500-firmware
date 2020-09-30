#!/bin/sh

REALM=`/bin/cat /module_name | sed 's/\n//g'`
UHTTPD_BIN="/usr/sbin/uhttpd"
PX5G_BIN="/usr/sbin/px5g"


uhttpd_stop()
{
	kill -9 $(pidof uhttpd)
	
	# Wait till we know all uhttpd processes are killed
	while kill -0 $(pidof uhttpd); do
		sleep 1
	done
}

uhttpd_start()
{
        $UHTTPD_BIN -D -I ndindex.html -h /www -r ${REALM} -x /cgi-bin -l /apps -L /www/cgi-bin/url-routing.lua -t 40  -C /etc/uhttpd.crt -K /etc/uhttpd.key -p 0.0.0.0:80 -s 0.0.0.0:443
}

case "$1" in
	stop)
		uhttpd_stop
	;;
	start)
		uhttpd_start
	;;
	restart)
		uhttpd_stop
		uhttpd_start
	;;
	*)
		logger -- "usage: $0 start|stop|restart"
	;;
esac

