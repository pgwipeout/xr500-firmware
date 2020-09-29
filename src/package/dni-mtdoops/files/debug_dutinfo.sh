#!/bin/sh

CPU_INFO=/tmp/debug_cpu
MEM_INFO=/tmp/debug_mem
FLASH_INFO=/tmp/debug_flash
SESSION_INFO=/tmp/debug_session

cpu_usage()
{
	
	eval $(awk '/cpu / {t1=$2+$3+$4+$5+$6+$7+$8;i1=$5}END{printf("t1=%d i1=%d",t1,i1)}' /proc/stat)
	sleep 1
	eval $(awk '/cpu / {t2=$2+$3+$4+$5+$6+$7+$8;i2=$5}END{printf("t2=%d i2=%d",t2,i2)}' /proc/stat)
	
	echo $((i2-i1)) $((t2-t1)) | awk '{printf("%f%%",100-$1/$2*100)}' > $CPU_INFO
}

mem_usage()
{
	awk '/^MemTotal:/ {t=$2};/^MemFree:/ {f=$2};/^Buffers:/ {b=$2};/^Cached:/ {c=$2};
		END{tb=t/1024;ub=((t-f-b-c)/1024);printf("%dMB/%dMB", ub, tb)}' /proc/meminfo >$MEM_INFO
}

session_usage()
{
	used_session=`cat /proc/sys/net/ipv4/netfilter/ip_conntrack_count`
	total_session=`cat /proc/sys/net/ipv4/netfilter/ip_conntrack_max`
	
	echo "${used_session}/${total_session}" > $SESSION_INFO
}

flash_usage()
{
	# XR500 use NAND flash
	ts=$(awk -F'+' '{print $3}' /hw_id)
	df -mh /overlay | awk -v ts=${ts:-256} '/overlay/ {fs=$4};
		END { us=ts-fs; printf("%dMB/%dMB", us, ts) }' >$FLASH_INFO
}

check_usb_storage_folder()
{
	mount | grep -E '/tmp/mnt/sd[a-z][1-9]' \
		&& echo 1 > /tmp/debug-usb \
		|| echo 0 > /tmp/debug-usb
}

cpu_usage
mem_usage
session_usage
flash_usage
check_usb_storage_folder
