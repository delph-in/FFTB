var svg = element("svg");
svg.setAttribute("version", "1.1");
document.getElementById("disc-scroller").appendChild(svg);
svg.style.background = "white";

function element(type)
{
	return document.createElementNS("http://www.w3.org/2000/svg", type);
}

function container()
{
	var g = element("svg");
	for(var x=0;arguments.length>x;x=x+1) { g.appendChild(arguments[x]); }
	return g;
}

function line(x1,y1,x2,y2)
{
	var l = element("line");
	l.setAttributeNS(null, "x1", x1);
	l.setAttributeNS(null, "x2", x2);
	l.setAttributeNS(null, "y1", y1);
	l.setAttributeNS(null, "y2", y2);
	l.setAttributeNS(null, "style", "stroke: black;");
	return l;
}

function text(str)
{
	var t = element("text");
	t.appendChild(document.createTextNode(str));
	svg.appendChild(t);
	var bbx = t.getBBox();
	svg.removeChild(t);
	t.bbx = bbx;
	return t;
}

DAUGHTER_HSPACE=10
DAUGHTER_VSPACE=20

function render_yield(str)
{
	var y = text(str);
	y.setAttributeNS(null, "y", y.bbx.height * 2/3);
	y.setAttributeNS(null, "style", "fill: red;");
	var g = element("svg");
	g.appendChild(y);
	g.mywidth = y.bbx.width;
	return g;
}

function render_tree(t)
{
	var	dtrs = [];
	var wtot = 0;
	for(var x in t.daughters)
	{
		dtrs[x] = render_tree(t.daughters[x]);
		wtot += dtrs[x].mywidth;
	}
	var lexical;
	if(dtrs.length)
		wtot += DAUGHTER_HSPACE * (dtrs.length-1);
	else
	{
		lexical = render_yield(getYield(t.from, t.to));
		wtot = lexical.mywidth;
	}
	var dtrs_wtot = wtot;

	var g = element("svg");
	var n = text(t.label);
	var nw = n.width;
	var nh = n.height;
	nw = n.bbx.width;
	nh = n.bbx.height;
	if(nw > wtot)wtot = nw;
	n.setAttributeNS(null, "x", wtot / 2 - nw / 2);
	n.setAttributeNS(null, "y", nh * 2/3);
	g.appendChild(n);
	var	dtr_x = wtot / 2 - dtrs_wtot / 2;
	for(var x in dtrs)
	{
		dtrs[x].setAttributeNS(null, "y", nh + DAUGHTER_VSPACE);
		dtrs[x].setAttributeNS(null, "x", dtr_x);
		g.appendChild(dtrs[x]);
		g.appendChild(line(wtot/2, nh, dtr_x + dtrs[x].mywidth/2, nh + DAUGHTER_VSPACE - 1));
		dtr_x += dtrs[x].mywidth + DAUGHTER_HSPACE;
	}
	if(lexical)
	{
		lexical.setAttributeNS(null, "y", nh + DAUGHTER_VSPACE);
		lexical.setAttributeNS(null, "x", dtr_x);
		g.appendChild(lexical);
		g.appendChild(line(wtot/2, nh, wtot/2, nh + DAUGHTER_VSPACE - 1));
	}
	g.mywidth = wtot;
	return g;
}

function show_trees()
{
	var total_h = 0;
	var max_w = 0;
	while(svg.firstChild)svg.removeChild(svg.firstChild);

	var availWidth = 400, availHeight = 400;

	if(message.trees.length > 0)
	{
		var host = document.getElementById("disc-scroller");
		host.appendChild(svg);
		availWidth = host.clientWidth - 18;
		availHeight = host.clientHeight - 18;
	}
	else if(svg.parentNode)svg.parentNode.removeChild(svg);

	var myg = element("g");
	svg.appendChild(myg);

	//for(var x in message.trees)
	if(message.trees.length > 0)
	{
	x = 0
	{
		var	t = render_tree(message.trees[x]);
		t.setAttribute("y", total_h);
		myg.appendChild(t);
		var b = t.getBBox();
		total_h += b.height;
		if(b.width > max_w)max_w = b.width;
	}
	}

	var scale = availWidth / max_w;
	if(availHeight / total_h < scale)scale = availHeight / total_h;
	if(scale > 1.0)scale = 1.0;
	myg.setAttribute("transform", "scale("+scale+")");
	//svg.style.width = availWidth + "px";
	//svg.style.height = availHeight + "px";
	//svg.setAttribute("style", "width: " + availWidth + "px; height: " + availHeight + "px;");
	svg.setAttribute("width", max_w);
	svg.setAttribute("height", total_h);

	var ds = document.getElementById("disc-scroller");
	var ov = document.getElementById("overlay");

	svg.onclick = function()
	{
		if(svg.parentNode == ds)
		{
			ov.appendChild(svg);
			ov.style.display = "block";
			myg.setAttribute("transform", "scale(1)");
			//svg.style.width = max_w + "px";
			//svg.style.height = total_h + "px";
			//svg.setAttribute("style", "width: " + max_w + "px; height: " + total_h + "px;");
		}
		else
		{
			ds.appendChild(svg);
			ov.style.display = "none";
			myg.setAttribute("transform", "scale("+scale+")");
			//svg.style.width = availWidth + "px";
			//svg.style.height = availHeight + "px";
			//svg.setAttribute("style", "width: " + availWidth + "px; height: " + availHeight + "px;");
		}
	}

	var html = message.ntrees + " remaining.";
	if(message.oldgold)
		html += " Gold tree is " + (message.oldgold.in?"in":"out") + ".";
	document.getElementById("counters").innerHTML = html;
}

var hilight_set = [];
var	first_hi;

function dehilight()
{
	for(var x=0;x<hilight_set.length;x++)
		hilight_set[x].style.color = "";
	hilight_set = [];
}

function hilight(x)
{
	dehilight();
	if(x.tokFrom >= first_hi.tokTo)
	{
		var w;
		for(w = first_hi;w!=x.nextSibling && w;w=w.nextSibling)
		{
			w.style.color = "red";
			hilight_set.push(w);
		}
		for(w = x.nextSibling;w && bars[w.tokFrom * 2];w = w.nextSibling)
		{
			w.style.color = "red";
			hilight_set.push(w);
		}
		for(w = first_hi.previousSibling;w && bars[w.tokTo * 2];w = w.previousSibling)
		{
			w.style.color = "red";
			hilight_set.push(w);
		}
	}
	else
	{
		var w;
		for(w = x;w && w!=first_hi.nextSibling;w=w.nextSibling)
		{
			w.style.color = "red";
			hilight_set.push(w);
		}
		for(w = first_hi.nextSibling;w && bars[w.tokFrom * 2];w = w.nextSibling)
		{
			w.style.color = "red";
			hilight_set.push(w);
		}
		for(w = x.previousSibling;w && bars[w.tokTo * 2];w = w.previousSibling)
		{
			w.style.color = "red";
			hilight_set.push(w);
		}
	}
}

function cancel_hilight()
{
	if(first_hi)
	{
		dehilight();
		first_hi = null;
	}
}

function no_hilight()
{
	dehilight();
	first_hi = null;
	dspan = "";
	dspanfrom = dspanto = null;
	refilter();
}

function begin_hilight_words(x)
{
	dehilight();
	first_hi = x;
	hilight(x);
}

function drag_hilight_words(x)
{
	if(first_hi)
		hilight(x);
}

var dspanto;
var dspanfrom;
var dspan = "";

function end_hilight_words(x)
{
	var from = 9999, to = 0;
	first_hi = null;
	for(var x in hilight_set)
	{
		var wf = hilight_set[x].tokFrom;
		var wt = hilight_set[x].tokTo;
		if(wf<from)from=wf;
		if(wt>to)to=wt;
	}
	//alert("words " +from+ " to " +to+ " inclusive.");
	if(from != to)
	{
		//decisions.push({type:"span",from:from,to:to});
		dspan = from + ":" + to;
		dspanfrom = from;
		dspanto = to;
		refilter();
	}
}

function show_sentence()
{
	//var words = message.item.split(" ");
	var tokens = message.tokens.sort(
		function(x,y) { return x.from - y.from; } );
	var chunks = message.chunks;
	var sentence = document.getElementById("sentence");

	hilight_set = [];

	var ci = 0;
	bars = [];
	for(var c in chunks)
	{
		for(var i=chunks[c].from;i<chunks[c].to;i=i+1)
		{
			bars[i] = chunks[c].state;
		}
	}

	while(sentence.firstChild)sentence.removeChild(sentence.firstChild);
	for(var x in tokens)
	{
		var d = document.createElement("div");
		d.style.cssFloat = "left";
		d.style.margin = "0px 0px 0px 0px";
		var t = document.createElement("table");
		d.appendChild(t);
		var b = document.createElement("tbody");
		t.style.margin = "0px 0px 0px 0px";
		t.style.padding = "0px 0px 0px 0px";
		t.style.borderCollapse = "collapse";
		t.style.border = "none";
		t.appendChild(b);
		sentence.appendChild(d);

		// text row
		var r = document.createElement("tr");
		b.appendChild(r);
		var cell = document.createElement("td");
		r.appendChild(cell);
		var cell = document.createElement("td");
		r.appendChild(cell);
		cell.appendChild(document.createTextNode(tokens[x].text));

		// green bar row
		r = document.createElement("tr");
		b.appendChild(r);
		cell = document.createElement("td");
		B = bars[ci]; ci = ci+1;
		cell.style.background = B?"green":"none";
		cell.style.paddingLeft = "5px";
		r.appendChild(cell);
		cell = document.createElement("td");
		B = bars[ci]; ci = ci+1;
		cell.style.background = B?"green":"none";
		r.appendChild(cell);

		d.tokFrom = tokens[x].from;
		d.tokTo = tokens[x].to;
		if(dspanto)
		{
			if(d.tokFrom >= dspanfrom && d.tokTo <= dspanto)
			{
				hilight_set.push(d);
				d.style.color = "red";
			}
		}
		var mobile = (/iphone|ipad|ipod|android|blackberry|mini|windows\sce|palm/i.test(navigator.userAgent.toLowerCase()));  
		if(mobile)
		{
			d.addEventListener("touchstart", function(x){return function(ev){ev.preventDefault();begin_hilight_words(x);}}(d), false);
			d.addEventListener("touchmove", function(ev){
				var x=document.elementFromPoint(ev.touches[0].clientX, ev.touches[0].clientY);
				while(x && x.nodeName.toLowerCase() != "div")x = x.parentNode;
				ev.preventDefault();if(x)drag_hilight_words(x);
				}, false);
			d.addEventListener("touchend", function(x){return function(ev){ev.preventDefault();end_hilight_words(x);}}(d), false);
		}
		else
		{
			d.onmousedown = function(x){return function(ev){ev.preventDefault();begin_hilight_words(x);if(ev.stopPropagaton)ev.stopPropagation(); else ev.cancelBubble = true;}}(d);
			d.onmousemove = function(x){return function(ev){ev.preventDefault();drag_hilight_words(x);if(ev.stopPropagaton)ev.stopPropagation(); else ev.cancelBubble = true;}}(d);
			d.addEventListener("mouseup", function(x){return function(ev){ev.preventDefault();end_hilight_words(x);if(ev.stopPropagaton)ev.stopPropagation(); else ev.cancelBubble = true;}}(d), false);
		}
		d.addEventListener("click", function(x){return function(ev){ev.preventDefault();if(ev.stopPropagaton)ev.stopPropagation(); else ev.cancelBubble = true;}}(d), false);
	}
	window.onmouseup = cancel_hilight;
	document.getElementById("sentence").onclick = no_hilight;
	document.getElementById("counters").onclick = no_hilight;
}

function toggle_old()
{
	use_old_decs = !use_old_decs;
	refilter();
}

function nixdecision(x)
{
	decisions.splice(x,1);
	refilter();
}

function show_decisions()
{
	var d = document.getElementById("decisions");
	d.innerHTML = "";
	for(var x in decisions)
	{
		var dec = decisions[x];
		var nixer = " <a href='javascript:nixdecision(" + x + ");'>[x]</a>";
		d.innerHTML += dec.type + ": " + dec.from + " to " + dec.to + nixer + "<br/>";
	}
	if(message.olddec)
	{
		var txt = (use_old_decs?"Including ":"Excluding ") + message.olddec.length + " old decisions.";
		d.innerHTML += "<a href='javascript:toggle_old();'>" + txt + "</a>";
	}
}

function select_discriminant(d)
{
	decisions.push({type:d.sign,from:d.from,to:d.to});
	refilter();
}

function buildUnaryChain(sign)
{
	var d = document.createElement("div");
	var signs = sign.split("@");
	var first = 1;
	for(var x in signs)
	{
		if(!first)
		{
			var n = document.createElement("div");
			n.appendChild(document.createTextNode("|"));
			d.appendChild(n);
		}
		first = 0;
		var n = document.createElement("div");
		n.appendChild(document.createTextNode(signs[x]));
		d.appendChild(n);
	}
	return d;
}

function getYield(from, to)
{
	var str = "";
	var tokens = message.tokens.sort(
		function(x,y) { return x.from - y.from; } );
	for(var x in tokens)
	{
		var t = tokens[x];
		if(t.from >= from && t.to <= to)
		{
			if(str != "")
				str = str + " ";
			str = str + t.text;
		}
	}
	return str;
}

function buildYield(from, to)
{
	var y = document.createElement("div");
	y.style.marginTop = "10px";
	y.style.color = "darkred";
	y.appendChild(document.createTextNode(getYield(from, to)));
	return y;
}

function show_discriminants()
{
	var discs = message.discriminants.sort(
		function(x,y) { return (x.to - x.from) - (y.to - y.from); } );
	var div = document.getElementById("disc-scroller");
	while(div.firstChild)div.removeChild(div.firstChild);
	for(var x in discs)
	{
		var dd = document.createElement("div");
		dd.style.cssFloat = "left";
		dd.style.margin = "10px";
		dd.style.padding = "10px";
		dd.style.border = "1px solid #c94";
		dd.style.background = "#ccf";
		dd.style.textAlign = "center";
		dd.appendChild(buildUnaryChain(discs[x].sign));
		if(dspan == "" || (dspanto-dspanfrom < 2))
		{
			var yield = buildYield(discs[x].from, discs[x].to);
			dd.appendChild(yield);
		}
		var count = document.createElement("div");
		count.style.marginTop = "10px";
		count.style.fontWeight = "bold";
		count.style.color = "darkgreen";
		count.appendChild(document.createTextNode(discs[x].count + " trees"));
		dd.appendChild(count);
		dd.discriminant = discs[x];
		dd.onclick = function(x){return function(ev){select_discriminant(x.discriminant);}}(dd);
		div.appendChild(dd);
	}
}
