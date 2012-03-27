#define	_GNU_SOURCE
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<assert.h>
#include	<signal.h>
#include	<sys/types.h>
#include	<dirent.h>
#include	<wchar.h>
#include	<locale.h>

#include	<liba.h>
#include	<tsdb.h>

#include	<ace/lexicon.h>
#include	<ace/hash.h>

#include	"tree.h"
#include	"treebank.h"

char	*tsdb_home_path = "/home/sweaglesw/logon/lingo/lkb/src/tsdb/home/";

void	html_headers(FILE	*f, char	*title)
{
	http_headers(f, "text/html");
	fprintf(f, "<html><head><title>%s</title></head>\n", title);
	fprintf(f, "<body><center><h1>%s</h1></center>\n", title);
}

void	html_footers(FILE	*f)
{
	fprintf(f, "</body></html>\n");
	fclose(f);
}

int	id_allocator;
int				nsessions;
struct session	**sessions;

struct session	*get_session(int	id)
{
	int	i;
	for(i=0;i<nsessions;i++)
		if(sessions[i]->id == id)return sessions[i];
	return NULL;
}

int	labelIsPresent(char	*label, char	*uchain)
{
	int	l = strlen(label);
	char	*s;

	again:
	s = strcasestr(uchain, label);
	if(s)
	{
		if((s==uchain || s[-1]=='@') && (s[l]==0 || s[l]=='@'))
			return 1;
		uchain = s+1;	// see if it appears again
		goto again;
	}
	return 0;
}

int	incompatible(int	from, int	to, char	*uc_label, struct constraint	c)
{
	if(c.from == from && c.to == to)
	{
		// same span
		if(c.type == constraintIsAConstituent)
			return 0; // this span is a constituent; that's all c wants
		if(c.type == constraintExactly)
			return strcasecmp(c.sign, uc_label)?1:0;	// right or wrong label
		if(c.type == constraintPresent)
			return labelIsPresent(c.sign, uc_label)?0:1;	// the label is there or not
		if(c.type == constraintAbsent)
			return labelIsPresent(c.sign, uc_label)?1:0;	// the label is there or not
	}
	if(c.from >= from && c.to <= to)
	{
		// c strictly contained in e
		return 0;	// can still be satisfied
	}
	if(c.from <= from && c.to >= to)
	{
		// e strictly contained in c
		return 0;	// if it is unsatisfied above us in the tree and we didn't notice, then there is a crossing bracket somewhere that we will notice
	}
	if(c.to <= from)return 0;	// c strictly before e
	if(c.from >= to)return 0;	// c strictly after e
	// crossing brackets
	if(c.type == constraintIsAConstituent
		|| c.type == constraintExactly
		|| c.type == constraintPresent)
		return 1;
	else return 0;	// for constraintAbsent, crossing brackets may not be a concern...
}

long long	count_remaining_trees_r(struct tb_edge	*e, int	nc, struct constraint	*c)
{
	long long	n = 1;
	int	i, bad = 0;

	for(i=0;i<nc;i++)
		if(incompatible(e->from, e->to, e->sign, c[i]))bad = 1;

	if(!bad)
	{
		for(i=0;i<e->ndaughters;i++)
			n *= count_remaining_trees_r(e->daughter[i], nc, c);
		e->unpackings = n;
	}
	else n = 0;

	for(i=0;i<e->npack;i++)
		n += count_remaining_trees_r(e->pack[i], nc, c);

	return n;
}

void	count_solutions_r(struct tb_edge	*e, long long outside)
{
	// assuming 'e' is used in 'outside' unique settings,
	// compute how many solutions 'e' appears in,
	// and recurse

	//if(e->id == 1233)printf("edge #1233: count solutions outside = %lld ; unpackings = %lld\n", outside, e->unpackings);

	int i, j;
	if(e->unpackings)
	{
		e->solutions += outside * e->unpackings;
		for(i=0;i<e->ndaughters;i++)
		{
			struct tb_edge	*d = e->daughter[i];
			long long	du = d->unpackings;
			for(j=0;j<d->npack;j++)
				du += d->pack[j]->unpackings;
			// du is the total number of unpackings of this daughter,
			// including edges packed onto it.
			long long other_daughters = e->unpackings / du;
			count_solutions_r(e->daughter[i], outside * other_daughters);
		}
	}
	for(i=0;i<e->npack;i++)
		count_solutions_r(e->pack[i], outside);
}

long long	count_remaining_trees(struct parse	*P, struct constraint	*c, int	nc)
{
	//printf("counting with %d constraints\n", nc);
	int i;
	long long n = 0;
	for(i=0;i<P->nedges;i++)
		P->edges[i]->unpackings = P->edges[i]->solutions = 0;
	for(i=0;i<P->nroots;i++)
		n += count_remaining_trees_r(P->roots[i], nc, c);
	for(i=0;i<P->nroots;i++)
		count_solutions_r(P->roots[i], 1);
	return n;
}

void	compute_linkage(struct parse	*P, char	*linkage, int	max, long long N)
{
	int i, j;
	for(i=0;i<P->nedges;i++)
	{
		struct tb_edge	*e = P->edges[i];
		//printf("edge #%d had %lld solutions out of %lld\n", e->id, e->solutions, N);
		if(e->solutions == N && e->unpackings == 1)
		{
			for(j=2*e->from+1;j<2*e->to;j++)
			{
				assert(j < max);
				linkage[j] = 1;
			}
		}
	}
}

void	find_discriminants(FILE	*f, struct parse	*P, int	from, int	to, int	ntrees)
{
	int	i, j;
	int have_span = (from == -1)?0:1;
	int	nd = 0;
	struct disc
	{
		char	*sign;
		int		count;
		int		from, to;
		int	lexical;
	}	*d = NULL;

	for(i=0;i<P->nedges;i++)
	{
		struct tb_edge	*e = P->edges[i];
		if(have_span && (e->from != from || e->to != to))continue;
		//printf("discriminant: edge #%d [%d - %d] %s has %lld trees\n", e->id, e->from, e->to, e->sign, e->solutions);
		if(e->solutions == 0 || e->solutions == ntrees)continue;
		for(j=0;j<nd;j++)
			if(!strcmp(d[j].sign, e->sign) && e->from == d[j].from && e->to == d[j].to && ((e->ndaughters==0)?1:0) == d[j].lexical)break;
		if(j==nd)
		{
			nd++;
			d = realloc(d, sizeof(struct disc)*nd);
			d[nd-1].sign = e->sign;
			d[nd-1].count = 0;
			d[nd-1].from = e->from;
			d[nd-1].to = e->to;
			d[nd-1].lexical = (e->ndaughters==0)?1:0;
		}
		d[j].count += e->solutions;
		//printf("%s now has %d\n", d[j].sign, d[j].count);
	}
	for(i=0;i<nd;i++)
		if(d[i].count == ntrees)
		{
			memmove(d+i, d+i+1, sizeof(struct disc)*(--nd - i));
			i--;
		}
	for(i=0;i<nd;i++)
	{
		if(i > 0)fprintf(f, ",");
		//char	*le_typed = d[i].lexical?to_letype_sign(d[i].sign):strdup(d[i].sign);
		char *esc = qescape(d[i].sign);
		//char *esc2 = qescape(le_typed);
		fprintf(f, "{sign:\"%s\",count:%d,from:%d,to:%d}", esc, d[i].count, d[i].from, d[i].to);
		//free(esc2);
		free(esc);
		//free(le_typed);
	}
	if(d)free(d);
}

struct lexeme	*get_lex_by_name_hash(char	*name)
{
	static struct hash	*h = NULL;
	if(!h)h = hash_new("lexicon");

	struct lexeme	*l = hash_find(h, name);
	if(l)return l;

	l = get_lex_by_name(name);
	if(l) { hash_add(h, strdup(name), l); return l; }

	int i;
	for(i=0;i<ngeneric_les;i++)
		if(!strcmp(name, generic_les[i]->word))
			{ hash_add(h, strdup(name), generic_les[i]); return generic_les[i]; }

	fprintf(stderr, "unknown lexeme '%s'!\n", name);
	return NULL;
}

int	tree_satisfies_constraints(struct tree	*t, struct constraint	*c, int	nc)
{
	int	i;
	char	uc_label[1024] = "";
	if(!t->ndaughters)return 1;	// ignore gold surface/token nodes
	//strcpy(uc_label, t->label);
	//while(t->ndaughters == 1 && t->daughters[0]->ndaughters)
	while(1)
	{
		if(*uc_label)strcat(uc_label, "@");
		if(!t->daughters[0]->ndaughters)
		{
			// 't' is a lexeme
			struct lexeme	*lex = get_lex_by_name_hash(t->label);
			assert(lex);
			char	*lt = lex->lextype->name;
			strcat(uc_label, lt);
			break;
		}
		// 't' is not a lexeme
		strcat(uc_label, t->label);
		if(t->ndaughters > 1)break;	// non-unary
		t = t->daughters[0];	// unary; repeat
	}
	// 't' is the bottom of a unary chain
	for(i=0;i<nc;i++)
	{
		if(incompatible(t->tfrom, t->tto, uc_label, c[i]))
		{
			printf("gold tree node '%s' conflicts with '%s'\n", uc_label, c[i].sign);
			return 0;
		}
	}
	for(i=0;i<t->ndaughters;i++)
		if(!tree_satisfies_constraints(t->daughters[i], c, nc))
			return 0;
	return 1;
}

int	has_unpacking(struct tb_edge	*e)
{
	int	i;
	if(e->unpackings)return 1;
	for(i=0;i<e->npack;i++)
		if(e->pack[i]->unpackings)return 1;
	return 0;
}

char	*get_uclabel(char	*stack, int	k)
{
	while(k-- > 0 && stack)
	{
		stack = strchr(stack, '@');
		if(stack)stack++;
	}
	if(!stack)return NULL;
	char	*rv = strdup(stack);
	if(strchr(rv, '@'))
		*strchr(rv, '@') = 0;
	return rv;
}

void	send_tree(FILE	*f, struct tb_edge	*e, int	ucdepth)
{
	int	i;
	// 'e' and its packed edges together have exactly one unpacking amongst them
	if(!e->unpackings)
	{
		for(i=0;i<e->npack;i++)
			if(e->pack[i]->unpackings)break;
		assert(i < e->npack);
		e = e->pack[i];
	}
	assert(e->unpackings == 1);

	char	*lab = get_uclabel(e->sign, ucdepth);
	char	*esc = qescape(lab);
	char	*l2 = get_uclabel(e->sign, ucdepth+1);
	if(l2)
	{
		free(l2);
		fprintf(f, "{label: \"%s\", from:%d, to:%d, daughters: [", esc, e->from, e->to);
		send_tree(f, e, ucdepth+1);
		fprintf(f, "]}");
	}
	else
	{
		fprintf(f, "{label: \"%s\", from:%d, to:%d, daughters: [", esc, e->from, e->to);
		for(i=0;i<e->ndaughters;i++)
		{
			if(i)fprintf(f, ",");
			send_tree(f, e->daughter[i], 0);
		}
		fprintf(f, "]}");
	}
	free(esc);
	free(lab);
}

void	web_nav(FILE	*f, char	*query)
{
	assert(strlen(query) >= 6);
	int	id = atoi(query+6);
	struct session	*S = get_session(id);
	if(!S) { webreply(f, "404 no such session"); return; }
	int dir = 0;
	if(!strncmp(query, "/next?", 6))dir = +1;
	else dir = -1;

	char	fullpath[10240];
	sprintf(fullpath, "%s/%s", tsdb_home_path, S->profile_id);
	struct tsdb	*profile = load_tsdb(fullpath);
	if(!profile) { webreply(f, "500 unable to read TSDB profile"); return; }

	char	*this_id = S->item_id;

	struct relation	*items = get_relation(profile, "item");
	assert(items);
	int	item_input = get_field(items, "i-input", "string");
	int	item_id = get_field(items, "i-id", "integer");
	int i;
	for(i=0;i<items->ntuples;i++)
		if(!strcmp(items->tuples[i][item_id], this_id))break;
	if(i == items->ntuples)
	{
		tsdb_free_profile(profile);
		webreply(f, "500 couldn't find that item");
		return;
	}

	i += dir;
	if(i < 0 || i >= items->ntuples)
	{ 
		tsdb_free_profile(profile);
		webreply(f, "404 no such item");
		return;
	}

	char	*new_id = items->tuples[i][item_id];
	printf("old item id '%s' + dir %d = new item id '%s'\n", this_id, dir, new_id);
	fprintf(f, "HTTP/1.1 302 Moved\r\n");
	char	*cgi = cgiencode(S->profile_id);
	fprintf(f, "Location: /private/parse?profile=%s&id=%s\r\n\r\nRedirecting...\n",
		cgi, new_id);
	free(cgi);
	fclose(f);
	tsdb_free_profile(profile);
}

void	web_session(FILE	*f, char	*query)
{
	int	id = atoi(query);
	struct session	*S = get_session(id);
	if(!S) { webreply(f, "404 no such session"); return; }

	char	*constraints = strchr(query, ';');
	if(constraints)constraints++;

	char	*span = constraints;
	constraints = strchr(constraints, ';');
	if(constraints)constraints++;
	int	from,to, have_span = 0;
	if(2 == sscanf(span, "%d:%d;", &from, &to))
		have_span = 1;

	char	*d;
	int		nc=0;
	struct constraint	*c=NULL;
	for(d=strtok(constraints,"&");d;d=strtok(NULL,"&"))
	{
		int	from, to;
		char	sign[128];
		assert(strlen(d) < 128);
		int	type = -1;
		if(3 == sscanf(d, "%[^=]=%d-%d", sign, &from, &to))
			type = constraintExactly;
		else if(3 == sscanf(d, "%[^:]:+:%d-%d", sign, &from, &to))
			type = constraintPresent;
		else if(3 == sscanf(d, "%[^:]:-:%d-%d", sign, &from, &to))
			type = constraintAbsent;
		assert(type != -1);
		nc++;
		c = realloc(c,sizeof(*c)*nc);
		c[nc-1].sign = strcmp(sign, "span")?cgidecode(sign):NULL;
		c[nc-1].from = from;
		c[nc-1].to = to;
		c[nc-1].type = type;
		if(!strcmp(sign, "span"))
			c[nc-1].type = constraintIsAConstituent;
		printf("constraint: %s [%d-%d] type %d\n", sign, from, to, type);
	}
	fflush(stdout);

	void	send_escaped(char	*key, char	*value)
	{
		char	*esc = qescape(value);
		fprintf(f, "%s: \"%s\",\n", key, esc);
		free(esc);
	}

	http_headers(f, "text/javascript");
	fprintf(f, "{");
	send_escaped("profile_id", S->profile_id);
	send_escaped("item_id", S->item_id);
	send_escaped("item", S->input);
	long long	ntrees = count_remaining_trees(S->parse, c, nc);
	fprintf(f, "ntrees: %lld,\n", ntrees);
	fprintf(f, "tokens: [");
	int i, slen = 1;
	for(i=0;i<S->parse->ntokens;i++)
	{
		struct token	*t = S->parse->tokens[i];
		if(i)fprintf(f, ",");
		char	*foo = malloc(5 * wcslen(t->text));
		wcstombs(foo, t->text, 5 * wcslen(t->text));
		char	*esc = qescape(foo);
		free(foo);
		fprintf(f, "{from:%d,to:%d,text:\"%s\"}", t->from, t->to, esc);
		free(esc);
		if(t->to >= slen)slen = t->to+1;
	}
	fprintf(f, "],\n");
	char	linkage[slen*2+1];
	bzero(linkage, slen*2+1);
	compute_linkage(S->parse, linkage, slen*2+1, ntrees);
	fprintf(f, "chunks: [");
	int	nch = 0, l0 = 0;
	for(i=1;i<slen*2+1;i++)
	{
		if(linkage[l0] != linkage[i] || i == slen*2)
		{
			if(nch++)fprintf(f, ",");
			fprintf(f, "{from:%d,to:%d,state:%d}", l0, i, linkage[l0]);
			//printf("%d - %d   is %d\n", l0, i, linkage[l0]);
			l0 = i;
		}
	}
	fprintf(f, "],\n");
	fprintf(f, "discriminants: [");
	if(have_span)
		find_discriminants(f, S->parse, from, to, ntrees);
	else find_discriminants(f, S->parse, -1, -1, ntrees);
	fprintf(f, "],");
	if(S->pref_tree)
	{
		int	in = tree_satisfies_constraints(S->pref_tree, c, nc);
		fprintf(f, "oldgold:{in:%d},", in);
	}
	if(S->npref_dec)
	{
		fprintf(f, "olddec:[");
		for(i=0;i<S->npref_dec;i++)
		{
			if(i)fprintf(f, ",");
			struct constraint	*C = &S->pref_dec[i];
			fprintf(f, "\"%s:%c:%d-%d\"", C->sign, (C->type == constraintPresent)?'+':'-',
				C->from, C->to);
		}
		fprintf(f, "],");
	}
	if(have_span)
	{
		for(i=0;i<S->parse->nedges;i++)
		{
			struct tb_edge	*e = S->parse->edges[i];
			if(e->from == from && e->to == to && e->solutions == ntrees && e->unpackings == 1)
				break;	// this is the root of the tree that we want to send back.
		}
		if(i<S->parse->nedges)
		{
			fprintf(f, "trees: [");
			send_tree(f, S->parse->edges[i], 0);
			fprintf(f, "],");
		}
		else fprintf(f, "trees: [],");
	}
	else if(ntrees == 1)
	{
		fprintf(f, "trees: [");
		for(i=0;i<S->parse->nroots;i++)
			if(has_unpacking(S->parse->roots[i]))break;
		assert(i < S->parse->nroots);
		send_tree(f, S->parse->roots[i], 0);
		fprintf(f, "],");
	}
	else fprintf(f, "trees: [],\n");
	fprintf(f, "end:0}\n");
	fclose(f);
	/*for(i=0;i<S->parse->nedges;i++)
	{
		struct tb_edge	*e = S->parse->edges[i];
		printf("edge #%d: [%d-%d] %s  has %lld solutions with %lld unpackings\n", e->id, e->from, e->to, e->sign, e->solutions, e->unpackings);
	}
	for(i=0;i<S->parse->nroots;i++)
		printf("root #%d\n", S->parse->roots[i]->id);*/

	for(i=0;i<nc;i++)
		if(c[i].sign)free(c[i].sign);
	if(c)free(c);
}

/* system for building a 'struct parse' and initiating a session, given an input sentence */

void	add_session(struct session	*S)
{
	nsessions++;
	sessions = realloc(sessions, sizeof(S)*nsessions);
	sessions[nsessions-1] = S;
}

FILE	*invoke_ace(char	*input)
{
	char	tmpn[1024] = "/tmp/input-XXXXXX";
	int		fd = mkstemp(tmpn);
	write(fd, input, strlen(input));
	write(fd, "\n", 1);
	close(fd);
	char	command[1024];
#define	ERG_PATH	"/home/sweaglesw/cdev/ace/erg-1010.dat"
	sprintf(command, "~/cdev/ace/ace -g " ERG_PATH " -O %s -r 'root_strict root_inffrag root_informal root_frag'", tmpn);
	return popen(command, "r");
}

struct parse	*read_forest_from_ace(FILE	*p, wchar_t	*winput)
{
	struct parse	*P = calloc(sizeof(*P),1);

	struct tb_edge	*make_edge(int	id)
	{
		struct tb_edge	*e = calloc(sizeof(*e),1);
		e->id = id;
		return e;
	}

	struct tb_edge	*enqueue(struct tb_edge	***L, int	*N, struct tb_edge	*e)
	{
		(*N)++;
		(*L) = realloc((*L), sizeof(struct tb_edge*)*(*N));
		return (*L)[(*N)-1] = e;
	}

	struct tb_edge	*get_edge(int	id)
	{
		int i;
		for(i=0;i<P->nedges;i++)
			if(P->edges[i]->id == id)return P->edges[i];
		return enqueue(&P->edges, &P->nedges, make_edge(id));
	}

	void	pack_edge(struct tb_edge	*host, struct tb_edge	*pack)
	{
		enqueue(&host->pack, &host->npack, pack);
	}

	void	daughter_edge(struct tb_edge	*host, struct tb_edge	*daughter)
	{
		enqueue(&host->daughter, &host->ndaughters, daughter);
	}

	while(!feof(p))
	{
		char	line[102400];
		wchar_t	text[102400];
		if(!fgets(line, 102300, p))break;
		if(line[0]=='\n')continue;
		if(!strncmp(line, "SKIP:", 5))
		{
			pclose(p);
			return P;
		}
		int	id;
		int from, to, cfrom, cto;
		if(1 == sscanf(line, "root %d\n", &id))
		{
			struct tb_edge	*e = get_edge(id);
			P->nroots++;
			P->roots = realloc(P->roots, sizeof(struct tb_edge*)*P->nroots);
			P->roots[P->nroots-1] = e;
		}
		else if(5 == sscanf(line, "token %d %d %d %d %l[^\n]\n", &from, &to, &cfrom, &cto, text))
		{
			P->ntokens++;
			P->tokens = realloc(P->tokens, sizeof(struct token*)*P->ntokens);
			struct token	*t = calloc(sizeof(*t),1);
			P->tokens[P->ntokens-1] = t;
			t->from = from;
			t->to = to;
			t->cfrom = cfrom;
			t->cto = cto;
			t->text = wcsdup(text);//calloc(sizeof(wchar_t),cto-cfrom + 1);
			//memcpy(t->text, winput + cfrom, sizeof(wchar_t)*(cto-cfrom));
			//printf("token %d-%d [%d-%d] = '%S'\n",
			//	t->from, t->to, t->cfrom, t->cto, t->text);
		}
		else
		{
			char	*lp = line;
			if(*lp != '(')
			{
				printf("unrecognized parser reply: %s\n", line);
				assert(*lp=='(');
			}
			lp++;
			id = atoi(lp);
			lp = strchr(lp, ' ');
			assert(lp && *lp==' ');
			lp++;
			char	*sign = lp;
			lp = strchr(lp, ' ');
			assert(lp && *lp==' ');
			*lp = 0;
			lp++;
			int	from = atoi(lp);
			lp = strchr(lp, ' ');
			assert(lp && *lp==' ');
			lp++;
			int	to = atoi(lp);
			lp = strchr(lp, ' ');
			assert(lp && *lp==' ');
			lp++;
			assert(*lp=='[');
			lp++;
			struct tb_edge	*e = get_edge(id);
			e->sign = strdup(sign);
			e->from = from;
			e->to = to;
			int done = 0;
			while(*lp && *lp != ']' && !done)
			{
				char	*end = strchr(lp, ',');
				char	*end2 = strchr(lp, ']');
				if(end2 < end || end == NULL)end = end2;
				assert(end);
				if(*end==']')done = 1;
				*end = 0;
				int	pid = atoi(lp);
				struct tb_edge	*pe = get_edge(pid);
				pack_edge(e, pe);
				lp = end+1;
			}
			if(done){lp--;*lp=']';}
			assert(*lp == ']');
			lp++;
			assert(*lp == ' ');
			lp++;
			assert(*lp == '(');
			lp++;
			done = 0;
			while(*lp && *lp != ')' && !done)
			{
				char	*end = strchr(lp, ',');
				char	*end2 = strchr(lp, ')');
				if(end2 < end || end == NULL)end = end2;
				assert(end);
				if(*end==')')done=1;
				*end = 0;
				int	did = atoi(lp);
				struct tb_edge	*de = get_edge(did);
				daughter_edge(e, de);
				lp = end+1;
			}
			if(done){lp--;*lp=')';}
			assert(*lp==')');
			lp++;
			assert(*lp==')');
		}
	}
	pclose(p);
	
	return P;
}

wchar_t	*towide(char	*mbs)
{
	wchar_t	*w = malloc((strlen(mbs)+2) * sizeof(wchar_t));
	size_t	err = mbstowcs(w, mbs, strlen(mbs)+1);
	//printf("towide of '%s' = '%S'  ; err = %zd\n", mbs, w, err);
	return w;
}

struct ucl
{
	int	nchains;
	struct uc
	{
		int	n;
		struct tb_edge	**e;
	}	*chains;
};

struct ucl	*unary_closure(struct tb_edge	*e)
{
	struct ucl	*u = NULL;
	if(e->ndaughters != 1)
	{
		u = calloc(sizeof(*u),1);
		u->nchains = 1;
		u->chains = malloc(sizeof(struct uc));
		u->chains[0].n = 1;
		u->chains[0].e = malloc(sizeof(struct tb_edge*));
		u->chains[0].e[0] = e;
	}
	else
	{
		u = unary_closure(e->daughter[0]);
		int	i;
		for(i=0;i<u->nchains;i++)
		{
			struct uc	*c = &u->chains[i];
			c->n++;
			c->e = realloc(c->e, sizeof(struct tb_edge*)*c->n);
			memmove(c->e+1, c->e, sizeof(struct tb_edge*)*(c->n-1));
			c->e[0] = e;
		}
	}
	int i;
	for(i=0;i<e->npack;i++)
	{
		struct ucl	*u2 = unary_closure(e->pack[i]);
		u->nchains += u2->nchains;
		u->chains = realloc(u->chains, sizeof(struct uc)*u->nchains);
		memcpy(u->chains + u->nchains - u2->nchains, u2->chains, sizeof(struct uc)*u2->nchains);
		free(u2->chains);
		free(u2);
	}
	return u;
}

char	*chain_sign(struct uc	c)
{
	char	*sign = NULL;
	int	len = c.n;
	int	i;
	for(i=0;i<c.n;i++)
		len += strlen(c.e[i]->sign);
	sign = calloc(1,len);
	for(i=0;i<c.n;i++)
	{
		if(i)strcat(sign, "@");
		strcat(sign, c.e[i]->sign);
	}
	return sign;
}

int	same_chain(struct uc	x, struct uc	y)
{
	if(x.n != y.n)return 0;
	int i;
	for(i=0;i<x.n;i++)
		if(x.e[i] != y.e[i])return 0;
	return 1;
}

struct tb_edge	*get_chain_edge(struct parse	*Pout, struct uc	**Chains, struct uc	c);
struct tb_edge	*get_ucl_edge(struct parse	*Pout, struct uc	**Chains, struct ucl	*u)
{
	struct tb_edge	*h = get_chain_edge(Pout, Chains, u->chains[0]);
	int i;
	if(!h->npack)
	{
		// only put the rest of the chains onto the packing list once
		for(i=1;i<u->nchains;i++)
		{
			struct tb_edge	*p = get_chain_edge(Pout, Chains, u->chains[i]);
			h->npack++;
			h->pack = realloc(h->pack, sizeof(struct tb_edge*)*h->npack);
			h->pack[h->npack-1] = p;
		}
	}
	else for(i=1;i<u->nchains;i++)
		free(u->chains[i].e);
	free(u->chains);
	free(u);
	return h;
}

struct tb_edge	*get_chain_edge(struct parse	*Pout, struct uc	**Chains, struct uc	c)
{
	int i;
	for(i=0;i<Pout->nedges;i++)
		if(same_chain((*Chains)[i], c))
		{
			free(c.e);
			return Pout->edges[i];
		}
	struct tb_edge	*e = calloc(sizeof(*e),1);
	e->from = c.e[0]->from;
	e->to = c.e[0]->to;
	e->id = Pout->nedges+1;
	e->sign = chain_sign(c);
	e->ndaughters = c.e[c.n-1]->ndaughters;
	e->daughter = calloc(sizeof(struct tb_edge*),e->ndaughters);
	Pout->nedges++;
	Pout->edges = realloc(Pout->edges, sizeof(struct tb_edge*)*Pout->nedges);
	Pout->edges[Pout->nedges-1] = e;
	(*Chains) = realloc((*Chains), sizeof(struct uc)*Pout->nedges);
	(*Chains)[Pout->nedges-1] = c;
	for(i=0;i<e->ndaughters;i++)
	{
		struct ucl	*u = unary_closure(c.e[c.n-1]->daughter[i]);
		e->daughter[i] = get_ucl_edge(Pout, Chains, u);
	}
	return e;
}

struct parse	*do_unary_closure(struct parse	*Pin)
{
	struct parse	*Pout = calloc(sizeof(*Pout),1);
	int i;
	Pout->ntokens = Pin->ntokens;
	Pout->tokens = calloc(sizeof(struct token*),Pin->ntokens);
	for(i=0;i<Pin->ntokens;i++)
	{
		Pout->tokens[i] = calloc(sizeof(struct token),1);
		Pout->tokens[i]->text = wcsdup(Pin->tokens[i]->text);
		Pout->tokens[i]->from = Pin->tokens[i]->from;
		Pout->tokens[i]->to = Pin->tokens[i]->to;
		Pout->tokens[i]->cfrom = Pin->tokens[i]->cfrom;
		Pout->tokens[i]->cto = Pin->tokens[i]->cto;
	}
	struct uc	*chains = NULL;
	for(i=0;i<Pin->nroots;i++)
	{
		struct ucl	*u = unary_closure(Pin->roots[i]);
		struct tb_edge	*e = get_ucl_edge(Pout, &chains, u);
		Pout->nroots++;
		Pout->roots = realloc(Pout->roots, sizeof(struct tb_edge*)*Pout->nroots);
		Pout->roots[Pout->nroots-1] = e;
	}
	for(i=0;i<Pout->nedges;i++)
		free(chains[i].e);
	if(chains)free(chains);
	return Pout;
}

void	print_forest(struct parse	*P)
{
	int i, j;
	for(i=0;i<P->nedges;i++)
	{
		struct tb_edge	*e = P->edges[i];
		printf("edge #%d: [%d-%d] %s\n", e->id, e->from, e->to, e->sign);
		for(j=0;j<e->ndaughters;j++)printf("  #%d", e->daughter[j]->id);
		printf("\n");
		if(e->npack)
		{
			printf("   packs");
			for(j=0;j<e->npack;j++)
				printf("  #%d", e->pack[j]->id);
			printf("\n");
		}
	}
}

char	*cgiarg(char	*args, char	*arg)
{
	int	l = strlen(arg);
	char	*r = strstr(args, arg);
	if(!r)return NULL;
	char	*and = strchr(r, '&');
	if(and)*and = 0;
	char	*v = cgidecode(r + l);
	if(and)*and = '&';
	return v;
}

void	free_parse(struct parse	*P)
{
	int i;
	free(P->roots);
	for(i=0;i<P->nedges;i++)
	{
		struct tb_edge	*e = P->edges[i];
		free(e->sign);
		free(e->pack);
		free(e->daughter);
		free(e);
	}
	free(P->edges);
	for(i=0;i<P->ntokens;i++)
	{
		struct token	*t = P->tokens[i];
		free(t->text);
		free(t);
	}
	free(P->tokens);
	free(P);
}

void	web_parse(FILE	*f, char	*cgiargs)
{
	char	*path = cgiarg(cgiargs, "profile=");
	char	*id = cgiarg(cgiargs, "id=");
	assert(path); assert(id);
	assert(!strchr(path, '.'));

	char	fullpath[10240];
	sprintf(fullpath, "%s/%s", tsdb_home_path, path);
	struct tsdb	*profile = load_tsdb(fullpath);
	if(!profile) { webreply(f, "500 unable to read TSDB profile"); free(id); free(path); return; }
	struct relation	*items = get_relation(profile, "item");
	assert(items);
	int	item_input = get_field(items, "i-input", "string");
	int	item_id = get_field(items, "i-id", "integer");
	char	**item = tsdb_lookup_relation_with_key(items, item_id, id);
	assert(item);
	char	*input = strdup(item[item_input]);
	printf("item id %s -> input '%s'\n", id, input);

	struct tree	*pref_tree = NULL;
	char		*pid = NULL;

	struct relation	*parses = get_relation(profile, "parse");
	if(!parses)goto nopreftree;
	int	parse_id = get_field(parses, "parse-id", "integer");
	int	parse_i_id = get_field(parses, "i-id", "integer");
	char	**parse = tsdb_lookup_relation_with_key(parses, parse_i_id, id);
	if(!parse)goto nopreftree;
	pid = parse[parse_id];
	printf("-> parse id %s\n", pid);

	struct relation	*preferences = get_relation(profile, "preference");
	if(!preferences)goto nopreftree;
	int	pref_parse_id = get_field(preferences, "parse-id", "integer");
	int	pref_result_id = get_field(preferences, "result-id", "integer");
	char	**pref = tsdb_lookup_relation_with_key(preferences, pref_parse_id, pid);
	if(!pref)goto nopreftree;
	char	*pref_rid = pref[pref_result_id];
	printf("-> preferred result id %s\n", pref_rid);

	struct relation	*results = get_relation(profile, "result");
	if(!results)goto nopreftree;
	int	result_parse_id = get_field(results, "parse-id", "integer");
	int	result_result_id = get_field(results, "result-id", "integer");
	int	result_derivation = get_field(results, "derivation", "string");
	void result_visitor(char	**result)
	{
		if(strcmp(result[result_result_id], pref_rid))return;
		char	*copy = strdup(result[result_derivation]);
		pref_tree = string_to_tree(copy);
		free(copy);
	}
	tsdb_visit_relation_with_key(results, result_parse_id, pid, result_visitor);
	if(!pref_tree)
	{
		nopreftree:
		printf("-> no preferred tree\n", pref_tree);
	}
	else
	{
		printf("-> got preferred tree %p\n", pref_tree);
	}

	int	npref_dec = 0;
	struct constraint	*pref_dec = NULL;
	if(pid)
	{
		struct relation	*decisions = get_relation(profile, "decision");
		if(decisions)
		{
			int	decision_parse_id = get_field(decisions, "parse-id", "integer");
			int decision_state = get_field(decisions, "d-state", "integer");
			int decision_type = get_field(decisions, "d-type", "integer");
			int decision_key = get_field(decisions, "d-key", "string");
			int decision_from = get_field(decisions, "d-start", "integer");
			int decision_to = get_field(decisions, "d-end", "integer");
			void decision_visitor(char	**decision)
			{
				int	state = atoi(decision[decision_state]);
				int	type = atoi(decision[decision_type]);
				char	*key = decision[decision_key];
				int	from = atoi(decision[decision_from]);
				int	to = atoi(decision[decision_to]);
				if((type == 3 || type == 2) && (state == 1 || state == 2))
				{
					npref_dec++;
					pref_dec = realloc(pref_dec, sizeof(struct constraint)*npref_dec);
					struct constraint	*c = pref_dec + npref_dec-1;
					c->sign = strdup(key);
					c->from = from;
					c->to = to;
					c->type = (state == 1)?constraintPresent:constraintAbsent;
					printf("RECOVERED constraint: %s [%d-%d] %s\n",
						c->sign, c->from, c->to, (state == 1)?"present":"absent");
				}
				else
				{
					if(type != 3 && type != 2)
						printf("DROPPING decision of type %d state %d\n", type, state);
				}
			}
			tsdb_visit_relation_with_key(decisions, decision_parse_id, pid, decision_visitor);
		}
	}

	tsdb_free_profile(profile);

	struct timeval	start, end;
	gettimeofday(&start, NULL);
	FILE	*p = invoke_ace(input);
	if(!p) { webreply(f, "500 unable to invoke parser"); free(id); free(path); return; }

	html_headers(f, "ACE Treebanker");
	fprintf(f, "Parsing... \n"); fflush(f);
	wchar_t	*winput = towide(input);
	printf("wide input = '%S'\n", winput);
	struct parse	*P = read_forest_from_ace(p, winput);
	free(winput);
	fprintf(f, "done (%d edges connected to %d roots).<br/>\n", P->nedges, P->nroots);
	gettimeofday(&end, NULL);
	double	dt = (end.tv_usec - start.tv_usec) * 0.000001 + (end.tv_sec - start.tv_sec);

	//printf("BEFORE unary closure:\n");
	//print_forest(P);

	struct parse	*newP = do_unary_closure(P);
	free_parse(P);
	P = newP;

	//printf("AFTER unary closure:\n");
	//print_forest(P);
	fprintf(f, "performed unary closure; now %d edges connected to %d roots.<br/>\n",
		P->nedges, P->nroots);

	struct session	*S = calloc(sizeof(*S),1);
	S->profile_id = strdup(path);
	S->item_id = strdup(id);
	S->input = input;	// already strdup'd
	S->parse = P;
	S->id = id_allocator++;
	S->pref_tree = pref_tree;
	S->npref_dec = npref_dec;
	S->pref_dec = pref_dec;
	add_session(S);

	fprintf(f, "Elapsed Time = %.1fs<br/>\n", dt);

	fprintf(f, "Session ID = %d<br/>\n", S->id);
	fprintf(f, "<a href='session?%d'>Enter Session</a><br/>\n", S->id);
	fprintf(f, "<script>window.location = '/private/session?%d';</script>\n", S->id);
	html_footers(f);

	free(id);
	free(path);
}

/* system for picking a profile and a sentence */

void	web_slash(FILE	*f, char	*path)
{
	int	is_profile = 0;
	int filter(const struct dirent	*d)
	{
		if(d->d_name[0]=='.')return 0;
		if(!strcmp(d->d_name, "relations"))is_profile = 1;
		return 1;
	}
	struct dirent	**names;
	if(strchr(path, '.') || strlen(path)>512)
		{ webreply(f, "500 bad path"); return; }
	char	fullpath[10240];
	sprintf(fullpath, "%s%s", tsdb_home_path, path);
	int i,n = scandir(fullpath, &names, filter, alphasort);
	if(n < 0) { webreply(f, "500 unable to read TSDB home directory"); return; }

	if(is_profile)
	{
		for(i=0;i<n;i++)
			free(names[i]);
		free(names);

		struct tsdb	*profile = load_tsdb(fullpath);
		if(!profile) { webreply(f, "500 unable to read TSDB profile"); return; }
		struct relation	*items = get_relation(profile, "item");
		int	item_input = get_field(items, "i-input", "string");
		int	item_id = get_field(items, "i-id", "integer");

		char	title[10240];
		sprintf(title, "ACE Treebanker: %s", path);
		html_headers(f, title);
		fprintf(f, "<table>\n");
		for(i=0;i<items->ntuples;i++)
		{
			char	*input = items->tuples[i][item_input];
			char	*iid = items->tuples[i][item_id];
			char	*cgi = cgiencode(path);
			fprintf(f, "<tr><td><a href=\"/private/parse?profile=%s&id=%s\">%s</a></td>\n",
				cgi, iid, iid);
			fprintf(f, "<td>%s</td></tr>\n",
				input);
			free(cgi);
		}
		fprintf(f, "</table>\n");
		html_footers(f);
		tsdb_free_profile(profile);
	}
	else
	{
		html_headers(f, "ACE Treebanker Home");
		for(i=0;i<n;i++)
		{
			fprintf(f, "<a href='%s/'>%s</a><br/>\n", names[i]->d_name, names[i]->d_name);
			free(names[i]);
		}
		if(strcmp(path, "/"))
			fprintf(f, "<a href='..'>..</a>\n");
		free(names);
		html_footers(f);
	}
}

/* system for receiving web requests */

void	web_callback(int	fd, void	*ptr, struct sockaddr_in	addr)
{
	char	method[128], path[1024];
	FILE	*f = get_http_request(fd, method, path);
	if(!f)return;
	printf("should %s    %s\n", method, path);
	if(!strcasecmp(method, "GET"))
	{
		if(strncmp(path, "/private/", 9))
		{
			fclose(f);
			return;
		}
		else
		{
			memmove(path, path+8, strlen(path)+1-8);
		}
		if(!strncmp(path, "/parse?", 7))
			web_parse(f, path+7);
		else if(!strcmp(path, "/assets/render.js"))
			webfile(f, "text/javascript", "assets/render.js");
		else if(!strcmp(path, "/assets/control.js"))
			webfile(f, "text/javascript", "assets/control.js");
		else if(!strncmp(path, "/session?", 9))
			webfile(f, "text/html", "assets/index.html");
		else if(!strncmp(path, "/next?", 6) || !strncmp(path, "/prev?", 6))
			web_nav(f, path);
		else web_slash(f, path);
		//else webreply(f, "404 not found");
	}
	else
	{
		if(!strncmp(path, "/session?", 9))
			web_session(f, path+9);
		else fclose(f);
	}
	fflush(stdout);
}

void	pipe_handler(int s)
{
	signal(SIGPIPE, pipe_handler);
}

main(int	argc, char	*argv[])
{
	int	fd = tcpip_list(9080);
	setlocale(LC_ALL, "");
	register_server_fd(fd, web_callback, NULL);
	signal(SIGINT, quit_server_event_loop);
	signal(SIGPIPE, pipe_handler);
	ace_load_grammar(ERG_PATH);
	//daemonize("web.log");
	server_event_loop();
}
