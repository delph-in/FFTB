function dec_to_string(d)
{
	return escape(d.sign) +':' + d.type + ':'+ d.from + "-" + d.to + ":" + d.inferred;
}

var dspan = "";

function do_save(accepted, and_then)
{
	if(accepted!=-1)save_comment()
	var xr = new XMLHttpRequest();
	var sid = window.location.search;
	xr.open("POST", "/save" + sid + "&accepted=" + accepted, true);
	xr.onreadystatechange = function()
	{
		if(xr.readyState==4)
		{
			if(xr.status == 200)
			{
				and_then();
			}
			else
			{
				alert("unexpected http code: " + xr.status + " = " + xr.responseText);
			}
		}
	}
	xr.send("");
}

function next_item()
{
	window.location = "/private/next" + window.location.search;
}

function next_unannotated_item()
{
	window.location = "/private/next_unannotated" + window.location.search;
}

function prev_item()
{
	window.location = "/private/prev" + window.location.search;
}

function do_accept()
{
	do_save(1, next_unannotated_item);
}

function do_reject()
{
	do_save(0, next_unannotated_item);
}

function get_candidates(request_decisions)
{
	xr = new XMLHttpRequest();
	var alldecs = decisions.map(dec_to_string);
	if(message && message.olddec)
	{
		var bitstring = "_olddecs=";
		for(var x in message.olddec)
			if(message.olddec[x].enabled)bitstring += "1";
			else bitstring += "0";
//				alldecs = alldecs.concat([message.olddec[x]]);
		alldecs= alldecs.concat([bitstring]);
	}
	var query_string = alldecs.join("&");
	sid = window.location.search;
	xr.open("POST", "/session" + sid + ";" + request_decisions + ";" + dspan + ";" + query_string, true);
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
				alert("unexpected http code: " + xr.status + " = " + xr.responseText);
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
}

function refilter()
{
	get_candidates(0);
}

var message;
var decisions = [];

get_candidates(1);

function exit_system()
{
	window.location = "/private/exit";
}

function save_comment()
{
	var xr = new XMLHttpRequest();
	var sid = window.location.search;
	var text = document.getElementById("comment").value
	message.comment = text
	xr.open("POST", "/comment" + sid + "&" + encodeURIComponent(text), true);
	xr.send("");
}
