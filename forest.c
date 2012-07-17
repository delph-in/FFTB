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
#include	"reconstruct.h"

/* cache of profiles */

struct profile_cache
{
	char		*path;
	struct tsdb	*profile;
	int	pin;
};

int ncache;
struct profile_cache	*cache;

purge_cache()
{
	int i,j=0;
	for(i=0;i<ncache;i++)
	{
		if(!cache[i].pin)
		{
			tsdb_free_profile(cache[i].profile);
			free(cache[i].path);
		}
		else cache[j++] = cache[i];
	}
	ncache = j;
}

unpin_all()
{
	int i;
	for(i=0;i<ncache;i++)
		cache[i].pin = 0;
}

struct tsdb	*cached_get_profile_and_pin(char	*path)
{
	int i;
	for(i=0;i<ncache;i++)
	{
		if(!strcmp(cache[i].path, path))
		{
			cache[i].pin = 1;
			return cache[i].profile;
		}
	}
	if(ncache >= 5)purge_cache();
	struct tsdb	*T = load_tsdb(path);
	if(!T)return NULL;
	ncache++;
	cache = realloc(cache, sizeof(struct profile_cache)*ncache);
	cache[ncache-1].path = strdup(path);
	cache[ncache-1].profile = T;
	return T;
}

/* system for building / loading a 'struct parse' and initiating a session, given an input sentence */

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

void	add_session(struct session	*S)
{
	nsessions++;
	sessions = realloc(sessions, sizeof(S)*nsessions);
	sessions[nsessions-1] = S;
}

void	end_session(struct session	*S)
{
	int i;
	for(i=0;i<nsessions;i++)
		if(sessions[i] == S)break;
	if(i<nsessions)
	{
		nsessions--;
		memmove(sessions+i, sessions+i+1, sizeof(struct session*)*(nsessions-i));
	}

	if(S->profile_id)free(S->profile_id);
	if(S->item_id)free(S->item_id);
	if(S->gold_profile_id)free(S->gold_profile_id);
	if(S->input)free(S->input);
	if(S->parse_id)free(S->parse_id);
	if(S->parse)free_parse(S->parse);
	for(i=0;i<S->nlocal_dec;i++)
		if(S->local_dec[i].sign)free(S->local_dec[i].sign);
	if(S->local_dec)free(S->local_dec);
	for(i=0;i<S->ngold_dec;i++)
		if(S->gold_dec[i].sign)free(S->gold_dec[i].sign);
	if(S->gold_dec)free(S->gold_dec);
	if(S->gold_active)free(S->gold_active);
	free(S);
}

FILE	*invoke_ace(char	*input)
{
	char	tmpn[1024] = "/tmp/input-XXXXXX";
	int		fd = mkstemp(tmpn);
	write(fd, input, strlen(input));
	write(fd, "\n", 1);
	close(fd);
	char	command[1024];
	//sprintf(command, "~/cdev/ace/ace -g " ERG_PATH " -O %s -r 'root_strict root_inffrag root_informal root_frag'", tmpn);
	sprintf(command, "~/cdev/ace/ace -g %s -O %s -r '%s'", grammar_ace_image_path, tmpn, grammar_roots);
	return popen(command, "r");
}

char	*sign_with_lexnames_to_sign_with_lextypes(char	*swln)
{
	struct lexeme	*lex = get_lex_by_name_hash(swln);
	if(!lex)
	{
		fprintf(stderr, "lexeme '%s' does not appear to be present in the loaded grammar\n", swln);
		return NULL;
	}
	assert(lex != NULL);
	return strdup(lex->lextype->name);
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
		if(host->is_root)pack->is_root = 1;
		pack->host = host;
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
			int i;
			e->is_root = 1;
			for(i=0;i<e->npack;i++)e->pack[i]->is_root = 1;
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
			e->sign_with_lexnames = strdup(sign);
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
			if(e->ndaughters)
				e->sign = strdup(e->sign_with_lexnames);
			else
			{
				e->sign = sign_with_lexnames_to_sign_with_lextypes(e->sign_with_lexnames);
				if(!e->sign) { pclose(p); return NULL; }
			}
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
		free(e->sign_with_lexnames);
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

char	*tokstr_parse_value_of_as_string(char	*tokstr, char	*key);

int	quoted_to_int(char	*qs) { return atoi(qs+1); }
wchar_t	*quoted_to_wcs(char	*qmbs)
{
	int	len = strlen(qmbs);
	assert(qmbs[0]=='"' && qmbs[len-1]=='"');
	qmbs[len-1] = 0;
	qmbs++;
	return towide(qmbs);
}

int	load_token_from_tsdb(struct parse	*P, int	id, char	*structure, int	from, int	to)
{
	int	idx = (-id)-1;
	if(idx >= P->ntokens)
	{
		int	oldn = P->ntokens, i;
		P->ntokens = idx+1;
		P->tokens = realloc(P->tokens, sizeof(struct token*)*P->ntokens);
		for(i=oldn;i<P->ntokens;i++)
			P->tokens[i] = NULL;
	}
	struct token	*t = calloc(sizeof(*t),1);
	P->tokens[idx] = t;

	t->from = from;
	t->to = to;

	char	*cfromstr = tokstr_parse_value_of_as_string(structure, "+FROM ");
	char	*ctostr = tokstr_parse_value_of_as_string(structure, "+TO ");
	char	*formstr = tokstr_parse_value_of_as_string(structure, "+FORM ");
	if(!cfromstr) { printf("token #%d missing +FROM\n", id); return -1; }
	if(!ctostr) { printf("token #%d missing +TO\n", id); return -1; }
	if(!formstr) { printf("token #%d missing +FORM\n", id); return -1; }

	t->cfrom = quoted_to_int(cfromstr);	free(cfromstr);
	t->cto = quoted_to_int(ctostr);	free(ctostr);
	t->text = quoted_to_wcs(formstr); free(formstr);
	//printf("loaded token #%d [%d-%d] chars <%d-%d> \"%S\"\n",
	//	id, t->from, t->to, t->cfrom, t->cto, t->text);
	return 0;
}

int	is_token_edge(char	*symbol)
{
	if(!strncmp(symbol, "token [", 6))return 1;
	return 0;
}

int	load_edge_from_tsdb(struct parse	*P, int	id, char	*symbol, int	is_root, int	from, int	to, char	*dtr_list, char	*alt_list)
{
	if(is_token_edge(symbol))
		return load_token_from_tsdb(P, id, symbol, from, to);

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
		if(host->is_root)pack->is_root = 1;
		pack->host = host;
		enqueue(&host->pack, &host->npack, pack);
	}

	void	daughter_edge(struct tb_edge	*host, struct tb_edge	*daughter)
	{
		enqueue(&host->daughter, &host->ndaughters, daughter);
	}

	if(dtr_list[0] != '(')return -1;
	dtr_list++;

	struct tb_edge	*e = get_edge(id);
	e->from = from;
	e->to = to;
	e->is_root = is_root;
	e->sign_with_lexnames = strdup(symbol);
	// ->parents[] gets populated when we do the unary closure; don't bother now.

	dtr_list = strdup(dtr_list);
	char	*dtr;
	for(dtr = strtok(dtr_list, " ()");dtr && *dtr;dtr = strtok(NULL, " ()"))
	{
		int	eid = atoi(dtr);
		if(eid < 0)continue;	// token
		daughter_edge(e, get_edge(eid));
	}
	free(dtr_list);

	alt_list = strdup(alt_list);
	char	*alt;
	for(alt = strtok(alt_list, " ()");alt && *alt;alt = strtok(NULL, " ()"))
	{
		int	eid = atoi(alt);
		assert(eid >= 0);	// tokens don't pack
		pack_edge(e, get_edge(eid));
	}
	free(alt_list);

	if(e->ndaughters == 0)
	{
		e->sign = sign_with_lexnames_to_sign_with_lextypes(e->sign_with_lexnames);
		if(!e->sign) { return -1; }
	}
	else e->sign = strdup(e->sign_with_lexnames);

	//printf("to load: edge #%d symbol '%s' root %d from %d to %d daughters %s alternates %s\n", id, symbol, is_root, from, to, dtr_list, alt_list);
	return 0;
}

int is_usable_stored_parse(struct parse	*P)
{
	int i;

	for(i=0;i<P->nedges;i++)
	{
		struct tb_edge	*e = P->edges[i];
		if(e->is_root && !e->host)
		{
			P->nroots++;
			P->roots = realloc(P->roots, sizeof(struct tb_edge*)*P->nroots);
			P->roots[P->nroots-1] = e;
		}
	}

	if(P->ntokens == 0)return 0;
	//else if(P->nroots == 0)return 0;
	else return 1;
}

char	*get_item_by_id(struct tsdb	*profile, char	*i_id)
{
	struct relation	*items = get_relation(profile, "item");
	assert(items);
	int	item_input = get_field(items, "i-input", "string");
	int	item_id = get_field(items, "i-id", "integer");
	char	**item = tsdb_lookup_relation_with_key(items, item_id, i_id);
	if(!item)return NULL;
	return strdup(item[item_input]);
}

char	*get_pid_by_id(struct tsdb	*profile, char	*i_id)
{
	struct relation	*parses = get_relation(profile, "parse");
	if(!parses)return NULL;
	int	parse_id = get_field(parses, "parse-id", "integer");
	int	parse_i_id = get_field(parses, "i-id", "integer");
	char	**parse = tsdb_lookup_relation_with_key(parses, parse_i_id, i_id);
	if(!parse)return NULL;
	return parse[parse_id];
}

char	*get_pref_rid(struct tsdb	*profile, char	*pid)
{
	struct relation	*preferences = get_relation(profile, "preference");
	if(!preferences)return NULL;
	int	pref_parse_id = get_field(preferences, "parse-id", "integer");
	int	pref_result_id = get_field(preferences, "result-id", "integer");
	char	**pref = tsdb_lookup_relation_with_key(preferences, pref_parse_id, pid);
	if(!pref)return NULL;
	return pref[pref_result_id];
}

char	*get_result(struct tsdb	*profile, char	*pid, char	*rid)
{
	struct relation	*results = get_relation(profile, "result");
	if(!results)return NULL;
	int	result_parse_id = get_field(results, "parse-id", "integer");
	int	result_result_id = get_field(results, "result-id", "integer");
	int	result_derivation = get_field(results, "derivation", "string");
	char	*returnme = NULL;
	void result_visitor(char	**result)
	{
		if(strcmp(result[result_result_id], rid))return;
		returnme = result[result_derivation];
	}
	tsdb_visit_relation_with_key(results, result_parse_id, pid, result_visitor);
	return returnme;
}

int	get_decisions(struct tsdb	*profile, char	*pid, struct constraint	**Decs)
{
	struct relation	*decisions = get_relation(profile, "decision");
	if(!decisions)return 0;
	int	decision_parse_id = get_field(decisions, "parse-id", "integer");
	int decision_state = get_field(decisions, "d-state", "integer");
	int decision_type = get_field(decisions, "d-type", "integer");
	int decision_key = get_field(decisions, "d-key", "string");
	int decision_from = get_field(decisions, "d-start", "integer");
	int decision_to = get_field(decisions, "d-end", "integer");

	int	ndecs = 0;
	struct constraint	*decs = NULL;

	void decision_visitor(char	**decision)
	{
		int	state = atoi(decision[decision_state]);
		int	type = atoi(decision[decision_type]);
		char	*key = decision[decision_key];
		int	from = atoi(decision[decision_from]);
		int	to = atoi(decision[decision_to]);
		if(type == 3 || type == 2 || type == 7)
		{
			ndecs++;
			decs = realloc(decs, sizeof(struct constraint)*ndecs);
			struct constraint	*c = decs + ndecs-1;
			c->sign = strdup(key);
			c->from = from;
			c->to = to;
			if(type == 7)c->type = constraintExactly;
			else if(state == 1 || state == 3)c->type = constraintPresent;
			else if(state == 2 || state == 4)c->type = constraintAbsent;
			else assert(!"unintelligible constraint");
			if(state==1 || state==2)c->inferred = 0;
			else c->inferred = 1;
			printf("RECOVERED constraint: %s [%d-%d] [%s] %s\n",
				c->sign, c->from, c->to, c->inferred?"inferred":"manual", (c->type == constraintPresent)?"present":((c->type == constraintAbsent)?"absent":"exactly"));
		}
		else
		{
			printf("DROPPING decision of type %d state %d\n", type, state);
		}
	}
	tsdb_visit_relation_with_key(decisions, decision_parse_id, pid, decision_visitor);
	*Decs = decs;
	return ndecs;
}

struct parse	*load_forest(struct tsdb	*profile, char	*pid)
{
	int pstatus = 0;
	struct parse	*P = calloc(sizeof(*P),1);

	printf("starting to load edges\n");
	struct relation	*edges = get_relation(profile, "edge");
	printf("get_relation returned\n");
	if(!edges)goto noedges;
	int	edge_parse_id = get_field(edges, "parse-id", "integer");
	int	edge_e_id = get_field(edges, "e-id", "integer");
	int	edge_name = get_field(edges, "e-name", "string");
	int	edge_status = get_field(edges, "e-status", "integer");
	int	edge_start = get_field(edges, "e-start", "integer");
	int	edge_end = get_field(edges, "e-end", "integer");
	int	edge_daughters = get_field(edges, "e-daughters", "string");
	int	edge_alternates = get_field(edges, "e-alternates", "string");

	void edge_visitor(char	**edge)
	{
		int status = load_edge_from_tsdb(P, atoi(edge[edge_e_id]),
			edge[edge_name], atoi(edge[edge_status]),
			atoi(edge[edge_start]), atoi(edge[edge_end]),
			edge[edge_daughters], edge[edge_alternates]);
		if(status != 0)pstatus = -1;
	}
	tsdb_visit_relation_with_key(edges, edge_parse_id, pid, edge_visitor);
	if(!is_usable_stored_parse(P))pstatus = -1;

	if(pstatus != 0)
	{
		printf("found a stored forest, but couldn't use it.\n");
		noedges:
		printf("-> no usable stored forest\n");
		free(P);
		P = NULL;
	}

	return P;
}

void	web_parse(FILE	*f, char	*cgiargs)
{
	char	*path = cgiarg(cgiargs, "profile=");
	char	*id = cgiarg(cgiargs, "id=");
	assert(path); assert(id);
	assert(!strchr(path, '.'));

	unpin_all();

	char	fullpath[10240];
	sprintf(fullpath, "%s/%s", tsdb_home_path, path);
	struct tsdb	*profile = cached_get_profile_and_pin(fullpath);
	if(!profile) { webreply(f, "500 unable to read TSDB profile"); free(id); free(path); return; }

	char	*goldpath = NULL;
	if(!strcmp(path, "/trec-test/"))
		goldpath = "/gold/erg/trec/";
	if(!strcmp(path, "/ws01-test/"))
		goldpath = "/gold/erg/ws01/";

	struct tsdb	*goldprofile = NULL;
	if(goldpath)
	{
		char	goldfullpath[10240];
		sprintf(goldfullpath, "%s/%s", tsdb_home_path, goldpath);
		goldprofile = cached_get_profile_and_pin(goldfullpath);
		if(!goldprofile) { webreply(f, "500 unable to read gold TSDB profile"); free(id); free(path); return; }
	}

	char	*input = get_item_by_id(profile, id);
	if(!input)
	{
		fprintf(stderr, "item id %s does not exist in %s\n", id, path);
		free(path);free(id);
		webreply(f, "500 no such item");
		return;
	}
	printf("item id %s -> input '%s'\n", id, input);

	if(goldprofile)
	{
		char	*goldinput = get_item_by_id(goldprofile, id);
		if(!goldinput)
		{
			fprintf(stderr, "item id %s does not exist in %s\n", id, goldpath);
			free(path);free(id);free(input);
			webreply(f, "500 no such gold item");
			return;
		}
		if(strcmp(goldinput, input))
		{
			fprintf(stderr, "gold item %s and profile item %s differ\n", id, id);
			free(path);free(id);free(input);
			webreply(f, "500 gold/profile mismatch");
		}
	}

	struct tree	*goldpref_tree = NULL;
	char		*pid = NULL, *goldpid = NULL;

	pid = get_pid_by_id(profile, id);
	printf("profile parse id %s\n", pid);
	if(goldprofile)
	{
		goldpid = get_pid_by_id(goldprofile, id);
		printf("gold parse id %s\n", goldpid);
		if(goldpid)
		{
			char	*pref_rid = get_pref_rid(goldprofile, goldpid);
			if(pref_rid)
			{
				printf("preferred gold result id %s\n", pref_rid);
				char	*deriv = get_result(goldprofile, goldpid, pref_rid);
				if(deriv)
				{
					char	*copy = strdup(deriv);
					goldpref_tree = string_to_tree(copy);
					free(copy);
				}
			}
		}
		if(goldpref_tree)printf("-> got gold preferred tree %p\n", goldpref_tree);
		else printf("no gold preferred tree\n");
	}

	struct constraint	*prof_dec = NULL;
	int	nprof_dec = pid?get_decisions(profile, pid, &prof_dec):0;

	struct constraint	*gold_dec = NULL;
	int	ngold_dec = 0;
	if(goldprofile)
		ngold_dec = goldpid?get_decisions(goldprofile, goldpid, &gold_dec):0;

	struct parse	*P = pid?load_forest(profile, pid):NULL;

	if(P)
	{
		printf("-> loaded stored forest\n");
		printf("found stored forest (%d edges connected to %d roots).<br/>\n", P->nedges, P->nroots);
		html_headers(f, "ACE Treebanker");
		fprintf(f, "found stored forest (%d edges connected to %d roots).<br/>\n", P->nedges, P->nroots);
	}

	if(!P)
	{
		struct timeval	start, end;
		gettimeofday(&start, NULL);
		FILE	*p = invoke_ace(input);
		if(!p) { webreply(f, "500 unable to invoke parser"); free(id); free(path); return; }

		html_headers(f, "ACE Treebanker");
		fprintf(f, "No stored forest; parsing... \n"); fflush(f);
		wchar_t	*winput = towide(input);
		printf("wide input = '%S'\n", winput);
		P = read_forest_from_ace(p, winput);
		free(winput);
		fprintf(f, "done (%d edges connected to %d roots).<br/>\n", P->nedges, P->nroots);
		gettimeofday(&end, NULL);
		double	dt = (end.tv_usec - start.tv_usec) * 0.000001 + (end.tv_sec - start.tv_sec);
		fprintf(f, "parsing time = %.1fs<br/>\n", dt);
	}

	//printf("BEFORE unary closure:\n");
	//print_forest(P);

	printf("about to apply UC\n");
	struct parse	*newP = do_unary_closure(P);
	printf("done with UC\n");
	free_parse(P);
	P = newP;

	//printf("AFTER unary closure:\n");
	//print_forest(P);
	fprintf(f, "performed unary closure; now %d edges connected to %d roots.<br/>\n",
		P->nedges, P->nroots);

	struct session	*S = calloc(sizeof(*S),1);
	S->profile_id = strdup(path);
	S->gold_profile_id = goldpath?strdup(goldpath):NULL;
	S->item_id = strdup(id);
	S->input = input;	// already strdup'd
	S->parse = P;
	S->id = id_allocator++;
	S->pref_tree = goldpref_tree;
	S->parse_id = pid?strdup(pid):NULL;

	S->nlocal_dec = nprof_dec;
	S->local_dec = prof_dec;
	S->ngold_dec = ngold_dec;
	S->gold_dec = gold_dec;
	S->gold_active = malloc(ngold_dec);
	if(!S->nlocal_dec)
		memset(S->gold_active, 1, ngold_dec);
	else memset(S->gold_active, 0, ngold_dec);

	add_session(S);

	fprintf(f, "Session ID = %d<br/>\n", S->id);
	fprintf(f, "<a href='session?%d'>Enter Session</a><br/>\n", S->id);
	fprintf(f, "<script>window.location = '/private/session?%d';</script>\n", S->id);
	html_footers(f);

	free(id);
	free(path);

	unpin_all();
}


int save_decisions(char	*profile_id, char	*parse_id, int	ncons, struct constraint	*cons)
{
	printf("have %d decisions to save to %s / %s\n", ncons, profile_id, parse_id);
	unpin_all();
	char	path[10240];
	sprintf(path, "%s/%s", tsdb_home_path, profile_id);
	struct tsdb	*t = cached_get_profile_and_pin(path);
	if(!t)return -1;

	struct relation	*r = get_relation(t, "decision");
	if(!r)return -1;

	int	d_parse_id = get_field(r, "parse-id", "integer");
	int	d_state = get_field(r, "d-state", "integer");
	int	d_type = get_field(r, "d-type", "integer");
	int	d_key = get_field(r, "d-key", "string");
	int	d_value = get_field(r, "d-value", "string");
	int	d_start = get_field(r, "d-start", "integer");
	int	d_end = get_field(r, "d-end", "integer");
	int	d_date = get_field(r, "d-date", "date");

	// first, purge all old records referring to this parse_id from the relation 
	int i, j;
	for(i=j=0;i<r->ntuples;i++)
	{
		if(!strcmp(r->tuples[i][d_parse_id], parse_id))
		{
			int k;
			for(k=0;k<r->nfields;k++)free(r->tuples[i][k]);
		}
		else r->tuples[j++] = r->tuples[i];
	}
	r->ntuples = j;

	// now, add the new tuples
	for(i=0;i<ncons;i++)
	{
		struct constraint	*c = cons+i;
		char	*tup[r->nfields];
		bzero(tup, sizeof(tup));
		tup[d_parse_id] = strdup(parse_id);
		unsigned int		state = -1;
		if(c->type == constraintPresent && !c->inferred)state = 1;
		else if(c->type == constraintAbsent && !c->inferred)state = 2;
		else if(c->type == constraintPresent && c->inferred)state = 3;
		else if(c->type == constraintAbsent && c->inferred)state = 4;
		else if(c->type == constraintExactly && !c->inferred)state = 1;
		else if(c->type == constraintExactly && c->inferred)state = 3;
		// logically there could be a constraintNotExactly as well... we don't have one.
		char	statestr[8];
		sprintf(statestr, "%d", state);
		tup[d_state] = strdup(statestr);
		if(c->type == constraintExactly)
		{
			tup[d_type] = strdup("7");	// types 1-6 are already claimed, see tsdb/lisp/redwoods.lsp reconstruct-discriminants()
		}
		else
		{
			struct rule	*rule = get_rule_by_name(c->sign);
			if(rule)tup[d_type] = strdup("3");	// :constituent
			else tup[d_type] = strdup("2");	// :type (lexical type)
		}
		tup[d_key] = strdup(c->sign?c->sign:"");
		tup[d_value] = strdup("");	// should be yield of the tree
		char	start[32]; sprintf(start, "%d", c->from);
		char	end[32]; sprintf(end, "%d", c->to);
		tup[d_start] = strdup(start);
		tup[d_end] = strdup(end);
		tup[d_date] = strdup("?");
		for(j=0;j<r->nfields;j++)if(!tup[j])tup[j] = strdup("");
		if(r->ntuples+1 > r->atuples)
		{
			r->atuples = (r->ntuples+1) * 1.5;
			r->tuples = realloc(r->tuples, sizeof(char**)*r->atuples);
		}
		r->tuples[r->ntuples] = malloc(sizeof(char*)*r->nfields);
		memcpy(r->tuples[r->ntuples], tup, sizeof(char*)*r->nfields);
		r->ntuples++;
	}
	tsdb_reindex_relation(r);
	int res = tsdb_write_relation(t, r);
	if(res != 0)
	{
		fprintf(stderr, "failed to write 'decision' relation.\n");
		return -1;
	}

	return 0;
}
