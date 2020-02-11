var svg = element("svg");
svg.setAttribute("version", "1.1");
document.getElementById("disc-scroller").appendChild(svg);
svg.style.background = "white";

var mrsbutton = document.createElement("button");
mrsbutton.innerHTML="[Show MRS]";
mrsbutton.onclick = show_mrs;
//document.getElementById("disc-scroller").appendChild(mrsbutton);

//var containerTag = "svg";	// results in sloooow rendering
var containerTag = "g";	// results in fast rendering!

function element(type)
{
	return document.createElementNS("http://www.w3.org/2000/svg", type);
}

function container()
{
	var g = element(containerTag);
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
	var g = element(containerTag);
	g.appendChild(y);
	g.mywidth = y.bbx.width;
	return g;
}

function render_tree(t)
{
	var	dtrs = [];
	var wtot = -DAUGHTER_HSPACE;
	var dtr_label_mean = 0;
	for(var x in t.daughters)
	{
		wtot += DAUGHTER_HSPACE;
		dtrs[x] = render_tree(t.daughters[x]);
		dtr_label_mean += wtot + dtrs[x].labelcenter;
		wtot += dtrs[x].mywidth;
	}
	var lexical;
	if(dtrs.length)
	{
		dtr_label_mean /= dtrs.length;
	}
	else
	{
		lexical = render_yield(getYield(t.from, t.to));
		wtot = lexical.mywidth;
		dtr_label_mean = wtot / 2;
	}
	var dtrs_wtot = wtot;

	var g = element(containerTag);
	var n = text(t.shortlabel);
	var nw = n.width;
	var nh = n.height;
	nw = n.bbx.width;
	nh = n.bbx.height;
	var labelcenter = dtr_label_mean;
	if(labelcenter - nw/2 < 0)labelcenter = nw/2;
	if(labelcenter + nw/2 > wtot)labelcenter = wtot - nw/2;
	if(nw > wtot) { wtot = nw; labelcenter = wtot / 2; }
	n.setAttributeNS(null, "x", labelcenter - nw / 2);
	n.setAttributeNS(null, "y", nh * 2/3);
	g.appendChild(n);
	var	dtr_x = wtot / 2 - dtrs_wtot / 2;
	var ytrans = nh + DAUGHTER_VSPACE;
	for(var x in dtrs)
	{
		dtrs[x].setAttributeNS(null, "transform", "translate(" + dtr_x + "," + ytrans + ")");
		g.appendChild(line(labelcenter, nh, dtr_x + dtrs[x].labelcenter, nh + DAUGHTER_VSPACE - 1));
		g.appendChild(dtrs[x]);
		dtr_x += dtrs[x].mywidth + DAUGHTER_HSPACE;
	}
	if(lexical)
	{
		lexical.setAttributeNS(null, "transform", "translate(" + dtr_x + "," + ytrans + ")");
		g.appendChild(line(labelcenter, nh, wtot/2, nh + DAUGHTER_VSPACE - 1));
		g.appendChild(lexical);
	}
	g.mywidth = wtot;
	g.labelcenter = labelcenter;
	g.labelheight = nh;
	t.mainsvg = g;
	t.labelsvg = n;

	return g;
}

var popup_to_stop = null;
function stop_popups()
{
	if(popup_to_stop)
		popup_to_stop.setAttributeNS(null, "style", "visibility: hidden;");
	popup_to_stop = null;
}

function render_tree_popups(t, isleft, isright, istop)
{
	var dtrs_wtot = -DAUGHTER_HSPACE;
	var	dtrs = [];
	for(var x in t.daughters)
	{
		dtrs[x] = render_tree_popups(t.daughters[x], (x>0)?0:isleft, (x<(t.daughters.length-1))?0:isright, 0);
		dtrs_wtot += t.daughters[x].mainsvg.mywidth + DAUGHTER_HSPACE;
	}

	var maing = t.mainsvg;
	var labelheight = maing.labelheight;
	var labelcenter = maing.labelcenter;
	var wtot = maing.mywidth;

	var g = element(containerTag);
	g.setAttributeNS(null, "width", wtot);

	var	dtr_x = wtot / 2 - dtrs_wtot / 2;
	var ytrans = labelheight + DAUGHTER_VSPACE;
	for(var x in dtrs)
	{
		dtrs[x].setAttributeNS(null, "transform", "translate(" + dtr_x + "," + ytrans + ")");
		dtr_x += dtrs[x].mywidth + DAUGHTER_HSPACE;
		g.appendChild(dtrs[x]);
	}


	var altlabel = text(t.label);
	var altwidth = altlabel.bbx.width;
	var altheight = altlabel.bbx.height;
	var boxwidth = altwidth + 10;
	var boxheight = altheight + 10;

	var bg = element("rect");
	bg.setAttributeNS(null, "style", "fill: white; stroke: black; stroke-width: 2px;");
	bg.setAttributeNS(null, "width", boxwidth);
	bg.setAttributeNS(null, "height", boxheight);
	altlabel.setAttributeNS(null, "x", 5);
	altlabel.setAttributeNS(null, "y", boxheight - 5 - 5);

	var galt = container(bg, altlabel);
	var galtx = (labelcenter - boxwidth/2);
	var galty = -5;
	if(isleft && galtx<1)galtx = 1;
	if(isright && galtx + boxwidth > wtot-1)galtx = wtot-1 - boxwidth;
	if(istop)galty = 1;
	galt.setAttributeNS(null, "transform", "translate(" + galtx + "," + galty + ")");
	galt.setAttributeNS(null, "style", "visibility: hidden;");
	g.appendChild(galt);

	g.mywidth = wtot;

	t.labelsvg.onmouseover = function() { stop_popups(); galt.setAttributeNS(null, "style", "visibility: visible;"); popup_to_stop = galt; }
	galt.onmouseout = stop_popups;//function() { stop_popups(); }

	return g;
}

function make_var_str(m,idx)
{
	var v = m.vars[idx]
	if(typeof(v) == typeof(""))return v
	else return "<span class='mrsvar' onmouseover='show_mrsvar("+idx+")' onmouseout='show_mrsvar(-1)' varid='"+idx+"'>" + v.type + idx + "</span>"
}

mrsdiv = ""

function hilight_mrsvar(d, idx, color)
{
	var vid=d.getAttribute("varid")
	if(vid)
	{
		if(vid==idx || idx==-1)
			d.style.color = color
	}
	else for(var xi in d.childNodes)
	{
		var x = d.childNodes[xi]
		if(typeof(x)==typeof(d) && 3!=x.nodeType)
			hilight_mrsvar(x, idx, color)
	}
}

function show_mrsvar(idx)
{
	if(idx>=0)
	{
		hilight_mrsvar(mrsdiv, -1, "darkgreen")
		hilight_mrsvar(mrsdiv, idx, "red")
		var m = message.mrses[0]
		for(var hc in m.hcons)
			if(m.hcons[hc].left == idx)
				hilight_mrsvar(mrsdiv, m.hcons[hc].right, "darkred")
		var	v = m.vars[idx]
		var h = v.type + " [ "
		for(var pi in v.props)
		{
			var	p = v.props[pi].name
			var val = v.props[pi].value
			if(pi>0)h += ',  '
			h += p + ": " + val
		}
		h += " ] "
		vardiv.innerHTML = h
		vardiv.style.display = "block"
	}
	else
	{
		hilight_mrsvar(mrsdiv, -1, "darkgreen")
		vardiv.style.display="none"
	}
}

function show_mrs()
{
	var ov = document.getElementById("overlay");
	var mrs = document.createElement("div");
	var m = message.mrses[0]
	var html = ""
	html += "TOP: " + make_var_str(m, m.ltop) + "<br/>"
	html += "INDEX: " + make_var_str(m, m.index) + "<br/>"
	html += "RELS: <br/>"
	for(var r in m.rels)
	{
		var rel = m.rels[r]
		html += " " + make_var_str(m, rel.label) + ":" + rel.pred;
		html += "("
		var comma = ""
		for(a in rel.args)
		{
			html += comma + rel.args[a].name + ": " + make_var_str(m, rel.args[a].value)
			comma = ","
		}
		html += ")<br/>"
	}
	html += "<br/>HCONS:  "
	for(var h in m.hcons)
	{
		var	hc = m.hcons[h]
		if(h>0)html += ",  "
		html += make_var_str(m, hc.left)
		if(hc.type=='=q')html += " =<sub>q</sub> "
		else html += " " + hc.type + " "
		html += make_var_str(m, hc.right)
	}
	html += "<br/>ICONS:  "
	for(var h in m.icons)
	{
		var	hc = m.icons[h]
		if(h>0)html += ",  "
		html += make_var_str(m, hc.left)
		html += " " + hc.type + " "
		html += make_var_str(m, hc.right)
	}
	html += "<br/>"
	mrs.innerHTML = html
	mrsdiv = mrs
	vardiv = document.createElement("div")
	vardiv.className="varinfo"
	vardiv.style.display="none"
	ov = document.getElementById("overlay");
	ov.appendChild(mrs);
	ov.appendChild(vardiv);
	ov.style.display = "block";
	mrs.onclick = function()
	{
		ov.removeChild(mrs);
		ov.style.display = "none";
	}
}

function show_trees()
{
	var total_h = 0;
	var max_w = 0;
	while(svg.firstChild)svg.removeChild(svg.firstChild);

	var host = document.getElementById("disc-scroller");

	if(message.ntrees == 1 && hilight_set.length == 0 && !message.recons_error)
		document.getElementById("accept_button").innerHTML = "<a href='javascript:do_accept();'>accept</a>"
	else document.getElementById("accept_button").innerHTML = "<span style='color:gray;'>accept</span>"

	var availWidth = 400, availHeight = 400;

	if(message.trees.length > 0)
	{
		host.appendChild(mrsbutton);
		host.appendChild(svg);
		availWidth = host.clientWidth - 18;
		availHeight = host.clientHeight - 18;
	}
	else
	{
		if(mrsbutton.parentNode)mrsbutton.parentNode.removeChild(mrsbutton);
		if(svg.parentNode)svg.parentNode.removeChild(svg);
		if(message.recons_error)
		{
			var error_div = document.createElement("div");
			var E = message.recons_error;
			var html = E.type + "<br/>";
			if(E.has_glb)
			{
				html += "Failure path inside daughter: " + E.path + "<br/>"
				html += "Types involved: " + E.type1 + "  (incompatible with) " + E.type2 + "<br/>"
			}
			html += "<br/>"
			html += "<br/>"
			html += "Label at unbuildable tree node: " + E.node.label + "<br/>"
			html += "Span of unbuildable tree: " + E.node.from + " to " + E.node.to + "<br/>"
			hilight_dec(E.node.from, E.node.to);
			error_div.innerHTML = html;
			host.appendChild(error_div);
		}
		else unhilight_dec();
		if(message.error)
		{
			host.appendChild(document.createTextNode(message.error));
		}
	}

	var myg = element("g");
	svg.appendChild(myg);

	//for(var x in message.trees)
	if(message.trees.length > 0)
	{
		x = 0
		var	t = render_tree(message.trees[x]);
		t.setAttribute("y", total_h);
		var	popt = render_tree_popups(message.trees[x], 1, 1, 1);
		popt.setAttribute("y", total_h);
		myg.appendChild(t);
		myg.appendChild(popt);
		var b = t.getBBox();
		total_h += b.height;
		if(b.width > max_w)max_w = b.width;
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
		}
		else
		{
			ds.appendChild(svg);
			ov.style.display = "none";
			myg.setAttribute("transform", "scale("+scale+")");
		}
		stop_popups();
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
		hilight_set[x].style.color = hilight_set[x].defaultColor;
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
	else dehilight();
}

function hilight_dec(from, to)
{
	var sentence = document.getElementById("sentence");
	for(var x in sentence.childNodes)
	{
		var n = sentence.childNodes[x];
		if(n.tokFrom == n.tokTo && from != to)
		{
			// skip the spaces on the ends of the span
			if(n.tokFrom == from || n.tokFrom == to)
				continue;
		}
		if(n.tokFrom >= from && n.tokTo <= to)
			n.style.background = 'lightgreen';
	}
}

function unhilight_dec()
{
	var sentence = document.getElementById("sentence");
	for(var x in sentence.childNodes)
	{
		var n = sentence.childNodes[x];
		if(n.style)n.style.background = 'none';
	}
}

var showing_spacetext = 0;
function hide_spacetext()
{
	var sentence = document.getElementById("sentence");
	for(var x in sentence.childNodes)
	{
		var n = sentence.childNodes[x];
		if(n.spacespan)
			n.spacespan.innerHTML = "&nbsp;";
	}
	document.getElementById("st-hide").style.display="inline";
	document.getElementById("st-show").style.display="none";
	showing_spacetext = 0;
}

function show_spacetext()
{
	var sentence = document.getElementById("sentence");
	for(var x in sentence.childNodes)
	{
		var n = sentence.childNodes[x];
		if(n.spacespan)
			n.spacespan.innerHTML = n.spacetext;
	}
	document.getElementById("st-hide").style.display="none";
	document.getElementById("st-show").style.display="inline";
	showing_spacetext = 1;
}

function find_token_vertices(tokens)
{
	var verts = [];
	for(var x in tokens)
	{
		var t = tokens[x];
		if(!verts[t.from])verts[t.from] = {left:t.cfrom, right:t.cfrom};
		else verts[t.from].right = t.cfrom;
		if(!verts[t.to])verts[t.to] = {left:t.cto, right:t.cto};
		else verts[t.to].left = t.cto;
	}
	verts = verts.sort(function(x,y) { return x.left - y.left; });
	if(!verts.length)return [];
	verts[0].left = 0;
	verts[verts.length-1].right = message.item.length;
	for(var x in verts)
	{
		var word = message.item.substring(verts[x].left, verts[x].right);
		verts[x].text = word;
		verts[x].id = parseInt(x);
	}
	return verts;
}

function renderable_span(text, from, to, has_bar, is_space)
{
	var d = document.createElement("div");
	d.defaultColor = is_space?"#aaa":"black";
	d.style.cssFloat = "left";
	d.style.margin = "0px 0px 0px 0px";
	d.style.color = d.defaultColor;
	var t = document.createElement("table");
	d.appendChild(t);
	var b = document.createElement("tbody");
	t.style.margin = "0px 0px 0px 0px";
	t.style.padding = "0px 0px 0px 0px";
	t.style.borderCollapse = "collapse";
	t.style.border = "none";
	t.appendChild(b);

	// text row
	var r = document.createElement("tr");
	b.appendChild(r);
	var cell = document.createElement("td");
	r.appendChild(cell);
	//cell.appendChild(document.createTextNode(text));
	var span = document.createElement("span");
	text = text.replace(/&nbsp;/, " ").replace(/&/,'&amp;').replace(/</,"&lt;").replace(/>/,"&gt;").replace(/ /,"&nbsp;");
	if(is_space)
	{
		d.spacespan = span;
		d.spacetext = text;
		span.innerHTML = showing_spacetext?text:"&nbsp;";
	}
	else span.innerHTML = text;
	cell.appendChild(span);

	// green bar row
	r = document.createElement("tr");
	b.appendChild(r);
	cell = document.createElement("td");
	cell.style.background = (has_bar==1)?"green":((has_bar==2)?"red":"none");
	//cell.style.paddingLeft = "5px";
	r.appendChild(cell);

	d.tokFrom = from;
	d.tokTo = to;

	if(dspanto)
	{
		var hit = 0;
		if(d.tokFrom >= dspanfrom && d.tokTo <= dspanto &&
			!(is_space && ((d.tokFrom == dspanfrom && d.tokTo == dspanfrom) || (d.tokFrom == dspanto && d.tokTo == dspanto))))
		{
			hilight_set.push(d);
			d.style.color = "red";
			hit = 1;
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
		d.onmousedown = function(x){return function(ev){ev.preventDefault();begin_hilight_words(x);if(ev.stopPropagation)ev.stopPropagation(); else ev.cancelBubble = true;}}(d);
		d.onmousemove = function(x){return function(ev){ev.preventDefault();drag_hilight_words(x);if(ev.stopPropagation)ev.stopPropagation(); else ev.cancelBubble = true;}}(d);
		d.addEventListener("mouseup", function(x){return function(ev){ev.preventDefault();end_hilight_words(x);if(ev.stopPropagation)ev.stopPropagation(); else ev.cancelBubble = true;}}(d), false);
	}
	d.addEventListener("click", function(x){return function(ev){ev.preventDefault();if(ev.stopPropagation)ev.stopPropagation(); else ev.cancelBubble = true;}}(d), false);

	document.getElementById("st-hide").addEventListener("click", function(x){return function(ev){ev.preventDefault();if(ev.stopPropagation)ev.stopPropagation(); else ev.cancelBubble = true;show_spacetext();}}(d), false);
	document.getElementById("st-show").addEventListener("click", function(x){return function(ev){ev.preventDefault();if(ev.stopPropagation)ev.stopPropagation(); else ev.cancelBubble = true;hide_spacetext();}}(d), false);

	return d;
}

function show_sentence()
{
	var sentence = document.getElementById("sentence");

	if(!message.tokens.length)
	{
		// failed to parse
		sentence.innerHTML = "<span style='color:red;'>Failed to parse: </span>";
		sentence.appendChild(document.createTextNode(message.item));
		return;
	}

	//var words = message.item.split(" ");
	var tokens = message.tokens.sort(
		function(x,y) { return x.from - y.from; } );
	var chunks = message.chunks;

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

	var vertices = find_token_vertices(message.tokens);
	message.vertices = vertices;
	var last_vert = null;
	var B = 0;
	for(var x in vertices)
	{
		var vert = vertices[x];
		if(last_vert)
		{
			var word = message.item.substring(last_vert.right, vert.left);
			sentence.appendChild(renderable_span(word, last_vert.id, vert.id, bars[B++], 0));
		}
		last_vert = vert;
		//sentence.appendChild(renderable_span(vert.text + "&nbsp;", vert.id, vert.id, bars[B++], 1));
		var sptext = vert.text.replace(/ /,"&nbsp;");
		if(sptext=="")sptext="&nbsp;";	// need something there or the greenline will go in the wrong place...
		sentence.appendChild(renderable_span(sptext, vert.id, vert.id, bars[B++], 1));
	}

	window.onmouseup = cancel_hilight;
	//document.getElementById("yellow").addEventListener("click", no_hilight, false);
	document.getElementById("yellow").addEventListener("mouseup", no_hilight, false);
}

/*function save_decisions()
{
	status = -1
	if(message.item_status == "accepted")
		status = 1
	if(message.item_status == "rejected")
		status = 0
	do_save(status, function(){});
}*/

function nixdecision(dec)
{
	for(var x in decisions)
	{
		if(decisions[x] == dec)
		{
			decisions.splice(x,1);
			refilter();
			return;
		}
	}
}

function dec_is_manual(d) { return d.inferred?0:1; }
function dec_is_inferred(d) { return d.inferred?1:0; }

function render_decision(dec, nixertype)
{
	var d = document.createElement("tr");

	var tdsign = document.createElement("td");
	var wrapable = dec.sign.replace(/@/g, " @");
	tdsign.appendChild(document.createTextNode(wrapable.toLowerCase()));
	tdsign.style.paddingRight = "10px";
	d.appendChild(tdsign);

	var tdtype = document.createElement("td");
	tdtype.appendChild(document.createTextNode(dec.type));
	tdtype.style.width = "30px";
	d.appendChild(tdtype);

	var tdspan = document.createElement("td");
	tdspan.appendChild(document.createTextNode(dec.from + " to " + dec.to + " "));
	d.appendChild(tdspan);

	var nixer = document.createElement("td");
	if(nixertype == "nixer")
	{
		nixer.innerHTML = "[x]";
		nixer.style.color = "blue";
		nixer.style.textDecoration = "underline";
		nixer.style.cursor = "pointer";
		nixer.onclick = function() { nixdecision(dec); }
	}
	else
	{
		var on = document.createElement("span"); on.innerHTML = "on";
		var off = document.createElement("span"); off.innerHTML = "off";
		nixer.appendChild(document.createTextNode("["));
		nixer.appendChild(on);
		nixer.appendChild(document.createTextNode("|"));
		nixer.appendChild(off);
		nixer.appendChild(document.createTextNode("]"));
		if(dec.enabled)
		{
			off.style.color="blue";
			off.style.textDecoration = "underline";
			off.style.cursor = "pointer";
			off.onclick = function() { dec.enabled = 0; refilter(); }
		}
		else
		{
			on.style.color="blue";
			on.style.textDecoration = "underline";
			on.style.cursor = "pointer";
			on.onclick = function() { dec.enabled = 1; refilter(); }
		}
	}
	d.appendChild(nixer);
	d.onmouseover = function() { hilight_dec(dec.from, dec.to); d.style.background = "#acf"; }
	d.onmouseout = function() { unhilight_dec(); d.style.background = ""; }
	return d;
}

function render_decisions(label, color, decs, nixertype)
{
	var div = document.createElement("div");
	var title = document.createElement("div");
	var tspan = document.createElement("span");
	tspan.appendChild(document.createTextNode(decs.length + " "));
	tspan.appendChild(document.createTextNode(label));
	tspan.style.color = color;
	tspan.style.fontWeight = "bold";
	title.appendChild(tspan);

	if(nixertype == "toggler")
	{
		var nixer = document.createElement("span");
		var on = document.createElement("span"); on.innerHTML = "all on";
		var off = document.createElement("span"); off.innerHTML = "all off";
		nixer.appendChild(document.createTextNode("["));
		nixer.appendChild(on);
		nixer.appendChild(document.createTextNode("|"));
		nixer.appendChild(off);
		nixer.appendChild(document.createTextNode("]"));
		var all_on = 1, all_off = 1;
		for(var x in decs) {
			if(!decs[x].enabled)all_on = 0;
			if(decs[x].enabled)all_off = 0;
		}
		if(!all_off)
		{
			off.style.color="blue";
			off.style.textDecoration = "underline";
			off.style.cursor = "pointer";
			off.onclick = function() {
				for(var x in decs)
					decs[x].enabled = 0;
				refilter();
			}
		}
		if(!all_on)
		{
			on.style.color="blue";
			on.style.textDecoration = "underline";
			on.style.cursor = "pointer";
			on.onclick = function() {
				for(var x in decs)
					decs[x].enabled = 1;
				refilter();
			}
		}
		title.appendChild(document.createTextNode(" "));
		title.appendChild(nixer);
	}

	div.appendChild(title);
	var tab = document.createElement("table");
	var tb = document.createElement("tbody");
	tab.appendChild(tb);
	div.appendChild(tab);
	for(var x in decs)
		tb.appendChild(render_decision(decs[x], nixertype));
	return div;
}

function show_decisions()
{
	if(!message.dec)message.dec = [];
	if(!message.olddec)message.olddec = [];
	decisions = message.dec;
	current_manual_decisions = message.dec.filter(dec_is_manual);
	current_manual_dec_frag = render_decisions(" new manual", "darkgreen", current_manual_decisions, "nixer");
	current_inf_decisions = message.dec.filter(dec_is_inferred);
	current_inf_dec_frag = render_decisions(" new inferred", "black", current_inf_decisions, "nixer");

	old_manual_decisions = message.olddec.filter(dec_is_manual);
	old_manual_dec_frag = render_decisions("gold manual", "darkred", old_manual_decisions, "toggler");
	old_inf_decisions = message.olddec.filter(dec_is_inferred);
	old_inf_dec_frag = render_decisions("gold inferred", "purple", old_inf_decisions, "toggler");

	var newd = document.getElementById("decisions");
	newd.innerHTML = "";
	if(current_manual_decisions.length)
		newd.appendChild(current_manual_dec_frag);
	if(current_inf_decisions.length)
		newd.appendChild(current_inf_dec_frag);
	if(old_manual_decisions.length)
		newd.appendChild(old_manual_dec_frag);
	if(old_inf_decisions.length)
		newd.appendChild(old_inf_dec_frag);

	document.getElementById("comment").value = message.comment;
}

function select_discriminant(d)
{
	if(d.type)decisions.push({type:d.type,sign:d.sign,from:d.from,to:d.to,inferred:0,lex:d.lex});
	else decisions.push({type:'=',sign:d.sign,from:d.from,to:d.to,inferred:0,lex:d.lex});
	refilter();
}

function open_window(url)
{
	window.open(url, '_blank');
}

function open_lextype_docs(event, lextype)
{
	if(event.shiftKey)
	{
		if(event.stopPropagation)event.stopPropagation();
		else event.cancelBubble = true;
		event.preventDefault();
		return open_window("http://lingo.stanford.edu/~danf/cgi-bin/ERG_1212/description.cgi?type=" + lextype);
	}
}

function open_rule_docs(event, rule)
{
	if(event.shiftKey)
	{
		if(event.stopPropagation)event.stopPropagation();
		else event.cancelBubble = true;
		event.preventDefault();
		return open_window("http://moin.delph-in.net/ErgRules");
	}
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
		if(signs[x].substring(signs[x].length-3) == "_le")
			n.onclick = function(s) { return function(ev)
				{ open_lextype_docs(ev, s); }; }(signs[x]);
		else n.onclick = function(s) { return function(ev)
				{ open_rule_docs(ev, s); }; }(signs[x]);
		d.appendChild(n);
	}
	return d;
}

function getYield(from, to)
{
	var str = "";
	var cfrom = message.vertices[from].right;
	var cto = message.vertices[to].left;
	return message.item.substring(cfrom, cto);

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

disctype = 'exactly'

function constituent_discriminants()
{
	var discs = []
	var cons = {}
	for(var c in message.discriminants)
	{
		var d = message.discriminants[c]
		sp = d.from + "-" + d.to
		if(!cons[sp])cons[sp] = 0
		cons[sp] += d.count
	}
	for(var sp in cons)
	{
		if(cons[sp] < message.ntrees)
		{
			from=parseInt(sp.split("-")[0])
			to=parseInt(sp.split("-")[1])
			discs.push({from:from, to:to, type: "=", sign: "span", count: cons[sp], lex:0})
		}
	}
	return discs
}

function present_discriminants()
{
	var discs = []
	var cons = {}
	for(var c in message.discriminants)
	{
		var d = message.discriminants[c]
		var rules = d.sign.split("@")
		var uniquerules = {}
		for(var ri in rules)uniquerules[rules[ri]]=1
		for(r in uniquerules)
		{
			sp = d.from + "-" + d.to + "@" + r
			if(!cons[sp])cons[sp] = 0
			cons[sp] += d.count
		}
	}
	for(var sp in cons)
	{
		if(cons[sp] < message.ntrees)
		{
			from=parseInt(sp.split("-")[0])
			to=parseInt(sp.split("-")[1])
			r=sp.split("@")[1]
			discs.push({from:from, to:to, type: "+", sign: r, count: cons[sp], lex:0})
		}
	}
	return discs
}

function absent_discriminants()
{
	var discs = []
	var cons = {}
	for(var c in message.discriminants)
	{
		var d = message.discriminants[c]
		var rules = d.sign.split("@")
		var uniquerules = {}
		for(var ri in rules)uniquerules[rules[ri]]=1
		for(r in uniquerules)
		{
			sp = d.from + "-" + d.to + "@" + r
			if(!cons[sp])cons[sp] = 0
			cons[sp] += d.count
		}
	}
	for(var sp in cons)
	{
		if(cons[sp] < message.ntrees)
		{
			from=parseInt(sp.split("-")[0])
			to=parseInt(sp.split("-")[1])
			r=sp.split("@")[1]
			discs.push({from:from, to:to, type: "-", sign: r, count: message.ntrees - cons[sp]})
		}
	}
	return discs
}

function show_discriminants()
{
	var	dtp = document.getElementById("disctypepicker")
	if(dtp)disctype = dtp.options[dtp.selectedIndex].value
	var pool = message.discriminants
	if(disctype=='constituent')pool = constituent_discriminants()
	else if(disctype=='present')pool = present_discriminants()
	else if(disctype=='absent')pool = absent_discriminants()
	var discs = pool.sort(
		function(x,y) { return (x.to - x.from) - (y.to - y.from); } );
	var div = document.getElementById("disc-scroller");

	div.innerHTML = "";	// faster
	//while(div.firstChild)div.removeChild(div.firstChild);

	if(message.recons_error)return;

	var showyield = (dspan == "" || (dspanto-dspanfrom < 2))?1:0;

	var frag = document.createDocumentFragment();
	// add them to 'frag', and then all-at-once to 'div' to avoid excessive layout costs

	if(message.trees.length == 0)
	{
		selecthtml = "<select id='disctypepicker' onchange='show_discriminants()'>"
		selecthtml += "<option value='constituent'" + (disctype=='constituent'?" selected":"") + ">Is A Constituent</option>"
		selecthtml += "<option value='exactly'" + (disctype=='exactly'?" selected":"") + ">Exact Unary Chain</option>"
		selecthtml += "<option value='present'" + (disctype=='present'?" selected":"") + ">Using Rule X</option>"
		selecthtml += "<option value='absent'" + (disctype=='absent'?" selected":"") + ">Not Using Rule</option>"
		selecthtml += "</select>"
		div.innerHTML += selecthtml
	}

	for(var x in discs)
	{
		var disc = discs[x];
		var dd = document.createElement("div");
		dd.className = "discrim";
		var uc = buildUnaryChain(disc.sign)
		dd.appendChild(uc);
		if(showyield)
		{
			var yield = buildYield(disc.from, disc.to);
			dd.appendChild(yield);
		}
		var count = document.createElement("div");
		count.className = "count";
		count.appendChild(document.createTextNode(disc.count + " trees"));
		dd.appendChild(count);
		//dd.appendChild(document.createTextNode(disc.type))
		dd.discriminant = disc;
		dd.onclick = function(x){return function(ev){
			unhilight_dec(); select_discriminant(x.discriminant);}}(dd);
		if(dspan == "")
		{
			dd.onmouseover = function(disc){return function(ev){
				hilight_dec(disc.from, disc.to);}}(disc);
			dd.onmouseout = unhilight_dec;
		}
		frag.appendChild(dd);
	}
	div.appendChild(frag);
}

function show_item_and_profile_id()
{
	document.getElementById("profile-id").innerHTML = message.profile_id;
	document.getElementById("item-id").innerHTML = message.item_id + " -- " + message.item_status;
	//document.getElementById("nextlink").href = "next" + window.location.search;
	//document.getElementById("prevlink").href = "prev" + window.location.search;
	document.getElementById("listlink").href = "/private" + message.profile_id;
}
