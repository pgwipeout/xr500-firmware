function getNetgearInterfaceUrl() {
        var port = location.port ? ":" + location.port : "";
	return location.protocol + "//" + location.hostname + port + "/funjsq_login.htm";
}
	

(function (context) {

$("duma-panel", context).prop("loaded", true);

$("#funjsq").attr("src", getNetgearInterfaceUrl());
})(this);

//# sourceURL=device-table.js
