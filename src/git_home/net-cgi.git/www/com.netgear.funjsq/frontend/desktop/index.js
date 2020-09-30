browserSetup.onReady(function () {
 $(document).ready(function () {

 var packageId = "com.netgear.funjsq";

 $("duma-panels")[0].add(
 "/apps/" + packageId + "/desktop/duma-funjsq.html",
 packageId, null, {
 width: 12, height: 12, x: 0, y: 0
 }
 );
 //$("#lalala").attr("src", getNetgearInterfaceUrl());

 });
});