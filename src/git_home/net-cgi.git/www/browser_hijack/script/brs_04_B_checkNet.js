function initPage()
{
	var head_tag  = document.getElementsByTagName("h1");
	var connect_text = document.createTextNode(bh_internet_checking);
	head_tag[0].appendChild(connect_text);

        var image = document.getElementById("waiting_img");
        image.setAttribute("src", "image/wait30.gif");
	
	loadValue();
}

function loadValue()
{
	var forms = document.getElementsByTagName("form");
	var cf = forms[0];

	if(ping_result == "failed")	//failed
	{
		if (((getTop(window).netgear_region.toUpperCase() == "WW" || getTop(window).netgear_region == "") && getTop(window).gui_region == "Russian" ) || getTop(window).netgear_region.toUpperCase() == "RU")
			this.location.href = "RU_welcome.htm";
		else{
		    if(getTop(window).dsl_enable_flag == "1"){	
			if(ping_ip_result == "failed" && ping_gate_result == "failed")
				this.location.href = "BRS_log11_dhcp_ping_fail_ip_fail.html";
			else if(ping_ip_result == "failed" && ping_gate_result == "success")
				this.location.href = "BRS_log11_dhcp_ping_fail_ip_fail_gate_sucess.html";
			else if(ping_ip_result == "success" && ping_gate_result == "failed")
				this.location.href = "BRS_log11_dhcp_ping_fail_ip_sucess_gate_fail.html";
			else
				this.location.href = "BRS_log11_dhcp_ping_fail.html";
		    }
		    else
 			this.location.href = "BRS_05_networkIssue.html";
		}
	}
	else if(ping_result == "success") //success
	{
		if(getTop(window).dsl_enable_flag == "1")
		{
			if(ping_ip_result == "failed" && ping_gate_result == "failed")
				this.location.href = "BRS_log11_dhcp_ping_success_ip_fail.html";
			else if(ping_ip_result == "failed" && ping_gate_result == "success")
				this.location.href = "BRS_log11_dhcp_ping_success_ip_fail_gate_success.html";
			else if(ping_ip_result == "success" && ping_gate_result == "failed")
				this.location.href = "BRS_log11_dhcp_ping_success_ip_sucess_gate_fail.html";
			else
				this.location.href = "BRS_log11_dhcp_ping_success.html";
		}
		else
 	         	this.location.href = "BRS_security.html";
	}
	setTimeout("loadValue();", 1000);
}

addLoadEvent(initPage);
