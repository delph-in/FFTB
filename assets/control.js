var	use_old_decs = 0;

function dec_to_string(d)
{
	return escape(d.type) + "=" + d.from + "-" + d.to;
	/*switch(d.type)
	{
		case	'span':
			return "span="+d.from+"-"+d.to;
	}
	return "";*/
}

var dspan = "";

function get_candidates()
{
	xr = new XMLHttpRequest();
	var alldecs = decisions.map(dec_to_string);
	if(use_old_decs)alldecs = alldecs.concat(message.olddec);
	var query_string = alldecs.join("&");
	sid = window.location.search;
	xr.open("POST", "/session" + sid + ";" + dspan + ";" + query_string, true);
	xr.onreadystatechange = function()
	{
		if(xr.readyState==4)
		{
			if(xr.status == 200)
			{
				//alert(xr.responseText);
				eval("message = " + xr.responseText);
				got_reply();
			}
			else
			{
				alert("unexpected http code: " + xr.status);
			}
		}
	}
	xr.send("");
}

shew_sentence = 0;

function got_reply()
{
	show_trees();
	show_decisions();
	show_discriminants();
	//if(!shew_sentence)
	//{
		//shew_sentence = 1;
		show_sentence();
	//}
}

function refilter()
{
	get_candidates();
}

var message;
var decisions = [];

refilter();
