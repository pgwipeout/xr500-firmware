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

	update_flag=`curl https://asus-plugin.funjsq.com/netgear/update_flag.php -l -k -s`

	[ "x$update_flag" == "x" ] && return

	if [ "x$update_flag" == "x1" ]; then
#		curl -s -l  -k https://static.funjsq.com/web_control/ipsetRule/funjsq100.conf -o /data/funjsq/config/dnsmasq.d/funjsq100.conf
#		curl -s -l -k https://static.funjsq.com/web_control/ipsetRule/funjsq101.conf -o /data/funjsq/config/dnsmasq.d/funjsq101.conf
#		curl -s -l  -k https://static.funjsq.com/web_control/ipsetRule/blocklistDL.conf -o /data/funjsq/config/dnsmasq.d/blocklistDL.conf
#		curl -s -l -k https://static.funjsq.com/web_control/ipsetRule/blocklistGW.conf -o /data/funjsq/config/dnsmasq.d/blocklistGW.conf
		curl -s -k https://static.funjsq.com/web_control/plugin_config/data/plugin_v1_config.tar.gz -o /tmp/plugin_v1_config.tar.gz
		tar -zxvf /tmp/plugin_v1_config.tar.gz -C /    
		DownloadConfig=`echo $?`

		if [ "x$DownloadConfig" != "x0" ]; then  
			rm -rf /tmp/plugin_v1_config.tar.gz
		else
			rm -rf /tmp/plugin_v1_config.tar.gz    
		fi 
	fi

	if [ "x$update_flag" == "x2" ]; then
		curl -s -l  -k https://static.funjsq.com/web_control/netgear/funjsq_cli -o /data/funjsq/bin/funjsq_cli
		curl -s -l -k https://static.funjsq.com/web_control/netgear/funjsq_ctl -o /data/funjsq/bin/funjsq_ctl
		chmod 777 /data/funjsq/bin/funjsq_ctl
		chmod 777 /data/funjsq/bin/funjsq_cli
	fi

	local cu_version=`cat /data/funjsq/config/values/funjsq_version`
	local update_version=`curl https://asus-plugin.funjsq.com/netgear/funjsq_version.php -l -k -s`
	
	[ "x$cu_version" == "x" ] && return
	[ "x$update_version" == "x" ] && return

	update_version=`echo $update_version | cut -d '#' -f 1`
#	cu_version=`echo $cu_version | tr -d '.'`
#	update_version_new=`echo $update_version | tr -d '.'`
	nvram set funjsq_update_version=$update_version

	echo "$update_version" >  /data/funjsq/config/values/funjsq_update_version

	check_new_version $cu_version $update_version && {

		curl -s -k https://static.funjsq.com/web_control/netgear/funjsq_plugin_netgear.tar.gz -o /tmp/funjsq_plugin.tar.gz
	
		tar -zxvf /tmp/funjsq_plugin.tar.gz -C /
		DownloadFlag=`echo $?`
		rm -rf /tmp/funjsq_plugin.tar.gz

		[ "x$DownloadFlag" != "x0" ] && { 
			echo 0 >  /data/funjsq/config/values/funjsq_status
			return 
		}
	
		[ "$1" != "init" ] && {
			killall funjsq_ctl 
			killall funjsq_time.sh
			killall funjsq_cli
			killall tail
			killall funjsq_conntime
		}

#		tar -zxvf /tmp/funjsq_plugin.tar.gz -C /

		/data/funjsq/bin/funjsq_ctl update 
		echo "$update_version" >  /data/funjsq/config/values/funjsq_version
		echo "$update_version" >  /data/funjsq/config/values/funjsq_update_version
		nvram set funjsq_version="$update_version"
		nvram set funjsq_update_version="$update_version"

		[ "$1" != "init" ] && {
			echo 9 >  /data/funjsq/config/values/funjsq_status
			nvram set funjsq_status=9
		}

	}
	

#	uci  commit &
}

start_funjsq(){
	stop_funjsq

	[ "x$1" == "x" ] && return
	[ "x$2" == "x" ] && return

#	Old_funjsq_accType=`cat /data/funjsq/config/values/funjsq_accType`
#	[ "x$1" != "x$Old_funjsq_accType" ] && {
#		/data/funjsq/bin/funjsq_ctl unbind >/dev/null 2>&1
#		echo $1 > /data/funjsq/config/values/funjsq_accType
#		echo $2 > /data/funjsq/config/values/funjsq_accArea
#	}

	NTP_time=`date -u | awk -F 'UTC' '{print $2}' | sed 's/ //g'`
	[ $NTP_time -lt 2017 ] && {
		#月日时分年，设定时间必须大于安装包生成时的时间
		date -s 111512122018
	}

	board_name=`cat /module_name`

	[ "$board_name" == "XR500" ] && {
		json100=`ubus call com.netdumasoftware.autoadmin reserve '{ "field" : "pmark", "subfield" : "funjsq100", "bits" : 1 }'`
		
		mark100=`echo $json100 | awk -F 'mask' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' `

		[ "x$mark100" != "x" ] &&  {
			echo $mark100 > /data/funjsq/config/values/mark100
			lable100=`ubus call com.netdumasoftware.autoadmin acquire_rtable '{ "label" : "funjsq_rtable100" }'`
			table_id100=`echo $lable100 | awk -F 'result' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' | sed  's/}//g'`
			
			echo $table_id100 > /data/funjsq/config/values/rtable_id


		}

		json101=`ubus call com.netdumasoftware.autoadmin reserve '{ "field" : "pmark", "subfield" : "funjsq101", "bits" : 1 }'`
		
		mark101=`echo $json101 | awk -F 'mask' '{print $2}'  | cut -d ',' -f 1 | cut -d ':' -f 2| tr '\n' ' ' | sed  's/ //g' `

		[ "x$mark101" != "x" ] &&  {
			echo $mark101 > /data/funjsq/config/values/mark101
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
	
	[ -f /dev/net/tun ] && {
		
		tun_path=`find /lib/modules/ -name tun.ko`
		insmod $tun_path
        }

	/data/funjsq/bin/funjsq_ctl start >/dev/null 2>&1 &
	
}

stop_funjsq(){
	/data/funjsq/bin/funjsq_ctl stop >/dev/null 2>&1
	killall funjsq_ctl >/dev/null 2>&1
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
	/data/funjsq/bin/funjsq_ctl remove >/dev/null 2>&1
	nvram  commit &
}


unbind_funjsq(){
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
	echo $code 
	echo $ExpireTime
	nvram set funjsq_expire="$ExpireTime"
	nvram set funjsq_mobile="$1"
	nvram set funjsq_password="$password"
	nvram  commit &

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
	echo $code
	echo $ExpireTime
	[ "$code"  !=  "1000" ] && exit
	nvram set funjsq_expire="$ExpireTime"
	nvram set funjsq_mobile="$1"
	nvram set funjsq_password="$password"
	install_funjsq
	nvram  commit &
}

LogOut_funjsq(){
	unbind_funjsq
	nvram unset funjsq_expire
	nvram unset funjsq_mobile
	rm /data/funjsq/config/usrinfo
	nvram  commit &
}

CPW_funjsq(){
	Jsondata=`/data/funjsq/bin/funjsq_ctl FunjsqCPW $1 $2 $3`
	if [ "x$Jsondata" != "x" ]; then
		echo $Jsondata > /tmp/FunjsqCPW
	fi
	code=`echo $Jsondata | sed 's/.$//' | awk -F 'code' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	echo $code 
	nvram  commit &
}

CheckPay_funjsq(){
	start_update_app init &
	
	Jsondata=`/data/funjsq/bin/funjsq_ctl FunjsqCheckPay `
	if [ "x$Jsondata" != "x" ]; then
		echo $Jsondata > /tmp/FunjsqCheckPay
	fi
	code=`echo $Jsondata | sed 's/.$//' | awk -F 'code' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	ExpireTime=`echo $Jsondata | sed 's/.$//' | awk -F 'ExpireTime' '{print $2}' | cut -d ',' -f 1  | sed 's/"//g'|sed  's/^.//'`
	echo $code

	[ "x$ExpireTime" != "x" ] && nvram set funjsq_expire="$ExpireTime"
	[ "x$code" == "x1000" ] && {
		nvram set funjsq_expire="$ExpireTime"
		funjsq_status=`nvram get funjsq_status`
		[ "x$funjsq_status" == "x3" ] && {
			echo "0" > /data/funjsq/config/values/funjsq_status
			nvram set funjsq_status=0
		}
	}
	nvram commit &
}

init_funjsq(){

	#读取旧的配置信息
	mobile=`nvram get  funjsq_mobile`
	password=`nvram get funjsq_password`
	expire=`nvram get  funjsq_expire`
	accType=`nvram get funjsq_accType`
	accArea=`nvram get funjsq_accArea`
	ip=`nvram get funjsq_ip`
	mac=`nvram get funjsq_mac`
	RXbyte=`nvram get funjsq_RXbyte`
	TXbyte=`nvram get funjsq_TXbyte`
	update_time=`nvram get funjsq_update_time`
	old_version=`nvram get funjsq_version`
	old_status=`nvram get funjsq_status`
	QQSupport=`nvram get funjsq_QQSupport`
	funjsq_ps4_ip=`nvram get funjsq_ps4_ip`
	funjsq_win_ip=`nvram get funjsq_win_ip`
	funjsq_xbox_ip=`nvram get funjsq_xbox_ip`
	funjsq_ns_mac=`nvram get funjsq_ns_ip`
	funjsq_ps4_mac=`nvram get funjsq_ps4_mac`
	funjsq_win_mac=`nvram get funjsq_win_mac`
	funjsq_xbox_mac=`nvram get funjsq_xbox_mac`
	funjsq_ns_mac=`nvram get funjsq_ns_mac`
	board_name=`cat /module_name`

	funjsq_start_flag=`nvram get funjsq_start_flag`
	echo "$old_version" > /data/funjsq/config/values/funjsq_version

	start_update_app init

	new_status=`nvram get funjsq_status`

	[ "x$new_status" == "x9" ] && {
		old_version=`nvram get funjsq_version`	
		update_time=`nvram get funjsq_update_time`
		QQSupport=`nvram get funjsq_QQSupport`
		nvram set funjsq_status="$old_status"
		nvram set funjsq_ip="$ip"
		nvram set funjsq_mac="$mac"
		nvram set funjsq_accType="$accType"
		nvram set funjsq_accArea="$accArea"
		nvram set funjsq_ps4_ip="$funjsq_ps4_ip"
		nvram set funjsq_win_ip="$funjsq_win_ip"
		nvram set funjsq_xbox_ip="$funjsq_xbox_ip"
		nvram set funjsq_ns_ip="$funjsq_ns_ip"
		nvram set funjsq_ps4_mac="$funjsq_ps4_mac"
		nvram set funjsq_win_mac="$funjsq_win_mac"
		nvram set funjsq_xbox_mac="$funjsq_xbox_mac"
		nvram set funjsq_ns_mac="$funjsq_ns_mac"

	}
	
	echo  "$mobile" > /data/funjsq/config/usrinfo
	echo  "$password" >> /data/funjsq/config/usrinfo
	echo "$board_name" > /data/funjsq/config/values/board_name

	echo  "$mobile" > /data/funjsq/config/values/funjsq_mobile
	echo  "$accType" > /data/funjsq/config/values/funjsq_accType
	echo  "$accArea" > /data/funjsq/config/values/funjsq_accArea
	echo  "$expire" > /data/funjsq/config/values/funjsq_expire
	echo "$old_status" > /data/funjsq/config/values/funjsq_status
	echo "0.0 B" > /data/funjsq/config/values/funjsq_RXbyte
	echo "0.0 B" > /data/funjsq/config/values/funjsq_TXbyte
	echo "00:00:00" > /data/funjsq/config/values/funjsq_conntime
	echo "$old_version" > /data/funjsq/config/values/funjsq_version
	echo "$ip" > /data/funjsq/config/values/funjsq_ip
	echo "$mac" > /data/funjsq/config/values/funjsq_mac
	echo "$update_time" > /data/funjsq/config/values/funjsq_update_time
	echo "$QQSupport" > /data/funjsq/config/values/funjsq_QQSupport

	echo "$funjsq_ps4_ip" > /data/funjsq/config/values/funjsq_ps4_ip
	echo "$funjsq_win_ip" > /data/funjsq/config/values/funjsq_win_ip
	echo "$funjsq_xbox_ip" > /data/funjsq/config/values/funjsq_xbox_ip
	echo "$funjsq_ns_ip" > /data/funjsq/config/values/funjsq_ns_ip

	echo "$funjsq_ps4_mac" > /data/funjsq/config/values/funjsq_ps4_mac
	echo "$funjsq_win_mac" > /data/funjsq/config/values/funjsq_win_mac
	echo "$funjsq_xbox_mac" > /data/funjsq/config/values/funjsq_xbox_mac
	echo "$funjsq_ns_mac" > /data/funjsq/config/values/funjsq_ns_mac

	echo "$funjsq_start_flag" > /data/funjsq/config/values/funjsq_start_flag
	
	echo "0" > /data/funjsq/config/values/funjsq_windows_enable
	echo "0" > /data/funjsq/config/values/funjsq_enable_delayLoss
	echo "" > /data/funjsq/config/values/funjsq_loss1
	echo "" > /data/funjsq/config/values/funjsq_loss2
	echo "" > /data/funjsq/config/values/funjsq_delay1
	echo "" > /data/funjsq/config/values/funjsq_delay2

}



ACTION=$1
case $ACTION in
init)
	init_funjsq
	;;
start)
	start_funjsq $2 $3
	;;
restart)
	start_funjsq 
	;;
stop)
	stop_funjsq 
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

