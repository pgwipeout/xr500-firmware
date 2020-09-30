#!/bin/sh

CONFIG=/bin/config
model_name="$(/sbin/artmtd -r board_model_id | cut -f 2 -d ":")"
[ "$model_name" != "XR500" -a "$model_name" != "XR450" ] && exit 1

# transfer XR500 or XR450 to lowercase
model_name_lc="$(echo $model_name |awk '{print tolower($0)}')"

echo "$model_name" > /tmp/module_name
echo "$model_name" > /tmp/hardware_version

for name in netbiosname Device_name wan_hostname upnp_serverName bridge_netbiosname ap_netbiosname; do
		# if it is not XR500 nor XR450, it is definitely be changed by user,
	    # so we will skip changing, keep it in the changed state
		if [ "x$($CONFIG get $name)" != "xXR500" -a "x$($CONFIG get $name)" != "xXR450" ]; then
				continue
		fi
				
		# handle the corner case that an XR500 device is renamed to XR450, if 
		# it is modified after first boot, we will let user rename it to XR450
		# also handle corner case that an XR450 device is renamed to XR500
		if [ "x$($CONFIG get board_region_default)" != "x1" ]; then
				continue
		fi

		$CONFIG set $name="$model_name"
done

# minidlna modelname
$CONFIG set minidlna_modelname="Windows Media Connect compatible (NETGEAR $model_name)"

# miniupnpd configure
$CONFIG set miniupnp_friendlyname="NETGEAR $model_name Wireless Router"
$CONFIG set miniupnp_modelname="$model_name Nighthawk® Pro Gaming WiFi"
$CONFIG set miniupnp_modelnumber="$model_name"
$CONFIG set miniupnp_modelurl="http://www.netgear.com/npg/"

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
