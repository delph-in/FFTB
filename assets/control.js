var	use_old_decs = [];

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

var firsttime = 1;

function get_candidates()
{
	xr = new XMLHttpRequest();
	var alldecs = decisions.map(dec_to_string);
	if(message && message.olddec)
	{
		for(var x in message.olddec)
			if(use_old_decs[x])
				alldecs = alldecs.concat([message.olddec[x]]);
	}
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
			else if(xr.status == 0)
			{
				// happens when the browser cancels the request because it is moving to another page...
				return;
			}
			else if(xr.status == 404)
			{
				window.history.back();
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
	show_sentence();
	show_decisions();
	show_discriminants();
	show_trees();
	show_item_and_profile_id();
	if(firsttime)
	{
		show_old();
		enable_all_old();
		firsttime = 0;
	}
}

function refilter()
{
	get_candidates();
}

var message;
var decisions = [];

refilter();
