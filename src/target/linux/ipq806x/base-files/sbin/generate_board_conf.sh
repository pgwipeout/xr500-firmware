#!/bin/sh

CONFIG=/bin/config
model_name="$(/sbin/artmtd -r board_model_id | cut -f 2 -d ":")"
[ "$model_name" != "XR500" -a "$model_name" != "XR450" ] && exit 1

# transfer XR500 or XR450 to lowercase
model_name_lc="$(echo $model_name |awk '{print tolower($0)}')"

echo "$model_name" > /tmp/module_name
echo "$model_name" > /tmp/hardware_version

$CONFIG set netbiosname="$model_name"
$CONFIG set Device_name="$model_name"
$CONFIG set wan_hostname="$model_name"
$CONFIG set upnp_serverName="$model_name"
$CONFIG set bridge_netbiosname="$model_name"
$CONFIG set ap_netbiosname="$model_name"

# minidlna modelname
$CONFIG set minidlna_modelname="Windows Media Connect compatible (NETGEAR $model_name)"

# miniupnpd configure
$CONFIG set miniupnp_friendlyname="NETGEAR $model_name Wireless Router"
$CONFIG set miniupnp_modelname="$model_name Nighthawk® Pro Gaming WiFi"
$CONFIG set miniupnp_modelnumber="$model_name"
$CONFIG set miniupnp_modelurl="http://www.netgear.com/home/products/wirelessrouters"

if [ "$model_name" != "XR500" ]; then
		$CONFIG set miniupnp_pnpx_hwid="VEN_01f2&amp;DEV_0037&amp;REV_01 VEN_01f2&amp;DEV_8000&amp;SUBSYS_01&amp;REV_01 VEN_01f2&amp;DEV_8000&amp;REV_01"
else
		$CONFIG set miniupnp_pnpx_hwid="VEN_01f2&amp;DEV_0035&amp;REV_01 VEN_01f2&amp;DEV_8000&amp;SUBSYS_01&amp;REV_01 VEN_01f2&amp;DEV_8000&amp;REV_01"
fi

$CONFIG set miniupnp_devupc="606449084528"
$CONFIG commit

# diff in /etc/config/system
sed -i "s/sysfs 'xr...:/sysfs '$model_name_lc:/g" /etc/config/system

# diff in package/zebra
sed -i "s/hostname.*/hostname $model_name_lc/g" /etc/zebra.conf
sed -i "s/hostname.*/hostname $model_name_lc/g" /etc/ripngd.conf

# diff in package/zz-dni-streamboost
sed -i "s/BOARD_MODEL=.*/BOARD_MODEL=\"${model_name_lc}\"/g" /etc/appflow/streamboost.sys.conf

# diff in package/avahi
sed -i "s/host-name=.*/host-name=${model_name}/g" /etc/avahi/avahi-daemon.conf

# diff in package/samba-scripts
sed -i "s/server string.*/server string = NETGEAR ${model_name}/g" /etc/samba/smb.conf
sync
