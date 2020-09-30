#! /bin/sh
# ====================================变量定义====================================
# $1: x.x.x $2: x.x.x (return true if $1 >= $2)
# v1 cu  v2 new
# 0--> have new version
check_new_version(){
		
	for n in 1 2 3
	do
		checkv1=$(echo $1 |awk -F. '{print $'$n'}')
		checkv2=$(echo $2 |awk -F. '{print $'$n'}')
		[ -z "$checkv2" ] && return 1 
		[ -z "$checkv1" ] && return 1

		if [ "$n" == "1" ]; then
			[ "$checkv2" -gt "$checkv1" ] && return 0
			[ "$checkv2" -lt "$checkv1" ] && return 1
		fi

		if [ "$n" == "2" ]; then
			[ "$checkv2" -gt "$checkv1" ] && return 0
			[ "$checkv2" -lt "$checkv1" ] && return 1
		fi

		if [ "$n" == "3" ]; then

			if [ "$checkv2" -gt "$checkv1" ]; then
				return 0
			else
				return 1
			fi
		fi
	done 
	
	return 1
}
start_update_app(){

	update_flag=`curl https://asus-plugin.funjsq.com/netgear/v2/official/r7800/update_flag.php -l -k -s`

	[ "x$update_flag" == "x" ] && return

	if [ "x$update_flag" == "x1" ]; then
#		curl -s -l  -k https://static.funjsq.com/web_control/ipsetRule/funjsq100.conf -o /data/funjsq/config/dnsmasq.d/funjsq100.conf
#		curl -s -l -k https://static.funjsq.com/web_control/ipsetRule/funjsq101.conf -o /data/funjsq/config/dnsmasq.d/funjsq101.conf
#		curl -s -l  -k https://static.funjsq.com/web_control/ipsetRule/blocklistDL.conf -o /data/funjsq/config/dnsmasq.d/blocklistDL.conf
#                curl -s -l -k https://static.funjsq.com/web_control/ipsetRule/blocklistGW.conf -o /data/funjsq/config/dnsmasq.d/blocklistGW.conf
		curl -s -k https://static.funjsq.com/web_control/plugin_config/v2/data/v2_config_dns.tar.gz -o /tmp/plugin_v2_config.tar.gz
	
		tar -zxvf /tmp/plugin_v2_config.tar.gz -C /		
		DownloadConfig=`echo $?`

		if [ "x$DownloadConfig" != "x0" ]; then  
			rm -rf /tmp/plugin_v2_config.tar.gz
		else
			rm -rf /tmp/plugin_v2_config.tar.gz	
		fi

	fi

	if [ "x$update_flag" == "x2" ]; then
		curl -s -l  -k https://static.funjsq.com/web_control/netgear/funjsq_cli -o /data/funjsq/bin/funjsq_cli
		curl -s -l -k https://static.funjsq.com/web_control/netgear/funjsq_ctl -o /data/funjsq/bin/funjsq_ctl
		chmod 777 /data/funjsq/bin/funjsq_ctl
		chmod 777 /data/funjsq/bin/funjsq_cli
	fi

	local cu_version=`cat /data/funjsq/config/values/funjsq_version`
	local update_version=`curl https://asus-plugin.funjsq.com/netgear/v2/official/r7800/funjsq_version.php -l -k -s`
	
	[ "x$cu_version" == "x" ] && return
	[ "x$update_version" == "x" ] && return

	update_version=`echo $update_version | cut -d '#' -f 1`
#	cu_version=`echo $cu_version | tr -d '.'`
#	update_version_new=`echo $update_version | tr -d '.'`
	nvram set funjsq_update_version=$update_version

	echo "$update_version" >  /data/funjsq/config/values/funjsq_update_version

	check_new_version $cu_version $update_version && {

		curl -s -k https://static.funjsq.com/web_control/netgear/v2/funjsq_plugin_netgear_v2.tar.gz -o /tmp/funjsq_plugin.tar.gz
	
		tar -zxvf /tmp/funjsq_plugin.tar.gz -C /
		DownloadFlag=`echo $?`
		
		rm -rf /tmp/funjsq_plugin.tar.gz

		[ "x$DownloadFlag" != "x0" ] && { 
		#	echo 0 >  /data/funjsq/config/values/funjsq_status
			return 
		}

#		killall funjsq_ctl 
#		killall funjsq_time.sh
#		killall funjsq_cli
#		killall tail
#		killall funjsq_conntime


		/data/funjsq/bin/funjsq_ctl update 
		echo "$update_version" >  /data/funjsq/config/values/funjsq_version
		echo "$update_version" >  /data/funjsq/config/values/funjsq_update_version
		nvram set funjsq_version="$update_version"
		nvram set funjsq_update_version="$update_version"


#		echo 9 >  /data/funjsq/config/values/funjsq_status
#		nvram set funjsq_status=9

	}
	
}

start_funjsq(){
#	stop_funjsq

	[ "x$1" == "x" ] && return
	[ "x$2" == "x" ] && return
	[ "x$3" == "x" ] && return

	NTP_time=`date -u | awk -F 'UTC' '{print $2}' | sed 's/ //g'`
	[ $NTP_time -lt 2017 ] && {
		#月日时分年，设定时间必须大于安装包生成时的时间
		date -s 111512122018
	}

	accMAC="$1"
	accType="$2"
	accArea="$3"
	board_name=`cat /module_name`

	[ "$board_name" == "XR500" ] && {
		json1=`ubus call com.netdumasoftware.autoadmin reserve '{ "field" : "pmark", "subfield" : "funjsq1", "bits" : 1 }'`
		mark1=`echo $json1 | awk -F 'mask' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' `
		[ "x$mark1" != "x" ] &&  {
			echo $mark1 > /data/funjsq/config/values/mark1
		}

		json2=`ubus call com.netdumasoftware.autoadmin reserve '{ "field" : "pmark", "subfield" : "funjsq2", "bits" : 1 }'`
		mark2=`echo $json2 | awk -F 'mask' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' `
		[ "x$mark2" != "x" ] &&  {
			echo $mark2 > /data/funjsq/config/values/mark2
		}

		json3=`ubus call com.netdumasoftware.autoadmin reserve '{ "field" : "pmark", "subfield" : "funjsq3", "bits" : 1 }'`
		mark3=`echo $json3 | awk -F 'mask' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' `
		[ "x$mark3" != "x" ] &&  {
			echo $mark3 > /data/funjsq/config/values/mark3
		}

		json4=`ubus call com.netdumasoftware.autoadmin reserve '{ "field" : "pmark", "subfield" : "funjsq4", "bits" : 1 }'`
		mark4=`echo $json4 | awk -F 'mask' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' `
		[ "x$mark4" != "x" ] &&  {
			echo $mark4 > /data/funjsq/config/values/mark4
		}

		json5=`ubus call com.netdumasoftware.autoadmin reserve '{ "field" : "pmark", "subfield" : "funjsq5", "bits" : 1 }'`
		mark5=`echo $json5 | awk -F 'mask' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' `
		[ "x$mark5" != "x" ] &&  {
			echo $mark5 > /data/funjsq/config/values/mark5
		}

		json100=`ubus call com.netdumasoftware.autoadmin reserve '{ "field" : "pmark", "subfield" : "funjsq100", "bits" : 1 }'`
		mark100=`echo $json100 | awk -F 'mask' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' `
		[ "x$mark100" != "x" ] &&  {
			echo $mark100 > /data/funjsq/config/values/mark100
		}

		json101=`ubus call com.netdumasoftware.autoadmin reserve '{ "field" : "pmark", "subfield" : "funjsq101", "bits" : 1 }'`
		mark101=`echo $json101 | awk -F 'mask' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' `
		[ "x$mark101" != "x" ] &&  {
			echo $mark101 > /data/funjsq/config/values/mark101

		}

		lable1=`ubus call com.netdumasoftware.autoadmin acquire_rtable '{ "label" : "funjsq_rtable1" }'`
		table_id1=`echo $lable1 | awk -F 'result' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' | sed  's/}//g'`
		[ "x$table_id1" != "x" ] &&  {
			echo $table_id1 > /data/funjsq/config/values/rtable_id1
		}

		lable2=`ubus call com.netdumasoftware.autoadmin acquire_rtable '{ "label" : "funjsq_rtable2" }'`
		table_id2=`echo $lable2 | awk -F 'result' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' | sed  's/}//g'`
		[ "x$table_id2" != "x" ] &&  {
			echo $table_id2 > /data/funjsq/config/values/rtable_id2
		}

		lable3=`ubus call com.netdumasoftware.autoadmin acquire_rtable '{ "label" : "funjsq_rtable3" }'`
		table_id3=`echo $lable3 | awk -F 'result' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' | sed  's/}//g'`
		[ "x$table_id3" != "x" ] &&  {
			echo $table_id3 > /data/funjsq/config/values/rtable_id3
		}

		lable4=`ubus call com.netdumasoftware.autoadmin acquire_rtable '{ "label" : "funjsq_rtable4" }'`
		table_id4=`echo $lable4 | awk -F 'result' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' | sed  's/}//g'`
		[ "x$table_id4" != "x" ] &&  {
			echo $table_id4 > /data/funjsq/config/values/rtable_id4
		}


		lable5=`ubus call com.netdumasoftware.autoadmin acquire_rtable '{ "label" : "funjsq_rtable5" }'`
		table_id5=`echo $lable5 | awk -F 'result' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' | sed  's/}//g'`
		[ "x$table_id5" != "x" ] &&  {
			echo $table_id5 > /data/funjsq/config/values/rtable_id5
		}


	}


	echo 7 >  /data/funjsq/config/values/funjsq_status
	nvram set funjsq_status=7
	echo 0 >  /data/funjsq/config/values/funjsq_ctl_restart_flag
	nvram set funjsq_ctl_restart_flag=0

	echo $1 > /data/funjsq/config/values/funjsq_accType
	echo $2 > /data/funjsq/config/values/funjsq_accArea
	nvram set funjsq_accType="$1"
	nvram set funjsq_accArea="$2"




	start_update_app &

	/data/funjsq/bin/funjsq_ctl start $accMAC  $accType $accArea >/dev/null 2>&1 &
	
}

stop_funjsq(){

	accMAC="$1"
	/data/funjsq/bin/funjsq_ctl stop $accMAC >/dev/null 2>&1
	nvram  commit &
}

GetStatus_funjsq(){

	/data/funjsq/bin/funjsq_ctl GetStatus > /www/mul_device.aspx 
	cat /www/mul_device.aspx
	nvram  commit &
}

install_funjsq(){
	
	killall funjsq_ctl >/dev/null 2>&1
	killall funjsq_cli >/dev/null 2>&1
	killall funjsq_time.sh >/dev/null 2>&1
	
	/data/funjsq/bin/funjsq_ctl unbind >/dev/null 2>&1
	/data/funjsq/bin/funjsq_ctl install >/dev/null &
	nvram  commit &

}

uninstall_funjsq(){
	unbind_funjsq 
	/data/funjsq/bin/funjsq_ctl logout >/dev/null 2>&1
	nvram  commit &
}

unbind_funjsq(){
	start_update_app init &
	stop_funjsq
	/data/funjsq/bin/funjsq_ctl unbind >/dev/null 2>&1
	killall funjsq_ctl >/dev/null 2>&1
	nvram  commit &
}

Reg_funjsq(){
	Jsondata=`/data/funjsq/bin/funjsq_ctl FunjsqReg $1 $2 $3`
	if [ "x$Jsondata" != "x" ]; then
		echo $Jsondata > /tmp/FunjsqReg
	fi
	code=`echo $Jsondata | sed 's/.$//' | awk -F 'code' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	ExpireTime=`echo $Jsondata | sed 's/.$//' | awk -F 'ExpireTime' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	password=`echo $Jsondata | sed 's/.$//' | awk -F 'EncryptPassword' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	AccNum=`echo $Jsondata | sed 's/.$//' | awk -F 'AccNum' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`

	echo $code 
	echo $ExpireTime
	
}

SMS_funjsq(){
	
	Jsondata=`/data/funjsq/bin/funjsq_ctl FunjsqSMS $1 $2`
	if [ "x$Jsondata" != "x" ]; then
		echo $Jsondata > /tmp/FunjsqSMS
	fi
	code=`echo $Jsondata | sed 's/.$//' | awk -F 'code' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	echo $code
	nvram  commit &

}

Login_funjsq(){
	Jsondata=`/data/funjsq/bin/funjsq_ctl FunjsqLogin $1 $2`
	if [ "x$Jsondata" != "x" ]; then
		echo $Jsondata > /tmp/FunjsqLogin
	fi
	code=`echo $Jsondata | sed 's/.$//' | awk -F 'code' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	ExpireTime=`echo $Jsondata | sed 's/.$//' | awk -F 'ExpireTime' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	password=`echo $Jsondata | sed 's/.$//' | awk -F 'EncryptPassword' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	AccNum=`echo $Jsondata | sed 's/.$//' | awk -F 'AccNum' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	echo $code
	echo $ExpireTime
}

Load_Info_funjsq(){
	start_update_app init &

	Jsondata=`/data/funjsq/bin/funjsq_ctl LoadUserStatus`
	if [ "x$Jsondata" == "x" ]; then
		return 
	fi
	code=`echo $Jsondata | sed 's/.$//' | awk -F 'code' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	ExpireTime=`echo $Jsondata | sed 's/.$//' | awk -F 'ExpireTime' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	AccNum=`echo $Jsondata | sed 's/.$//' | awk -F 'AccNum' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	echo $code
	echo $ExpireTime

}

LogOut_funjsq(){
	uninstall_funjsq
	nvram  commit &
}

CPW_funjsq(){
	Jsondata=`/data/funjsq/bin/funjsq_ctl FunjsqCPW $1 $2 $3`
	if [ "x$Jsondata" != "x" ]; then
		echo $Jsondata > /tmp/FunjsqCPW
	fi
	code=`echo $Jsondata | sed 's/.$//' | awk -F 'code' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	echo $code 
}

CheckPay_funjsq(){
	Jsondata=`/data/funjsq/bin/funjsq_ctl FunjsqCheckPay `
	if [ "x$Jsondata" != "x" ]; then
		echo $Jsondata > /tmp/FunjsqCheckPay
	fi
	code=`echo $Jsondata | sed 's/.$//' | awk -F 'code' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	ExpireTime=`echo $Jsondata | sed 's/.$//' | awk -F 'ExpireTime' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	echo $code

}

init_funjsq(){
#	sleep 40
	#读取旧的配置信息
	funjsq_login=`nvram get funjsq_no_need_login`

	old_version=`nvram get funjsq_version`
	mkdir -p /data/funjsq/config/values
	echo "$old_version" > /data/funjsq/config/values/funjsq_version

	start_update_app init &

	[ "x$funjsq_login" != "x1" ]  && {
		killall funjsq_detect
		rm -rf /data/funjsq/config/values/*
		/data/funjsq/bin/funjsq_detect -i br0 -d
	}
	
	/data/funjsq/bin/funjsq_ctl init 
}

echo "$1" "$2" "$3" "$4" >> /tmp/funjsq_api_debug 

ACTION=$1
case $ACTION in
init)
	init_funjsq
	;;
start)
	start_funjsq $2 $3 $4
	;;
restart)
	start_funjsq 
	;;
stop)
	stop_funjsq $2 
	;;
GetStatus)
	GetStatus_funjsq  
	;;
install)
	install_funjsq  
	;;
uninstall)
	stop_funjsq 
	uninstall_funjsq 
	;;
unbind)
	unbind_funjsq 
	;;
reset)
	unbind_funjsq 
	;;
FunjsqReg)
	rm -rf /tmp/FunjsqReg
	Reg_funjsq $2 $3 $4 
	;;
FunjsqSMS)
	rm -rf /tmp/FunjsqSMS
	SMS_funjsq $2 $3
	;;
FunjsqLogin)
	rm -rf /tmp/FunjsqLogin
	Login_funjsq $2 $3
	;;
LoadUserStatus)
	Load_Info_funjsq
	;;
FunjsqLogOut)
	LogOut_funjsq 
	;;
FunjsqCPW)
	rm -rf /tmp/FunjsqCPW
	CPW_funjsq $2 $3 $4 
	;;
FunjsqCheckPay)
	rm -rf /tmp/FunjsqCheckPay
	CheckPay_funjsq
	;;
esac

