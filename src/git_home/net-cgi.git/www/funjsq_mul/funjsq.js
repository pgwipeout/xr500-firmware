(function ($$) {

	"use strict";


	$$(function () {

		$$.PAGE_WAITING_DIV ="<div id=\"page_waiting\" class=\"running\"></div><div class=\"page_doing running\"> \
		<div class=\"loadingMessage roundCorners\">请等待一会...</div></div>"

		$$.submit_wait = function(container, string) {
			$$(container).append(string);
		};
	/**
	 * @function post form data.
	 * @param {container id or class} - the id or class of the container
	 * @param {url} - request url, if it's NULL or empty, it will auto get formId's action value.
	 * @param {function} - callback function
	 */
	$$.postForm = function (container, url, callback) {
		url = $$(container).attr("action");
		var funcs = function(json) {
			if ( callback != null )
		callback(json);
		};

		$$.ajax({
			url: url,
			type: "POST",
			data: $$(container).serialize(),
			dataType: 'json',
			contentType: "application/json; charset=utf-8",
			success: funcs
		});
	};

	$$.getData = function (url, callback, ajax_error) {
		var new_url = url;

		$$.ajax({
			url: new_url,
			success: function(json) {
				callback(json);
			},
			dataType: "json",
			error: function(){
				ajax_error();
			}
		});
	};

	/*******************************************************************************************
	 *
	 * Rewrite alert and confirm dialog
	 *
	 *******************************************************************************************/
	$$.alertBox = function(msg, str, act) {
		if ( !$$('#modalOverlay').length ) {
			$$('body').append("<div id='modalOverlay'></div>");
		}
		$$('#modalOverlay').addClass('active');
		$$('#modalOverlay').fadeIn('fast');

		var date = new Date();
		var id = "alert"+date.getTime();
		var close = function() {
			if ( $$('.modalBox:visible').length == 1 ){
				$$('#modalOverlay').fadeOut();
			}
			$$('#'+id).fadeOut();
			$$('#'+id).remove();
		},
		    action = close;

		if ( !str ) str = "我知道了";
		if ( typeof(act) == "function" ) {
			action = function() {
				act();
				close();
			}
		}

		var data = "<div id='"+id+"'class='modalBox'><div><img id='alertImage' src='/image/pay_issue.png'></div><div id='alertMsg'>温馨提示</div><div class='recommendation'><p>"
			+ msg + "</p><div class='centerButtons'><div class='boxButtons'><a class='alertClose' id='"+id+"Ok'>"
			+ str + "</a></div></div></div></div>";
		$$('body').append(data);
		$$('#'+id+'Ok').click(action);
		//fixed_modal();
	};
	$$.confirmBox = function(msg, str1, act1, str2, act2) {
		if ( !$$('#modalOverlay').length ) {
			$$('body').append("<div id='modalOverlay'></div>");
		}
		$$('#modalOverlay').addClass('active');
		$$('#modalOverlay').fadeIn('fast');

		var date = new Date();
		var id = "alert"+date.getTime();
		var close = function() {
			if ( $$('.modalBox:visible').length == 1 ){
				$$('#modalOverlay').fadeOut();
			}
			$$('#'+id).fadeOut();
			$$('#'+id).remove();
		},
		    action1 = close,
		    action2 = close;
		if ( !str1 ) str1 = ok_mark;
		if ( !str2 ) str2 = cancel_mark;
		if ( typeof(act1) == "function" ) {
			action1 = function() {
				act1();
				close();
			};
		}
		if ( typeof(act2) == "function" ) {
			action2 = function() {
				act2();
				close();
			};
		}


		var data = "<div id='"+id+"'class='modalBox'><div class='recommendation'><p>"
			+ msg + "</p><div class='centerButtons'><div class='boxButtons'><a class='btn primary close' id='"+id+"Cancel'>"
			+ str2 + "</a><a class='btn primary close' id='"+id+"Ok'>"
			+ str1 + "</a></div></div></div></div>";
		$$('body').append(data);
		$$('#'+id+'Ok').click(action1);
		$$('#'+id+'Cancel').click(action2);
		fixed_modal();
	};
	// regular expression
	$$.REG_PASSWORD = /^[0-9a-zA-Z]{6,32}$/;
	$$.REG_PASSWORD2 = /^(?![0-9]+$$)(?![a-zA-Z]+$$)[0-9A-Za-z]{6,32}$/;
	$$.REG_NUM = /^[0-9]{6}$/;
	$$.REG_PHONE = /^[1][0-9]{10}$/;

	if ( $$('#loginForm').length ) {
		var count = 0;
		if(funjsq_no_need_login == "1"){
			$$('#contain_back').css('display','none');
			$$('#service_question').css('display','none');
			$$('#formframe_wait_div').css('display','block');
			location.href="funjsq_select.htm";
		}
		if(forget_or_register == "1"){
			$$('#forgetBt').trigger('click');
		}else if(forget_or_register == "2"){
			$$('#registerBt').trigger('click');
		}

		$$('#loginBt').click(function() {
			var cf=document.forms[0];
			if ( !$$.REG_PHONE.test($$('#loginName').val()) || $$('#loginName').val().length!=11) {
				$$.alertBox("无效的账号！");
				return false;
			}else{
				cf.hidden_funjsq_username.value = cf.funjsq_username.value;
			}

			if ( !$$.REG_PASSWORD.test($$('#loginPwd').val()) ) {
				$$.alertBox("无效的密码！");
				return false;
			}else{
				cf.hidden_funjsq_password.value = cf.funjsq_password.value;
			}

			cf.action="/admin.cgi?/funjsq_login.htm timestamp="+ts;
			
			$$('#loginForm').css('display','none');
			$$('#contain_back').css('display','none');
			$$('#service_question').css('display','none');
			$$('#formframe_wait_div').css('display','block');

			$$.postForm('#loginForm', '', function(json) {
				if ( json.status == "success" && json.code =="1200" || json.code =="1000") {
					location.href = "funjsq_select.htm";
				} else if ( json.code !="1200"){
				$$('#contain_back').css('display','block');
					$$('#loginForm').css('display','block');
					$$('#service_question').css('display','block');
					$$('#formframe_wait_div').css('display','none');

					if( json.code == "1201"){
						$$.alertBox("账号不存在");
					}else if( json.code == "1202" || json.code == "1203"){
						count++;
						if(count < 3){
							if( json.code == "1202" )
				$$.alertBox("不是合法的手机号码，请重新输入");
							else
				$$.alertBox("密码错误，请重新输入");
						}else {
							count=0;
							$$.alertBox("您已重复输入错误的账号或密码3次，请您提供电话号码，以便我们提供验证码做确认");
							$$('#forgetBt').trigger("click");
						}
					}else{
					$$('#contain_back').css('display','block');
						$$('#loginForm').css('display','block');
						$$('#service_question').css('display','block');
						$$('#formframe_wait_div').css('display','none');

						$$.alertBox("请检查你的网络连接。");
						//location.href = "funjsq_login.htm";
					}
					//location.href = "funjsq_login.htm";
				} 
			});
		});

		$$('#forgetBt').click(function() {
			$$('#loginForm').css('display','none');
			$$('#registerForm').css('display','none');
			$$('#forgetForm').css('display','block');
		});

		$$('#registerBt').click(function() {
		$$.alertBox("新会员注册成功后，您将获得一个月的免费使用权。为确保您充分体验该服务，建议您在开始游戏前再注册账号");
		$$('#loginForm').css('display','none');
		$$('#registerForm').css('display','block');
		$$('#forgetForm').css('display','none');
		});

		$$('#forgetBackBt').click(function() {
			$$('#loginForm').css('display','block');
			$$('#registerForm').css('display','none');
			$$('#forgetForm').css('display','none');
		});

		$$('#registerBackBt').click(function() {
			$$('#loginForm').css('display','block');
			$$('#registerForm').css('display','none');
			$$('#forgetForm').css('display','none');
		});

		var wait=60;
		$$.time_count = function(form) {
			if(wait == 0){
				$$('#'+form+'VerifyBt').prop('disabled',false);
				wait = 60;
				$$('#'+form+'VerifyBt').val("获取验证码");
				$$('#'+form+'VerifyBt').removeClass("disabled");
			}else{
				$$('#'+form+'VerifyBt').prop('disabled',true);
				$$('#'+form+'VerifyBt').val("重新发送("+wait+"s)");
				$$('#'+form+'VerifyBt').addClass("disabled");
				wait--;
				setTimeout(function(){
					$$.time_count(form);
				},1000);
			}
		}

		$$.get_verify = function (num,form)
		{
			var cf=document.forms[num];
			if ( !$$.REG_PHONE.test($$('#'+form+'Name').val()) || $$('#'+form+'Name').val().length!=11) {
				$$.alertBox("无效的账号！");
				return false;
			}else{
				cf.hidden_funjsq_username.value = cf.funjsq_username.value;
			}

			cf.submit_flag.value=""+form+"_get_verify";
			cf.action="/admin.cgi?/funjsq_login.htm timestamp="+ts;
			wait=60;
			$$.time_count(form);
			$$.postForm('#'+form+'Form', '', function(json) {
				if ( json.code == "1100" || json.code == "1300" ) {
					$$.alertBox("验证码发送成功");
					return true;
				} else if ( json.status == "error"){
					if( json.code == "1101"){
						$$.alertBox("账号已存在，请直接登录");
					}else if( json.code == "1301"){
						$$.alertBox("账号不存在，请先注册帐号");
					}else if( json.code == "1102" || json.code == "1302"){
						$$.alertBox("您的手机号输入有误，请重新输入");
					}else if( json.code == "1103" || json.code == "1303"){
						$$.alertBox("验证码调用次数过多，请一天后重试,或者联系在线客服");
					}
					wait=0;
				} else{
					$$.alertBox("请检查你的网络连接。");
					wait=0;
				}
			});
		}

		var verify_count = 0;
		$$.forget_register_apply = function (num,form)
		{
			var cf=document.forms[num];
			if ( !$$.REG_PHONE.test($$('#'+form+'Name').val()) || $$('#'+form+'Name').val().length!=11) {
				$$.alertBox("无效的账号！");
				return false;
			}else{
				cf.hidden_funjsq_username.value = cf.funjsq_username.value;
			}
			if(form == "register"){
				if ( !$$.REG_PASSWORD2.test($$('#'+form+'Pwd').val()) ) {
					$$.alertBox("无效的密码！");
					return false;
				}else{
					cf.hidden_funjsq_password.value = cf.funjsq_password.value;
				}
			}else{
				if ( !$$.REG_PASSWORD.test($$('#'+form+'Pwd').val()) ) {
					$$.alertBox("无效的密码！");
					return false;
				}else{
					cf.hidden_funjsq_password.value = cf.funjsq_password.value;
				}
			}
			var vry=$$('#'+form+'Vry').val();
			//alert(vry.match(/\d/g).length);
			if(vry.match(/\d/g)==null || vry.match(/\d/g).length!=4){
				$$.alertBox("无效的验证码！");
				return false;
			}else
				cf.hidden_funjsq_verify.value = cf.funjsq_verify.value;

			cf.hidden_funjsq_verify.value = cf.funjsq_verify.value;
			cf.action="/admin.cgi?/funjsq_login.htm timestamp="+ts;
			cf.submit_flag.value="funjsq_"+form+"";
			$$('#formframe_wait_div').css('display','block');
			$$('#'+form+'Form').css('display','none');
			$$('#contain_back').css('display','none');
			$$('#service_question').css('display','none');
			$$.postForm('#'+form+'Form', '', function(json) {
				if ( json.code == "1104" || json.code == "1304" ) {
					cf.submit_flag.value="funjsq_login";
					$$.postForm('#'+form+'Form', '', function(json) {
						if ( json.status == "success" && json.code =="1200") {
							location.href = "funjsq_select.htm";
						}else{
							$$('#formframe_wait_div').css('display','none');
							$$('#service_question').css('display','block');
							$$('#contain_back').css('display','block');
							$$('#loginForm').css('display','block');

							$$.alertBox("请重新登陆");
						}
					});
				}else{
					$$('#formframe_wait_div').css('display','none');
					$$('#service_question').css('display','block');
					$$('#contain_back').css('display','block');
					$$('#'+form+'Form').css('display','block');
					if( form == "register"){
						if ( json.code == "1105" ) {
							$$.alertBox("注册失败，注册账号已存在");
						}else if ( json.code == "1106" ){
							$$.alertBox("注册失败，不是合法的手机号码");
						}else if ( json.code == "1107" ){
							$$.alertBox("未发送短信验证码");
						}else if ( json.code == "1108" ){
							$$.alertBox("验证码已过期，请获取新的验证码");
						}else if ( json.code == "1109" ){
							$$.alertBox("请输入正确的验证码");
						}else{
							$$.alertBox("未知错误，请重试");
						}
					}else if (form == "forget"){
						if( json.code == "1305") {
							$$.alertBox("账号不存在，请先注册手机号");
						}else if ( json.code == "1306" ){
							$$.alertBox("修改失败，不是合法的手机号码");
						}else if ( json.code == "1307" ){
							$$.alertBox("未发送短信验证码");
						}else if ( json.code == "1308" ){
							$$.alertBox("验证码已过期，请获取新的验证码");
						}else if ( json.code == "1309" ){
							$$.alertBox("请输入正确的验证码");
						}else{
							$$.alertBox("未知错误，请重试")
						}
					}

				}
			});
		}
	}

	if ( $$('#selectForm').length ) {
		var devNum = "0";
		var progress=0;
		var device_num="0";
		var click_num="100";
		var click_close="100";
		var date = new Date();
		var date_expire = new Date(expire);
		var cur_date = date.getTime();
		var expire_date = date_expire.getTime();
		if(funjsq_user_status == "1501"){
			$$.alertBox("密码已经发生变更，退出到登录页面。","确定" , function(){
					var cf=document.forms[0];
					cf.submit_flag.value="need_login";
					cf.action="/admin.cgi?/funjsq_select.htm timestamp="+ts;
					$$.postForm('#selectForm', '', function(json) {
						if ( json.status == "1" ) {
							location.href = "funjsq_login.htm";
						} else {
							$$.alertBox("请刷新重试");
						}
					});
					});
	}else if(funjsq_user_status=="1502"){
		$$.alertBox("会员过期，请续费");
	}else if(funjsq_user_status=="1601"){
		$$.alertBox("服务器维护中，预计10分钟，请等待");
	}else if(funjsq_user_status=="1602"){
   		$$.alertBox("服务器维护中，预计半小时，请等待");
	}

		$$.getData('mul_device.aspx',function(json){
			var newRow = "";
			var num=0;
			var connect_status="funjsq_disable2",connect_value,font_value,font_color="gray";
			var Area = new Array();
			for(var key in json){
				if(key == "deviceNum"){
					devNum = json.deviceNum;
				}else if ( devNum != "0"){
					Area[num]=json[key].accArea;
					$$('.funjsq_no_device').css('display','none');	
					newRow = '<table class="funjsq_mul_device" id="funjsq_device_tb'+num+'" cellspacing="3" cellpadding="0"><tr>';
					if (json[key].conn_onoff == "0"){
						if(json[key].type == "1"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_play_gray" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "2"){
							newRow += '<td rowspan="2" class="icon_windows_gray icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
						}else if (json[key].type == "3"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_xbox_gray" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "4"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_nint_gray" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "5"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_mac_gray" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "6"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_iphone_gray" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "7"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_android_gray" id="icon'+num+'"></div></td>';
						}else {
							newRow += '<td rowspan="2" class="icon_unknow_gray icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
						}
					}else{
						if(json[key].type == "1"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_play" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "2"){
							newRow += '<td rowspan="2" class="icon_windows icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
						}else if (json[key].type == "3"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_xbox" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "4"){
							newRow += '<td rowspan="2" class="icon_device"><div>'+json[key].Name+'</div><div class="icon_nint" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "5"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_mac" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "6"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_iphone" id="icon'+num+'"></div></td>';
						}else if (json[key].type == "7"){
							newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_android" id="icon'+num+'"></div></td>';
						}else {
							newRow += '<td rowspan="2" class="icon_unknow icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
						}
					}
					if(json[key].status == "1"){
						connect_status = "funjsq_enable2";
						connect_value = "open";
						font_color = "green";
						font_value = "加速成功";
					}else if (json[key].status == "6"){
						connect_status = "funjsq_disable2";
						connect_value = "close";
						font_color = "red";
						font_value = "开启失败";
					}else if (json[key].status == "8"){
						connect_status = "funjsq_disable2";
						connect_value = "close";
						font_color = "red";
						font_value = "操作异常";
					}else if (json[key].status == "10"){
						connect_status = "funjsq_disable2";
						connect_value = "close";
						font_color = "gray";
						font_value = "已断线";
					}else {
						connect_status = "funjsq_disable2";
						font_color = "gray";
						connect_value = "close";
						font_value = "联机加速";
					}
					newRow += '<td id="mulMac'+num+'">MAC:'+key+'</td><td id="mulTxb'+num+'">上传流量: '+json[key].TXbyte+'</td><td>请选择加速节点</td><td class="icon_update_shop '+connect_status+'" id="plugin'+num+'" onclick="$$.pluginClick('+num+', '+json[key].type+');" style="width:13%;"><input hidden="" value="'+connect_value+'" id="pluginBt'+num+'"><div class="circleProgress_wrapper_new" id="circle'+num+'" hidden=""><div class="progress_new" id="progress'+num+'"></div><div class="wrapper_new right_new"><div class="circleProgress_new rightcircle_new"></div></div><div class="wrapper_new left_new"><div class="circleProgress_new leftcircle_new"></div></div></div></td></tr><tr><td>';
					newRow += 'IP:'+json[key].IP+'</td><td id="mulRxb'+num+'">下载流量: '+json[key].RXbyte+'</td><td><select id="accArea'+num+'" name="accArea" onChange="$$.accAreaChange('+num+');"><option value="0" selected>请选择加速节点</option></select></td><td style="color:'+font_color+';" id="fontStatus'+num+'">'+font_value+'</td></tr></table>';
					$$('.funjsq_type_new').append(newRow);
					num++;
				}
				device_num=num;
				console.log("device num "+device_num+"");
			}

			$$.getData('https://api.funjsq.com/thirdParty/netgear/AccNode.php',function(json){
					var areaRow = "";
					var i;
					for(var key in json){
						areaRow = '<option';
						areaRow += ' value="'+key+'">'+json[key]+'</option>';

						for( i=0; i<devNum; i++){
							$$('#accArea'+i+'').append(areaRow);
							$$('#accArea'+i+'').val(Area[i]);
						}
					}
					},function(){
						var areaRow = "";
						var i;
						areaRow ='<option value="AUTO">自动（推荐）</option><option value="HK">香港</option><option value="JP">日本</option>';
						for(i=0; i<devNum; i++){
							$$('#accArea'+i+'').append(areaRow);
							$$('#accArea'+i+'').val(accArea);
						}

					});
		},function(){
		});
		
		$$.pluginClick = function(num, type) {
			var cf=document.forms[0];

			if(funjsq_user_status == "1501"){
				$$.alertBox("密码已经发生变更，退出到登录页面。","确定" , function(){
					var cf=document.forms[0];
					cf.submit_flag.value="need_login";
					cf.action="/admin.cgi?/funjsq_select.htm timestamp="+ts;
					$$.postForm('#selectForm', '', function(json) {
						if ( json.status == "1" ) {
							location.href = "funjsq_login.htm";
						} else {
							$$.alertBox("请刷新重试");
						}
					});
				});
				return false;
			}else if(funjsq_user_status=="1502"){
				$$.alertBox("会员过期，请续费");
				return false;
			}else if(funjsq_user_status=="1601"){
				$$.alertBox("服务器维护中，预计10分钟，请等待");                                return false;
			}else if(funjsq_user_status=="1602"){
				$$.alertBox("服务器维护中，预计半小时，请等待");
				return false;
			}
			for(var i=0;i < devNum;i++){
				if(i != num && $$('#plugin'+i+'').hasClass("active"))
				{
					$$.alertBox("请等待另一台设备加速完成！");
					return false;
				}
			}

			if($$('#accArea'+num+'').val() == "0")
			{
				$$.alertBox("请选择加速节点！");
				return false;
			}

			if($$('#pluginBt'+num+'').val() =="open")
			{
				$$('#pluginBt'+num+'').val("close");
				$$('#plugin'+num+'').removeClass("funjsq_enable2");
				$$('#plugin'+num+'').addClass("funjsq_disable2");
				click_close=num;
				cf.submit_flag.value="close_plugin_new";
			}else if ($$('#pluginBt'+num+'').val() =="close")
			{
				if( $$('.funjsq_enable2').length == funjsq_acc_max || $$('.funjsq_enable2').length > funjsq_acc_max) {
					$$.alertBox("您需要购买更高级的路由器才能支持更多的加速终端数，推荐型号："+funjsq_acc_tips);
					return false;
				}
				if( $$('.funjsq_enable2').length == funjsq_accNum || $$('.funjsq_enable2').length > funjsq_accNum) {
					$$.alertBox("超过您的加速终端数，您需要>升级加速终端数");
					return false;
				}
				$$('#pluginBt'+num+'').val("open");
				$$('#plugin'+num+'').removeClass("funjsq_disable2");
				$$('#plugin'+num+'').addClass("funjsq_enable2");
				cf.submit_flag.value="open_plugin_new";
			}

			var select_mac = $$('#mulMac'+num+'').html().substring(4,21);
			cf.selMac.value = select_mac;
			cf.selType.value = type;
			cf.selArea.value = $$('#accArea'+num+'').val();
			cf.action="/admin.cgi?/funjsq_select.htm timestamp="+ts;
			$$.postForm('#selectForm', '', function(json) {
				if(cf.submit_flag.value =="open_plugin_new"){
					progress=0;
					if ( json.status == "1" ) {
						//$$('#pluginBt'+num+'').val("wait");
						$$('#plugin'+num+'').removeClass("funjsq_disable2");
						$$('#plugin'+num+'').removeClass("funjsq_enable2");
						$$('#plugin'+num+'').addClass("active");
						$$('#circle'+num+'').css('display','block');
						$$('#fontStatus'+num+'').html("开启中");
						$$('#fontStatus'+num+'').css('color','gray');
						$$.progress_bar(num); 
						click_num=num;
						//click_status="1";
						//setTimeout(function(){$$.refresh_info();},2*1000);
					} else {
						//location.href = "funjsq_pay.htm";
						$$.alertBox("请重试");
					}
				}else{
					if ( json.status == "1" ) {
						
						//$$.checkStatus();
						//$$.refresh_info();
						//location.href = "funjsq_select.htm";
					} else {
						$$.alertBox("无效的请求，请重试");
						//location.href = "funjsq_select.htm";
					}
				}
			});
			return true;
		}


		//需要测试一下，打开一个设备之后 是否一直在更新他的上传下载流量（debug信息），代码里显示是更新的。。。
		$$.refresh_info = function(){
			$$.getData('refresh_status.aspx',function(json){});
			$$.getData('mul_device.aspx',function(json){
				//检查num是不是空的，空的话就显示为空页面，break
				for(var key in json){
					if(key == "deviceNum"){
						devNum = json.deviceNum;
						if(devNum=="0"){
							$$('.funjsq_no_device').css('display','block');
							break;
						}
					} /*else if(device_num=="0")//查找是否有table,没有的话就要插入表格了 记得用device_num
					{	
						var connect_status="funjsq_disable2",connect_value,font_value,font_color="gray";
						$$('.funjsq_no_device').css('display','none');	
						var newRow = '<table class="funjsq_mul_device" id="funjsq_device_tb'+device_num+'" cellspacing="3" cellpadding="0"><tr>';
						if (json[key].conn_onoff == "0"){
							if(json[key].type == "1"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_play_gray" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "2"){
								newRow += '<td rowspan="2" class="icon_windows_gray icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
							}else if (json[key].type == "3"){
								newRow += '<td rowspan="2" class=" icon_device" ><div>'+json[key].Name+'</div><div class="icon_xbox_gray" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "4"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_nint_gray" id="icon'+device_num+'"></div></td>';
							}else {
								newRow += '<td rowspan="2" class="icon_unknow_gray icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
							}
						}else{
							if(json[key].type == "1"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_play" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "2"){
								newRow += '<td rowspan="2" class="icon_windows icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
							}else if (json[key].type == "3"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_xbox" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "4"){
								newRow += '<td rowspan="2" class="icon_device"><div>'+json[key].Name+'</div><div class="icon_nint" id="icon'+device_num+'"></div></td>';
							}else {
								newRow += '<td rowspan="2" class="icon_unknow icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
							}
						}
						if(json[key].status == "1"){
							connect_status = "funjsq_enable2";
							connect_value = "open";
							font_color = "green";
							font_value = "加速成功";
						}else if (json[key].status == "6"){
							connect_status = "funjsq_disable2";
							connect_value = "close";
							font_color = "red";
							font_value = "开启失败";
						}else if (json[key].status == "8"){
							connect_status = "funjsq_disable2";
							connect_value = "close";
							font_color = "red";
							font_value = "操作异常";
						}else if (json[key].status == "10"){
							connect_status = "funjsq_disable2";
							connect_value = "close";
							font_color = "gray";
							font_value = "已断线";
						}else {
							connect_status = "funjsq_disable2";
							font_color = "gray";
							connect_value = "close";
							font_value = "联机加速";
						}
						newRow += '<td id="mulMac'+device_num+'">MAC:'+key+'</td><td id="mulTxb'+device_num+'">上传流量: '+json[key].TXbyte+'</td><td>请选择加速节点</td><td class="icon_update_shop '+connect_status+'" id="plugin'+device_num+'" onclick="$$.pluginClick('+device_num+', '+json[key].type+');" style="width:13%;"><input hidden="" value="'+connect_value+'" id="pluginBt'+device_num+'"><div class="circleProgress_wrapper_new" id="circle'+device_num+'" hidden=""><div class="progress_new" id="progress'+device_num+'"></div><div class="wrapper_new right_new"><div class="circleProgress_new rightcircle_new"></div></div><div class="wrapper_new left_new"><div class="circleProgress_new leftcircle_new"></div></div></div></td></tr><tr><td>';
						newRow += 'IP:'+json[key].IP+'</td><td id="mulRxb'+device_num+'">下载流量: '+json[key].RXbyte+'</td><td><select id="accArea'+device_num+'" name="accArea" onChange="$$.accAreaChange('+device_num+');"><option value="0" selected>请选择加速节点</option></select></td><td style="color:'+font_color+';" id="fontStatus'+device_num+'">'+font_value+'</td></tr></table>';
						$$('.funjsq_type_new').append(newRow);
						++device_num;
						break;
					}*/ else//有设备
					{
						var flag="0";
						$$('.funjsq_no_device').css('display','none');
						for(var n=0;n<device_num;n++){//设置一个flag，判断table有没有这个mac ，没有的话就要插入tr
							var table_mac = $$('#mulMac'+n+'').html().substring(4,21);
							//console.log("table mac"+table_mac+"");
							//console.log("key mac"+key+"");
							if(key==table_mac){
								//更新icon 其他都更新过了吧
								//console.log("match mac"+table_mac+"");
								//console.log(json[key].conn_onoff);
								flag="1";
								if (json[key].conn_onoff == "0"){
									if(json[key].type == "1"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_play_gray");
										//console.log("play灰色的"+n+"");
									}else if(json[key].type == "2"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_windows_gray");
										$$("#icon"+n+"").addClass("icon_device");
										//var hh=$$("#icon"+n+"").attr("class");
										//console.log("windows灰色的"+n+hh+"");
									}else if(json[key].type == "3"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_xbox_gray");
										//console.log("xbox灰色的"+n+"");
									}else if(json[key].type == "4"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_nint_gray");
										//console.log("int灰色的"+n+"");
									}else if(json[key].type == "5"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_mac_gray");
										//console.log("int灰色的"+n+"");
									}else if(json[key].type == "6"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_iphone_gray");
										//console.log("int灰色的"+n+"");
									}else if(json[key].type == "7"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_android_gray");
										//console.log("int灰色的"+n+"");
									}else{
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_unknow_gray");
										$$("#icon"+n+"").addClass("icon_device");
										//var hh=$$("#icon"+n+"").attr("class");
										//console.log("灰色的"+n+hh+"");
									}						
								}else {
									if(json[key].type == "1"){
										
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_play");
										//console.log("play亮起来"+n+"");
									}else if(json[key].type == "2"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_windows");
										$$("#icon"+n+"").addClass("icon_device");
										var hh=$$("#icon"+n+"").attr("class");
										//console.log("window亮起来"+n+hh+"");
									}else if(json[key].type == "3"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_xbox");
										//console.log("xbox亮起来"+n+"");
									}else if(json[key].type == "4"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_nint");
										//console.log("nint亮起来"+n+"");
									}else if(json[key].type == "5"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_mac");
										//console.log("int灰色的"+n+"");
									}else if(json[key].type == "6"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_iphone");
										//console.log("int灰色的"+n+"");
									}else if(json[key].type == "7"){
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_android");
										//console.log("int灰色的"+n+"");
									}else{
										$$("#icon"+n+"").removeClass();
										$$("#icon"+n+"").addClass("icon_unknow ");
										$$("#icon"+n+"").addClass("icon_device");
										//var hh=$$("#icon"+n+"").attr("class");
										//console.log("else亮起来"+n+hh+"");																				
									}									
								}
								$$('#mulTxb'+n+'').html('上传流量: '+json[key].TXbyte+'');
								$$('#mulRxb'+n+'').html('下载流量: '+json[key].RXbyte+'');
								console.log("click num "+click_num);
								console.log("n "+n);
								console.log("click_close "+click_close);
								if(progress!="0" && n==click_num && progress<30){
								
									$$('#pluginBt'+n+'').val("wait");
									$$('#plugin'+n+'').removeClass("funjsq_disable2");
									$$('#plugin'+n+'').removeClass("funjsq_enable2");
									$$('#plugin'+n+'').addClass("active");
									$$('#circle'+n+'').css('display','block');
									$$('#fontStatus'+n+'').html("开启中");
									$$('#fontStatus'+n+'').css('color','gray');
									//$$.progress_bar(n);
									console.log("process"+n+progress+"");
									if(progress >= 96){
										$$('#progress'+n+'').html("90%");										
										$$('#circle'+n+'').css('display','none');
										$$('#plugin'+n+'').removeClass("funjsq_enable2");		
										$$('#plugin'+n+'').addClass("funjsq_disable2");
				
										$$('#resetBt2').trigger("click");
										$$('#pluginBt'+n+'').val("open");
										progress=0;
									}else{
										$$('#progress'+n+'').html(""+progress+"%");
										progress = parseInt(progress)+10;
										console.log("process in"+n+progress+"");
									}
								}else if(click_close==n&&json[key].status=="1"){
									$$('#fontStatus'+n+'').html("联机加速");
									$$('#fontStatus'+n+'').css('color','gray');
									click_close="100";
								}
								else{
								console.log("正常刷新");
								if( json[key].status == "1" )
								{
									console.log("not click");
									//progress=0;
									$$('#pluginBt'+n+'').val("open");
									$$('#plugin'+n+'').removeClass("funjsq_disable2");
									$$('#plugin'+n+'').addClass("funjsq_enable2");
									$$('#plugin'+n+'').removeClass("active");
									$$('#circle'+n+'').css('display','none');
									$$('#fontStatus'+n+'').html("加速成功");
									$$('#fontStatus'+n+'').css('color','green');
								}else if( json[key].status == "7" ){
									$$('#pluginBt'+n+'').val("wait");
									$$('#plugin'+n+'').removeClass("funjsq_disable2");
									$$('#plugin'+n+'').removeClass("funjsq_enable2");
									$$('#plugin'+n+'').addClass("active");
									$$('#circle'+n+'').css('display','block');
									$$('#fontStatus'+n+'').html("开启中");
									$$('#fontStatus'+n+'').css('color','gray');
									//$$.progress_bar(n);
									console.log("process"+n+progress+"");
									if(progress >= 96){
										$$('#progress'+n+'').html("90%");										
										$$('#circle'+n+'').css('display','none');
										$$('#plugin'+n+'').removeClass("funjsq_enable2");		
										$$('#plugin'+n+'').addClass("funjsq_disable2");
				
										$$('#resetBt2').trigger("click");
										$$('#pluginBt'+n+'').val("open");
										progress=0;
									}else{
										$$('#progress'+n+'').html(""+progress+"%");
										progress = parseInt(progress)+10;
										console.log("process in"+n+progress+"");
									}
									console.log("process"+progress+"");
									console.log("加速中");
									//setTimeout(function(){$$.updateStatus(select_mac);},3*1000);
								}else{
									//progress=0;
									$$('#pluginBt'+n+'').val("close");
									$$('#plugin'+n+'').removeClass("funjsq_enable2");
									$$('#plugin'+n+'').addClass("funjsq_disable2");
									$$('#plugin'+n+'').removeClass("active");
									$$('#circle'+n+'').css('display','none');
								if(json[key].status == "6" ){
									$$('#fontStatus'+n+'').html("开启失败");
									$$('#fontStatus'+n+'').css('color','red');
									console.log("kaiqishibai");
								}else if( json[key].status == "10" ){
									$$('#fontStatus'+n+'').html("已断线");
									$$('#fontStatus'+n+'').css('color','gray');
									console.log("yiduanxian");
								}else if( json[key].status == "8" ){
									$$('#fontStatus'+n+'').html("操作异常");
									$$('#fontStatus'+n+'').css('color','red');
									console.log("操作异常"+n+"");
								}else if ( json[key].status == "0" ){
									$$('#fontStatus'+n+'').html("联机加速");
									$$('#fontStatus'+n+'').css('color','gray');
									console.log("练级加速"+n+"");
									}
								}	
							}
								//该status
								console.log("progress break"+progress+"");
								break;	
							}						
						}
						if(flag=="0"){
							//新设备 device_num +1
							var newRow="";
							var connect_status="funjsq_disable2",connect_value,font_value,font_color="gray";
							newRow = '<table class="funjsq_mul_device" id="funjsq_device_tb'+device_num+'" cellspacing="3" cellpadding="0"><tr>';
							if (json[key].conn_onoff == "0"){
							if(json[key].type == "1"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_play_gray" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "2"){
								newRow += '<td rowspan="2" class="icon_windows_gray icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
							}else if (json[key].type == "3"){
								newRow += '<td rowspan="2" class=" icon_device" ><div>'+json[key].Name+'</div><div class="icon_xbox_gray" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "4"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_nint_gray" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "5"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_mac_gray" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "6"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_iphone_gray" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "7"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_android_gray" id="icon'+device_num+'"></div></td>';
							}else {
								newRow += '<td rowspan="2" class="icon_unknow_gray icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
							}
						}else{
							if(json[key].type == "1"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_play" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "2"){
								newRow += '<td rowspan="2" class="icon_windows icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
							}else if (json[key].type == "3"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_xbox" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "4"){
								newRow += '<td rowspan="2" class="icon_device"><div>'+json[key].Name+'</div><div class="icon_nint" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "5"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_mac" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "6"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_iphone" id="icon'+device_num+'"></div></td>';
							}else if (json[key].type == "7"){
								newRow += '<td rowspan="2" class=" icon_device"><div>'+json[key].Name+'</div><div class="icon_android" id="icon'+device_num+'"></div></td>';
							}else {
								newRow += '<td rowspan="2" class="icon_unknow icon_device" id="icon'+device_num+'"><div>'+json[key].Name+'</div></td>';
							}
						}
						if(json[key].status == "1"){
							connect_status = "funjsq_enable2";
							connect_value = "open";
							font_color = "green";
							font_value = "加速成功";
						}else if (json[key].status == "6"){
							connect_status = "funjsq_disable2";
							connect_value = "close";
							font_color = "red";
							font_value = "开启失败";
						}else if (json[key].status == "8"){
							connect_status = "funjsq_disable2";
							connect_value = "close";
							font_color = "red";
							font_value = "操作异常";
						}else if (json[key].status == "10"){
							connect_status = "funjsq_disable2";
							connect_value = "close";
							font_color = "gray";
							font_value = "已断线";
						}else {
							connect_status = "funjsq_disable2";
							font_color = "gray";
							connect_value = "close";
							font_value = "联机加速";
						}
						newRow += '<td id="mulMac'+device_num+'">MAC:'+key+'</td><td id="mulTxb'+device_num+'">上传流量: '+json[key].TXbyte+'</td><td>请选择加速节点</td><td class="icon_update_shop '+connect_status+'" id="plugin'+device_num+'" onclick="$$.pluginClick('+device_num+', '+json[key].type+');" style="width:13%;"><input hidden="" value="'+connect_value+'" id="pluginBt'+device_num+'"><div class="circleProgress_wrapper_new" id="circle'+device_num+'" hidden=""><div class="progress_new" id="progress'+device_num+'"></div><div class="wrapper_new right_new"><div class="circleProgress_new rightcircle_new"></div></div><div class="wrapper_new left_new"><div class="circleProgress_new leftcircle_new"></div></div></div></td></tr><tr><td>';
						newRow += 'IP:'+json[key].IP+'</td><td id="mulRxb'+device_num+'">下载流量: '+json[key].RXbyte+'</td><td><select id="accArea'+device_num+'" name="accArea" onChange="$$.accAreaChange('+device_num+');"><option value="0" selected>请选择加速节点</option></select></td><td style="color:'+font_color+';" id="fontStatus'+device_num+'">'+font_value+'</td></tr>';
						$$('.funjsq_type_new').append(newRow);
						++device_num;
						}	
					}//循环table中的mac 查找是否有在table但不在json中，如果有 则删除table中的这一项,但是不确定断连得话是会删除还是
				}
			});
			setTimeout(function(){$$.refresh_info();},3000);
		} 
		
		$$.refresh_info();
		$$.accAreaChange = function(count) {
			$$.getData('mul_device.aspx',function(json){
				var num=0;
				for(var key in json){
					if(count == num){
						if( json[key].status == "1" ){
							var cf = document.forms[0];

							cf.selMac.value = key;
							cf.selType.value = json[key].type;
							cf.selArea.value = $$('#accArea'+num+'').val();
							cf.action="/admin.cgi?/funjsq_select.htm timestamp="+ts;
							cf.submit_flag.value ="open_plugin_new";
							$$.postForm('#selectForm', '', function(json) {
								progress=0;
								if ( json.status == "1") {
									//$$.refresh_info();
								} else {
									$$.alertBox("请重试");
								}
							});
						}
					}

					if( key != "deviceNum" )
						num++;
				}
			});
		}
		//$$.refresh_info();
		/* $$.updateStatus = function(select_mac) {
			$$.getData('refresh_status.aspx',function(json){
				if(json.refresh_status == "1"){
					$$.getData('mul_device.aspx',function(json){
						var num=0;
						for(var key in json){
							if(key == select_mac){
								if( json[key].status == "1" ){
									progress=0;
									$$('#pluginBt'+num+'').val("open");
									$$('#plugin'+num+'').removeClass("funjsq_disable2");
									$$('#plugin'+num+'').addClass("funjsq_enable2");
									$$('#plugin'+num+'').removeClass("active");
									$$('#circle'+num+'').css('display','none');
									$$('#mulTxb'+num+'').html('上传流量: '+json[key].TXbyte+'');
									$$('#mulRxb'+num+'').html('下载流量: '+json[key].RXbyte+'');
									$$('#fontStatus'+num+'').html("加速成功");
									$$('#fontStatus'+num+'').css('color','green');
									console.log("加速成功"+json[key].TXbyte+json[key].RXbyte+"");
									setTimeout(function(){$$.updateStatus(select_mac);},1*1000);
								}else if( json[key].status == "7" ){
									$$('#pluginBt'+num+'').val("wait");
									$$('#plugin'+num+'').removeClass("funjsq_disable2");
									$$('#plugin'+num+'').removeClass("funjsq_enable2");
									$$('#plugin'+num+'').addClass("active");
									$$('#circle'+num+'').css('display','block');
									$$('#fontStatus'+num+'').html("开启中");
									$$('#fontStatus'+num+'').css('color','gray');
									$$.progress_bar(num);
									console.log("加速中");
									setTimeout(function(){$$.updateStatus(select_mac);},3*1000);
								}else{
									progress=0;
									$$('#pluginBt'+num+'').val("close");
									$$('#plugin'+num+'').removeClass("funjsq_enable2");
									$$('#plugin'+num+'').addClass("funjsq_disable2");
									$$('#plugin'+num+'').removeClass("active");
									$$('#circle'+num+'').css('display','none');
									if( json[key].status == "6" ){
										$$('#fontStatus'+num+'').html("开启失败");
										$$('#fontStatus'+num+'').css('color','red');
										console.log("kaiqishibai");
									}else if( json[key].status == "10" ){
										$$('#fontStatus'+num+'').html("已断线");
										$$('#fontStatus'+num+'').css('color','gray');
										console.log("yiduanxian");
									}else if( json[key].status == "8" ){
										$$('#fontStatus'+num+'').html("操作异常");
										$$('#fontStatus'+num+'').css('color','red');
										console.log("操作异常"+num+"");
									}else if ( json[key].status == "0" ){
										$$('#fontStatus'+num+'').html("联机加速");
										$$('#fontStatus'+num+'').css('color','gray');
										console.log("练级加速"+num+"");
									}
									setTimeout(function(){$$.updateStatus(select_mac);},1000);
								}
							}

							if( key != "deviceNum" )
								num++;
						}
					},function(){
					});
				}
			},function(){
			});
		} */

		
		$$('#resetBt2').click(function() {
			var cf=document.forms[0];
			cf.submit_flag.value="reset_plugin";
			cf.action="/admin.cgi?/funjsq_select.htm timestamp="+ts;
			$$.postForm('#selectForm', '', function(json) {
				if ( json.status == "1" ) {
					//$$.checkStatus();
					location.href = "funjsq_select.htm";
				} else {
					$$.alertBox("无效的请求，请重试");
					//location.href = "funjsq_select.htm";
				}
			});
			return true;
		});

		$$.progress_bar = function(num) {
			if(progress == 90){
				$$('#progress'+num+'').html("90%");
				setTimeout(function(){
					$$('#circle'+num+'').css('display','none');
					$$('#plugin'+num+'').removeClass("funjsq_enable2");		
					$$('#plugin'+num+'').addClass("funjsq_disable2");
				},1500);
				$$('#resetBt2').trigger("click");
				$$('#pluginBt'+num+'').val("open");
			}else{
				$$('#progress'+num+'').html(""+progress+"%");
				progress = parseInt(progress)+10;
				//setTimeout($$.progress_bar(num),3000);
			}
		}
	}

	if ( $$('#payForm').length ) {
		var suit_num;
		var acc_num = 1;
		var select_id;
		var price = new Array();
		var suit_type = 1;
		var ExpireFlag = 0;
                var user_accNum;

		$$('#nowNum').html(funjsq_accNum);
		$$('#afterNum').html(funjsq_accNum);
		$$.getData('http://api.funjsq.com/thirdParty/netgear/v2/SearchProductInfo.php?Mobile='+mobile+'',function(json){
				var i=0;
				if ( json.List.length > 0 ) {
					for ( i = 0; i < json.List.length; i++ ){
						var id = json.List[i].ID;
						price[id] = json.List[i].Price;
						if(json.List[i].ID == "181"){
							$$('#pay1').html(json.List[i].Price);
						}else if(json.List[i].ID == "186"){
							$$('#pay2').html(json.List[i].Price);						
						}else if(json.List[i].ID == "191"){
							$$('#pay3').html(json.List[i].Price);
						}else if(json.List[i].ID == "196"){
							$$('#pay4').html(json.List[i].Price);
						}
					}
					$$.selectSuit("1");
				}
				ExpireFlag = json.ExpireFlag;
				user_accNum = json.user_accNum;
				if( ExpireFlag == "0"){
					$$(".num_select").css("display","none");
					if(user_accNum){
						$$('#num'+user_accNum+'').addClass("active");
						for(var m=1;m<6;m++){
							if(m != user_accNum)
								$$('#num'+m+'').removeClass("active");
						}
					}
					$$.selectNum(user_accNum);
				}else if( ExpireFlag == "1"){
					$$("#suitType2").css("display","none");
				}
				if( json.code == "1001")
					$$.alertBox("用户不存在");
	},function(){});

		$$.chExpireTime = function(suit_num){
			var ExpireTime = $$('.pay').find('li.text').find('p:eq(1)').find('span:eq(0)').text();//console.log(ExpireTime);
			var TimeType = $$('.pay li:eq(0)').find('span.active').attr('data-fun-product');//console.log(T);
			var UseTime = $$('.pay li:eq(1)').find('span.active').attr('data-fun-product');
			if(TimeType !='' && TimeType != null && TimeType != undefined ){
				if(TimeType == 'd'){
					if(moment().isBefore(ExpireTime) != true){
						ExpireTime = moment().format('YYYY-MM-DD HH:mm:ss');
					}
					console.log(ExpireTime);
					var newExpireTime=moment(ExpireTime,'YYYY-MM-DD HH:mm:ss').add(UseTime,'day').format('YYYY-MM-DD HH:mm:ss');
					console.log(newExpireTime);
					$$('.pay').find('li.text').find('p:eq(1)').find('span:eq(1)').text(newExpireTime);
					if(suit_num==1){
						var updateTime=moment(ExpireTime,'YYYY-MM-DD HH:mm:ss').add(1,'month').format('YYYY-MM-DD HH:mm:ss');
						$$("#pay_time").html(updateTime);
					}else if(suit_num==2){
						var updateTime=moment(ExpireTime,'YYYY-MM-DD HH:mm:ss').add(3,'month').format('YYYY-MM-DD HH:mm:ss');
						$$("#pay_time").html(updateTime);
					}else if(suit_num==3){
						var updateTime=moment(ExpireTime,'YYYY-MM-DD HH:mm:ss').add(6,'month').format('YYYY-MM-DD HH:mm:ss');
						$$("#pay_time").html(updateTime);
					}else if(suit_num==4){
						var updateTime=moment(ExpireTime,'YYYY-MM-DD HH:mm:ss').add(12,'month').format('YYYY-MM-DD HH:mm:ss');
						$$("#pay_time").html(updateTime);
					}
				}else if (TimeType == 'h'){
					if(moment().isBefore(ExpireTime) != true){
						ExpireTime = moment().format('YYYY-MM-DD HH:mm:ss');
					}
					//console.log(ExpireTime);
					var newExpireTime=moment(ExpireTime,'YYYY-MM-DD HH:mm:ss').add(UseTime,'hours').format('YYYY-MM-DD HH:mm:ss');
					$$('.pay').find('li.text').find('p:eq(1)').find('span:eq(1)').text(newExpireTime);
				}else{
					$$('.pay').find('li.text').find('p:eq(1)').find('span:eq(1)').text(ExpireTime);
				}
			}else{
				$$('.pay').find('li.text').find('p:eq(1)').find('span:eq(1)').text(ExpireTime);
			}
			$$.chPayAmount();
		}

		/**
		 * [chPayAmount 更改余额]
		 * @return   {[type]}                 [description]
		 * @Author   vincent
		 * @DateTime 2017-08-12T14:47:33+0800
		 */
		$$.chPayAmount = function(){
			var Amount= $$('.pay li:eq(1)').find('span.active').find('em').attr('data-fun-product');
			$$('.pay li:eq(4)').find('p:eq(1)').find('b').text(Amount);
		}

		$$('#payBt').click(function() {
			var val=$$('.sum b').html();
			if(val == "0.00"){
				$$.alertBox("请先选择加速终端数");
				return false;
			}
			//var id=suit_num*3 + 178;
			window.open("https://api.funjsq.com/thirdParty/netgear/v2/zfb/alipayapi.php?Mobile="+mobile+"&ID="+select_id+"&R=XR500");
			$$('#payForm').css('display','none');
			$$('#confirmForm').css('display','block');
		});

		$$.selectType = function(num){
			if(num == "1"){
				$$('#suitType1').addClass("active");
				$$('#suitType2').removeClass("active");
				$$('.suit_select').css('display','block');
				if(ExpireFlag == "0"){
					$$('.num_select').css('display','none');
					acc_num=user_accNum;
				}
				suit_type=1;
				select_id = parseInt(suit_num)*5 + parseInt(acc_num) + 175;
				$$('.sum b').html(price[select_id]);
			}else{
				$$('.num_select').css('display','block');
				$$('#suitType2').addClass("active");
				$$('#suitType1').removeClass("active");
				$$('.suit_select').css('display','none');
				suit_type=2;
				for(var j=1;j<6;j++)
					$$('#num'+j+'').removeClass("active");
				$$('.sum b').html("0.00");
			}
		}

		$$.selectSuit = function(num){
			suit_num = num;
			select_id = parseInt(suit_num)*5 + parseInt(acc_num) + 175;
			$$('#suit'+num+'').addClass("active");
			$$('.sum b').html(price[select_id]);
			for(var j=1;j<5;j++){
				if(j != num)
					$$('#suit'+j+'').removeClass("active");
			}
			$$.chExpireTime(suit_num);
		}

		$$.selectNum = function(num){
			if(suit_type == "1"){
			/*	if(ExpireFlag == "0"){
					if( num > funjsq_acc_max ){
						$$.alertBox("您需要购买更高级的路由器才能支持更多的加速终端数，推荐型号："+funjsq_acc_tips);
						return false;
					}
				}else
				*/
				if(ExpireFlag == "1"){
					if(num > funjsq_acc_max){
						$$.alertBox("您需要购买更高级的路由器才能支持更多的加速终端数，推荐型号："+funjsq_acc_tips);
						return false;
					}
				}
			}else if(suit_type == "2"){
				if(ExpireFlag == "0"){
					if( num < user_accNum){
						$$.alertBox("您当前套餐未过期，您需要等待套餐过期之后才可以降低终端数");
						return false;
					}
					if( num > funjsq_acc_max){
						$$.alertBox("您需要购买更高级的路由器才能支持更多的加速终端数，推荐型号："+funjsq_acc_tips);
						return false;
					}

					if( num == user_accNum)
						return false;
				}
			}
			acc_num = num;
			$$('#afterNum').html(num);
			$$('#num'+num+'').addClass("active");
			if(suit_type=="2")
				select_id = parseInt(acc_num) + 287;
			else
				select_id = parseInt(suit_num)*5 + parseInt(acc_num) + 175;
			$$('.sum b').html(price[select_id]);
			for(var n=1;n<6;n++){
				if(n != num)
					$$('#num'+n+'').removeClass("active");
			}
		}

		$$('#suit1').trigger('click');

		$$('#payTrouble').click(function() {
			if(QQSupport== "" )
			$$.alertBox("请您点击【加速器客服】进行在线咨询，或者关注微信公众号：【帆游加速器】了解更多优惠信息");
			else
			$$.alertBox("请您点击【加速器客服】进行在线咨询，或者关注微信公众号：【帆游加速器】了解更多优惠信息");
		location.href="funjsq_pay.htm";
		}); 

		$$('#payFinish').click(function() {
			var cf=document.forms[0];
			cf.action="/admin.cgi?/funjsq_pay.htm timestamp="+ts;
			$$('#formframe_wait_div').css('display','block');	

			$$('#confirmForm').css('display','none');
			$$('#contain_back').css('display','none');
			$$.postForm('#payForm', '', function(json) {
				if ( json.code == "1403" ) {
					location.href="funjsq_select.htm";
				} else if(json.code == "1404"){
					$$.alertBox("支付失败，请重新尝试");
					$$('#payForm').css('display','block');
					$$('#confirmForm').css('display','none');
					$$('#formframe_wait_div').css('display','none');
				} else{
					$$.alertBox("请检查你的网络连接。");
					$$('#formframe_wait_div').css('display','none');
					$$('#payForm').css('display','block');
					$$('#confirmForm').css('display','none');
				}
			});
		});
	}

	$$.checkStatus = function()
	{
		$$.getData('plugin_info.aspx',function(json){
			if (json.funjsq_status != "0")
			setTimeout(function(){$$.checkStatus();},3*1000);
			else
			return true;
		});
	}


	}); // end ready function

}(jQuery));
