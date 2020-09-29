function initPage()
{
	loadValue();
}

function loadValue()
{
	var forms = document.getElementsByTagName("form");
	var cf = forms[0];
	var finish=0;

	if(ping_result == "failed")	//failed
	{
	  if(getTop(window).dsl_enable_flag == 1){
		  this.location.href="BRS_log02_connection_fail.html";
		  finish=1;
	  }else{
		if( getTop(window).apmode_flag == "1" && ap_mode == "1" && ap_mode_detection_flag == "1" )
			this.location.href="BRS_05_networkIssue.html";
		else if((getTop(window).have_broadband == "1" && (getTop(window).is_ru_version == 1 || getTop(window).is_pr_version == 1)) || getTop(window).have_lte_flag == "1")
			this.location.href = "BRS_021_genieHelp.html";
		else if (getTop(window).is_ru_version == 1)
			this.location.href = "RU_welcome.htm";//33693
		else
			this.location.href = "BRS_02_genieHelp.html";
		finish=1;
	  }
	}
	else if(ping_result == "success") //success
	{
	  if(getTop(window).dsl_enable_flag == 1){
		  this.location.href="BRS_log02_connection_success.html";
		  finish=1;
	  }else{
		if( getTop(window).hdd_multi_user ==1)
		{
			if(hijack_process == 2 && parent.first_hdd_nofind == 0)
				cf.submit();
			else
				this.location.href = "BRS_security.html";
		}
		else{
			if(hijack_process == 2)
				cf.submit();
			else
				this.location.href = "BRS_security.html";		
		}
		finish=1;
	  }
	}

	if(finish != 1)
		setTimeout("loadValue();", 1000);
}

addLoadEvent(initPage);
