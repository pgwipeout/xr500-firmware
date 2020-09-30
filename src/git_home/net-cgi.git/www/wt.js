(function ($$) {
 function WTFast(implVersion,wtfAPIURL){if(implVersion<1.0){throw"Incompatble client version.";}
 if(wtfAPIURL==null){this.wtfAPIURL="https://rapi.wtfast.com";}else{this.wtfAPIURL=wtfAPIURL;}
 this.userData={};this.authenticated=false;this.devices={};this.gameIndex=[];}
 WTFast.prototype.LoadDeviceString=function(deviceString){this.RequireAuthenticated();this.devices={};var row=deviceString.split('<');for(var i=1;i<row.length;i++){var cols=row[i].split('>');gameID=parseFloat(cols[4]);this.UpdateDevice(cols[1].toLowerCase(),{"enabled":cols[0]==1,"node1":cols[2],"node2":cols[3],"gameID":gameID,"gameName":this.GameName(gameID)});}}
 WTFast.prototype.DumpDeviceString=function(){this.RequireAuthenticated();var deviceString="";$$.each(this.devices,function(macAddr,device){deviceString+="<";deviceString+=(device.enabled?"1":"0")+">";deviceString+=macAddr+">";deviceString+=device.node1+">";deviceString+=device.node2+">";deviceString+=device.gameID;});return deviceString;}
 WTFast.prototype.Login=function(username,password,callback){this.RequireUnauthenticated();$$.ajax({url:this.wtfAPIURL+"/user/login",jsonp:"callback",dataType:"jsonp",async:true,data:{username:username,password:password},success:$$.proxy(function(response){if(response.Success){this.userData=response;this.authenticated=true;this.userData.Server_List.sort();var gameIndex={};this.userData.Game_List.sort(function(x,y){return x.name.localeCompare(y.name);});this.userData.Game_List.forEach(function(game){gameIndex[parseFloat(game.id)]=game.name;});this.gameIndex=gameIndex;callback(true);}else{callback(false);}},this),error:function(){throw"Unable to reach WTFast API";}});}
 WTFast.prototype.Logout=function(callback){this.RequireAuthenticated();$$.ajax({url:this.wtfAPIURL+"/user/logout",jsonp:"callback",dataType:"jsonp",async:true,data:{session_hash:this.userData.Session_Hash},success:$$.proxy(function(response){this.userData={};this.authenticated=false;callback(response.Success);},this),error:function(){throw"Unable to reach WTFast API";}});}
 WTFast.prototype.UpdateDevice=function(mac,n){this.RequireAuthenticated();if(!this.ValidMac(mac)){throw"Invalid MAC address provided"};mac=mac.toLowerCase();var p={}
 if(this.devices.hasOwnProperty(mac)){p=this.devices[mac];}else{p={enabled:false,node1:"",node2:"",gameID:null,gameName:null}}
 if(n.hasOwnProperty('enabled'))p.enabled=n.enabled;if(n.hasOwnProperty('node1'))p.node1=n.node1;if(n.hasOwnProperty('node2'))p.node2=n.node2;if(n.hasOwnProperty('gameID')){gameID=parseFloat(n.gameID)
 p.gameID=gameID;p.gameName=this.GameName(gameID);}
 this.devices[mac]=p;}
 WTFast.prototype.GetDevice=function(mac){this.RequireAuthenticated();return this.devices[mac.toLowerCase()];}
 WTFast.prototype.RemoveDevice=function(mac){this.RequireAuthenticated();delete this.devices[mac.toLowerCase()];}
 WTFast.prototype.RequireUnauthenticated=function(){if(this.authenticated){throw"API already authenticated";}}
 WTFast.prototype.RequireAuthenticated=function(){if(!this.authenticated){throw"API not authenticated";}}
 WTFast.prototype.ValidMac=function(mac){var regex=/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$$/;return regex.test(mac);}
 WTFast.prototype.Authenticated=function(){return this.authenticated;}
 WTFast.prototype.Username=function(){this.RequireAuthenticated();return this.userData.eMail;}
 WTFast.prototype.AccountType=function(){this.RequireAuthenticated();return this.userData.Account_Type;}
 WTFast.prototype.SupportedGames=function(){this.RequireAuthenticated();return this.userData.Game_List;}
 WTFast.prototype.Nodes=function(){this.RequireAuthenticated();return this.userData.Server_List;}
 WTFast.prototype.MaxDevices=function(){this.RequireAuthenticated();return this.userData.Max_Computers;}
 WTFast.prototype.DaysRemaining=function(){this.RequireAuthenticated();return this.userData.Days_Left;}
 WTFast.prototype.Devices=function(){this.RequireAuthenticated();return this.devices;}
 WTFast.prototype.GameName=function(gameID){this.RequireAuthenticated();return this.gameIndex[parseFloat(gameID)];}
 WTFast.prototype.SessionHash=function(){this.RequireAuthenticated();return this.userData.Session_Hash;}

 var wtfast = {};
 $$(function() {
		 wtfast = new WTFast(1.0);
		 // 未登录状态1. Starting WTFast

		 $$("#login").click(function() {
			 info("Logging in...");
			 var username = $$("#username").val();
			 var password = $$("#password").val();
			 wtfast.Login(username, password, function(success) {
				 if (success)
				 {
				 info("Log in!");
				 var cf=document.forms[0];
				 cf.enable_wtfast.value=1;
				 cf.wtfast_login.value=1;
				 cf.wtfast_session_hash.value=wtfast.SessionHash();
				 cf.wtfast_max_clients.value=wtfast.MaxDevices();
				 cf.wtfast_days_left.value=wtfast.DaysRemaining();
				 cf.wtfast_account_type.value=wtfast.AccountType();
				 cf.wtfast_game_list.value=JSON.stringify(wtfast.SupportedGames());
				 cf.wtfast_server_list.value=wtfast.Nodes();
				 cf.submit();
				 }else {
				 info("Log in failed!");
				 }
				 });
		 });
		 //form=success
		 if($$('#successForm').length){
			 wtfast.Login(wt_email, wt_passwd, function(success) {
					 if (success)
					 {
					 $$("#wtfusername").html(wtfast.Username());
					 $$("#wtfaccounttype").html(wtfast.AccountType());
					 $$("#wtfmaxdevices").html(wtfast.MaxDevices());
					 $$("#wtfendeddate").html(wtfast.DaysRemaining());		
					 wtfast.LoadDeviceString(mockGetDevices());
					 populateDeviceList();

					 var node1Select = $$("#devicenode1");				
					 node1Select.append('<option value="">-</option>');
					 $$.each( wtfast.Nodes(), function(_, node ) {
						 node1Select.append('<option value="'+node+'">'+node+'</option>');
						 });
					 node1Select.children().clone().appendTo("#devicenode2");

					 var gameSelect = $$("#devicegameid");
					 $$.each( wtfast.SupportedGames(), function(_, game ) {
						 gameSelect.append('<option value="'+game.id+'">'+game.name+'</option>');
						 });

					 }
			 });
			 //when login disable
			 $$("#apply").click(function(){
					 var cf=document.forms[0];
					 if(cf.enable_wtfast_ch.checked==true){
					 mockCommitDevices();	
					 }else{
					 cf.submit_flag.value="apply_disable_wtfast";
					 cf.enable_wtfast.value="0";
					 cf.wtfast_login.value="0";
					 cf.action="/apply.cgi?/WTFAST.htm timestamp="+ts;
					 wtfast.Logout(function(success) {
						 if (success) {
						 info("Disabled!");
						 console.log("logout");
						 cf.submit();
						 } else {
						 alert("Disable failed");
						 }
						 });	
					 }					
					 });
			 //logout
			 $$("#logout").click(function() {
					 info("Logging out...");
					 wtfast.Logout(function(success) {
						 if (success) {

						 info("Logged out!");
						 mockStopWTFast();
						 } else {
						 info("Log out failed, or already logged out!");
						 }
						 });
					 });

			 $$(document).on("click","#newdevice",function() {
					 $$("#devicemac").val("");
					 $$("#devicemac").prop('disabled', false);
					 $$("#deviceenabled").prop('checked', true);	
					 $$("#devicenode1").val("");
					 $$("#devicenode2").val("");
					 $$("#devicegameid option:first").attr('selected','selected');
					 $$("#deviceauto").prop('checked', true);
					 $$("#devicenodeselectors").hide();
					 $$("#devicesave").show();
					 $$("#deviceupdate").hide();
					 showEditor();
					 });

			 $$(document).on("click",".editdevice",function() {
					 mac = $$(this).parent().data("mac");
					 device = wtfast.GetDevice(mac);	
					 $$("#devicemac").val(mac);
					 $$("#devicemac").prop('disabled', true);
					 $$("#deviceenabled").prop('checked', device.enabled);
					 $$("#devicenode1").val(device.node1);
					 $$("#devicenode2").val(device.node2);
					 $$("#devicegameid").val(device.gameID);
					 if (device.node1 == "") {
					 $$("#deviceauto").prop('checked', true);
					 $$("#devicenodeselectors").hide();
					 } else {
					 $$("#deviceauto").prop('checked', false);
					 $$("#devicenodeselectors").show();
					 }
					 $$("#devicesave").hide();
					 $$("#deviceupdate").show();
					 showEditor();
					 });

			 $$(document).on("click",".deletedevice",function() {
					 row = $$(this).parent();
					 mac = row.data("mac");
					 wtfast.RemoveDevice(mac);
					 row.remove();
					 var cf=document.forms[0];
					 cf.devicestring.value=wtfast.DumpDeviceString();
					++device_num;
					 });

			 $$("#devicesave").click(function() {
					 var mac = $$("#devicemac").val();
					 if (wtfast.GetDevice(mac)!=null) {
					 alert("That device already exists.");
					 return;
					 }
					 saveDevice(mac);
					 });

			 $$("#devicecancel").click(function() {
					 hideEditor();
					 });

			 $$("#deviceupdate").click(function() {
					 var mac = $$("#devicemac").val();
					 saveDevice(mac);
					 });

			 $$("#deviceauto").click(function() {
					 if ($$(this).prop('checked')) {
					 $$("#devicenodeselectors").hide();
					 $$("#devicenode1").val("");
					 $$("#devicenode2").val("");
					 } else {
					 $$("#devicenodeselectors").show();
					 }
					 });

			 $$("#devicenode1").change(function(){
					 if ($$(this).val() == $$("#devicenode2").val()) {
					 $$("#devicenode2").val("")
					 }
					 });

			 $$("#devicenode2").change(function(){
					 if ($$(this).val() == $$("#devicenode1").val()) {
					 $$("#devicenode1").val("")
					 }
					 });
		 }
 });
 function saveDevice(mac) {
	 if (!wtfast.ValidMac(mac)) {
		 alert("Invalid MAC address.");
		 $$("#devicemac").focus();
		 return;
	 }
	 wtfast.UpdateDevice(mac, {
			 "enabled":$$("#deviceenabled").prop('checked'),
			 "node1": $$("#devicenode1").val(),
			 "node2": $$("#devicenode2").val(),
			 "gameID": $$("#devicegameid").val()
			 });
	 populateDeviceList();
	 hideEditor();
	 var cf=document.forms[0];
	 cf.devicestring.value=wtfast.DumpDeviceString();
	++device_num;
 }

 function showEditor() {
	 $$("#disabler").show();
	 $$("#deviceeditor").center().show();
 }

 function hideEditor() {
	 $$("#deviceeditor").hide();
	 $$("#disabler").hide();
 }

 function populateDeviceList() {
	 $$("#wtfdevicelist").empty();
	 $$.each( wtfast.Devices(), function( macAddr, device ) {
			 modeString = (device.node1==""?"auto":"manual ("+device.node1+(device.node2 != ""?" &rarr; "+device.node2:"")+")");
			 row = $$('<tr></tr>');
			 row.data("mac",macAddr);
			 row.append('<td class="ruleenabled">'+device.enabled+'</td>');
			 row.append('<td class="ruledevice">'+macAddr+'</td>');
			 row.append('<td class="rulemode">'+modeString+'</td>');
			 row.append('<td class="rulegame">'+device.gameName+'</td>');
			 row.append('<input type="button" class="deletedevice" value="Delete">');
			 row.append('<input type="button" class="editdevice" value="Edit">');
			 $$("#wtfdevicelist").append(row);
			 });
 }

 function populateNodeSelect(target) {
	 target.append('<option value="">-</option>');
	 $$.each( wtfast.Nodes(), function(_, node ) {
			 target.append('<option value="'+node+'">'+node+'</option>');
			 });
 }

 function populateGameSelect(target) {
	 $$.each( wtfast.SupportedGames(), function(_, game ) {
			 target.append('<option value="'+game.id+'">'+game.name+'</option>');
			 });
 }

 function info(str) {
	 $$("#message").html(str);
 }

 jQuery.fn.center = function () {
	 this.css("position","absolute");
	 this.css("top", Math.max(0, (($$(window).height() - $$(this).outerHeight()) / 2) + $$(window).scrollTop()) + "px");
	 this.css("left", Math.max(0, (($$(window).width() - $$(this).outerWidth()) / 2) + $$(window).scrollLeft()) + "px");
	 return this;
 }

 function mockGetDevices() {
	 // this is where server-side code would be triggered via AJAX to read wtf_rulelist
	 // from NVRAM.	
	 console.log("mockGetDevices called");
	 return devicestr;
 }

 function mockCommitDevices() {
	 // this is where server-side code
	 // string to wtf_rulelist in NVRAM, and to send either a start or SIGHUP to wtfslhd.
	 console.log("mockCommitDevices called");
	 var cf=document.forms[0];
	if(device_num=="0")
		cf.devicestring.value=devicestr;
	 cf.wtfast_login.value="1";
	 cf.enable_wtfast.value=1;
	 cf.wtfast_login.value=1;	
	 cf.submit();
 }

 function mockStartWTFast() {
	 // this is where server-side code would be triggered to send either a start or 
	 // SIGHUP to wtfslhd.	
	 console.log("mockStartWTFast called");
 }

 function mockStopWTFast() {
	 // this is where server-side code would be triggered to stop wtfslhd.
	 var cf=document.forms[0];
	 cf.submit_flag.value="wtfast_logout";
	 cf.action="/apply.cgi?/WTFAST.htm timestamp="+ts;
	 cf.wtfast_login.value="0";
	 cf.enable_wtfast.value="0";
	 console.log("mockStopWTFast called");
	 cf.submit();
 }

}(jQuery));

