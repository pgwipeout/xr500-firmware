#!/bin/sh

[ "$OVPN_CLIENT_TRACE" = "on" ] && set -x

. /etc/openvpn/openvpn_client.env

product="$(awk '{print tolower($0)}' /module_name)"
remote_location="https://http.fw.updates1.netgear.com/sw-apps/vpn-client/${product}"
id_file="updated_date.txt"

curl_opt="-s --retry 5"

init_local_dirs()
{
	mkdir -p ${ovpn_client_cfg_dir}/${ipvanish}
	mkdir -p ${ovpn_client_cfg_dir}/${hidemyass}
}

download_and_expand_configs()
{
	#rm -rf ${ovpn_client_cfg_dir}/${ipvanish}/*
	#curl ${curl_opt} ${remote_location}//configs.zip -o ${ovpn_client_cfg_dir}/${ipvanish}.zip \
	#	&& unzip -q ${ovpn_client_cfg_dir}/${ipvanish}.zip -d ${ovpn_client_cfg_dir}/${ipvanish}
	rm -rf ${ovpn_client_cfg_dir}/${hidemyass}/*
	curl ${curl_opt} ${remote_location}/vpn-configs.zip -o ${ovpn_client_cfg_dir}/${hidemyass}.zip \
		&& unzip -q ${ovpn_client_cfg_dir}/${hidemyass}.zip -d ${ovpn_client_cfg_dir}/${hidemyass}
}

init_local_dirs
while true; do
	curl ${curl_opt} ${remote_location}/${providerlist_file_name} -o ${ovpn_client_cfg_dir}/${providerlist_file_name}.new
	if [ ! -f ${ovpn_client_cfg_dir}/${providerlist_file_name} ] \
		|| ! diff ${ovpn_client_cfg_dir}/${providerlist_file_name}.new ${ovpn_client_cfg_dir}/${providerlist_file_name}; then
		download_and_expand_configs \
			&& mv ${ovpn_client_cfg_dir}/${providerlist_file_name}.new ${ovpn_client_cfg_dir}/${providerlist_file_name}
	fi
	[ $? -eq 0 ] && sleep 43200 || sleep 10
done
