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

#include	<ace/tree.h>
#include	<ace/reconstruct.h>

#include	"treebank.h"

char	*dqescape(char	*raw);

struct tree	*duplicate_tree(struct tree	*tin)
{
	struct tree	*tout = malloc(sizeof(*tin));
	int i;
	*tout = *tin;
	tout->label = tin->label?strdup(tin->label):NULL;
	tout->shortlabel = tin->shortlabel?strdup(tin->shortlabel):NULL;
	tout->tokenids = malloc(sizeof(int)*tin->ntokens);
	memcpy(tout->tokenids, tin->tokenids, sizeof(int)*tin->ntokens);
	tout->tokens = malloc(sizeof(char*)*tin->ntokens);
	for(i=0;i<tin->ntokens;i++)tout->tokens[i] = strdup(tin->tokens[i]);
	tout->daughters = malloc(sizeof(struct tree*)*tin->ndaughters);
	for(i=0;i<tin->ndaughters;i++)
	{
		tout->daughters[i] = duplicate_tree(tin->daughters[i]);
		tout->daughters[i]->parent = tout;
	}
	tout->lexhead = NULL;
	return tout;
}

int	verify(struct tb_edge	*e, struct reconstruction_error	*error)
{
	assert(e->unpackings==1);
	struct tree	*t = extract_tree(e, 0);
	clear_slab();
	void callback(struct tree	*t, struct dg	*d) { }
	struct dg	*d = reconstruct_tree_or_error(t, callback, error);
	if(!d)error->node = duplicate_tree(error->node);
	free_tree(t);
	clear_slab();
	return d?1:0;
}

void	compute_forcings(struct parse	*P, struct tb_edge	**forced_edge, int	max, long long N)
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
				if(!forced_edge[j])
					forced_edge[j] = e;
				else if(e->from < forced_edge[j]->from || e->to > forced_edge[j]->to)
					forced_edge[j] = e;
			}
		}
	}
}

void	find_discriminants(FILE	*f, struct parse	*P, int	from, int	to, long long	ntrees)
{
	int	i, j;
	int have_span = (from == -1)?0:1;
	int	nd = 0;
	struct disc
	{
		char	*sign;
		long long		count;
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
		char *esc = dqescape(d[i].sign);
		fprintf(f, "{sign:\"%s\",count:%lld,from:%d,to:%d}", esc, d[i].count, d[i].from, d[i].to);
		free(esc);
	}
	if(d)free(d);
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

struct tree	*extract_tree(struct tb_edge	*e, int	ucdepth)
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

	struct tree	*t = calloc(sizeof(*t),1);
	t->label = get_uclabel(e->sign_with_lexnames, ucdepth);
	t->tfrom = e->from;
	t->tto = e->to;

	char	*l2 = get_uclabel(e->sign_with_lexnames, ucdepth+1);
	if(l2)
	{
		free(l2);
		t->ndaughters = 1;
		t->daughters = calloc(sizeof(struct tree*),1);
		t->daughters[0] = extract_tree(e, ucdepth+1);
	}
	else
	{
		t->ndaughters = e->ndaughters;
		if(!e->ndaughters)
		{
			// lexeme node
			// need to supply an orthography daughter tree node, which has a few more fields
			struct tree	*orth = calloc(sizeof(struct tree),1);
			t->ndaughters = 1;
			t->daughters = calloc(sizeof(struct tree*),1);
			t->daughters[0] = orth;

			int	wlen = 0;
			for(i=0;i<e->ntokens;i++)
			{
				if(i)wlen++;
				wlen += wcslen(e->tokens[i]->text);
			}
			wchar_t	*orth_str = malloc(sizeof(wchar_t)*(wlen+4)), *wp=orth_str;
			for(i=0;i<e->ntokens;i++)
			{
				if(i)wp += swprintf(wp, (orth_str+wlen+3-wp), L" ");
				wp += swprintf(wp, (orth_str+wlen+3-wp), L"%S", e->tokens[i]->text);
			}
			int	mlen = wcstombs(NULL, orth_str, 0) + 1;
			char	*mos = malloc(mlen+1);
			wcstombs(mos, orth_str, mlen);
			free(orth_str);

			//orth->label = strdup("_");	// our use of the tree doesn't depend on this and it's inconvenient to get it here
			orth->label = mos;
			orth->tfrom = t->tfrom;
			orth->tto = t->tto;
			// ->ntokens  ( = the lexeme's ->stemlen...)
			// ->tokens (could be ""s for our purposes)
			// ->cfrom, ->cto (could be phony for our purposes)
			struct lexeme	*lex = get_lex_by_name_hash(t->label);
			assert(lex != NULL);
			orth->ntokens = lex->stemlen;
			assert(orth->ntokens == e->ntokens);
			orth->tokens = calloc(sizeof(char*),orth->ntokens);
			orth->tokenids = calloc(sizeof(int),orth->ntokens);
			int j;
			for(j=0;j<orth->ntokens;j++)
			{
				orth->tokenids[j] = e->tokens[j]->id;
				orth->tokens[j] = strdup(e->tokens[j]->avmstr);
			}
			orth->cfrom = orth->cto = -1;
		}
		else
		{
			t->daughters = calloc(sizeof(struct tree*),t->ndaughters);
			for(i=0;i<e->ndaughters;i++)
				t->daughters[i] = extract_tree(e->daughter[i], 0);
		}
	}
	return t;
}

void	fsend_tree(FILE	*f, struct tree	*t)
{
	int	i;

	int lexical = 0;
	if(t->ndaughters == 1 && t->daughters[0]->ndaughters == 0)lexical = 1;

	char	*esc;
	if(lexical)
	{
		// for whatever reason, grammarians prefer to see the lexical type as the full-form sign for a lexical node, rather than the lexeme identifier
		struct lexeme	*lex = get_lex_by_name_hash(t->label);
		esc = dqescape(lex->lextype->name);
	}
	else esc = dqescape(t->label);
	char	*sesc = dqescape(t->shortlabel);
	fprintf(f, "{label: \"%s\", shortlabel: \"%s\", from:%d, to:%d, daughters: [", esc, sesc, t->tfrom, t->tto);
	free(esc);
	free(sesc);


	if(!lexical)for(i=0;i<t->ndaughters;i++)
	{
		if(i)fprintf(f, ",");
		fsend_tree(f, t->daughters[i]);
	}
	fprintf(f, "]}");
}

int	send_tree(FILE	*f, struct tb_edge	*e, struct reconstruction_error	*error)
{
	struct tree	*t = extract_tree(e, 0);
	assert(t != NULL);
	void callback(struct tree	*t, struct dg	*d)
		{ t->shortlabel = label_dag(d, "?"); }
	struct dg	*result = reconstruct_tree_or_error(t, callback, error);
	if(result != NULL)
		fsend_tree(f, t);
	free_tree(t);
	clear_slab();
	if(result == NULL)return -1;
	else return 0;
}

void	web_session(FILE	*f, char	*query)
{
	int	id = atoi(query);
	struct session	*S = get_session(id);
	if(!S) { webreply(f, "404 no such session"); return; }

	char	*constraints = strchr(query, ';');
	if(constraints)constraints++;
	else { webreply(f, "500 bad syntax"); return; }

	char	*rqc = constraints;
	constraints = strchr(constraints, ';');
	int	requesting_constraints = atoi(rqc);
	if(constraints)constraints++;
	else { webreply(f, "500 bad syntax"); return; }

	char	*span = constraints;
	constraints = strchr(constraints, ';');
	if(constraints)constraints++;
	else { webreply(f, "500 bad syntax"); return; }
	int	from,to, have_span = 0;
	if(2 == sscanf(span, "%d:%d;", &from, &to))
		have_span = 1;

	char	*d;
	int		i, nc=0;
	struct constraint	*c=NULL;
	char	*olddecbits = NULL;
	if(!requesting_constraints)
	{
		for(d=strtok(constraints,"&");d;d=strtok(NULL,"&"))
		{
			if(!strncmp(d, "_olddecs=", 9))
			{
				olddecbits = d + 9;
				continue;
			}

			int	from, to;
			char	sign[128];
			assert(strlen(d) < 128);
			int	type = -1;
			int inferred = 0;
			if(4 == sscanf(d, "%[^:]:=:%d-%d:%d", sign, &from, &to, &inferred))
				type = constraintExactly;
			else if(4 == sscanf(d, "%[^:]:+:%d-%d:%d", sign, &from, &to, &inferred))
				type = constraintPresent;
			else if(4 == sscanf(d, "%[^:]:-:%d-%d:%d", sign, &from, &to, &inferred))
				type = constraintAbsent;
			if(type == -1)
			{
				printf("bad constraint: '%s'\n", d);
				badconst:
				webreply(f, "500 bad constraint");
				if(c)
				{
					int i;
					for(i=0;i<nc;i++)
						if(c[i].sign)free(c[i].sign);
					free(c);
				}
				return;
			}
			nc++;
			c = realloc(c,sizeof(*c)*nc);
			c[nc-1].sign = strcmp(sign, "span")?cgidecode(sign):NULL;
			c[nc-1].from = from;
			c[nc-1].to = to;
			c[nc-1].type = type;
			c[nc-1].inferred = inferred;
			if(!strcmp(sign, "span"))
				c[nc-1].type = constraintIsAConstituent;
			printf("constraint: %s [%d-%d] type %d\n", sign, from, to, type);
		}
		// save the local_dec array
		if(S->local_dec)
		{
			for(i=0;i<S->nlocal_dec;i++)
				if(S->local_dec[i].sign)free(S->local_dec[i].sign);
			free(S->local_dec);
		}
		S->local_dec = nc?malloc(sizeof(struct constraint)*nc):NULL;
		S->nlocal_dec = nc;
		memcpy(S->local_dec, c, sizeof(struct constraint)*nc);
		for(i=0;i<S->nlocal_dec;i++)
			if(S->local_dec[i].sign)
				S->local_dec[i].sign = strdup(S->local_dec[i].sign);
	}
	else
	{
		// copy the local_dec array to c
		c = S->nlocal_dec?malloc(sizeof(struct constraint)*S->nlocal_dec):NULL;
		nc = S->nlocal_dec;
		memcpy(c, S->local_dec, sizeof(struct constraint)*nc);
		for(i=0;i<nc;i++)
			if(c[i].sign)
				c[i].sign = strdup(c[i].sign);
	}
	char	*bits = olddecbits;
	for(i=0;i<S->ngold_dec;i++)
	{
		if(bits && !bits[i])goto badconst;
		if((!bits && (!requesting_constraints || S->gold_active[i]))
			|| (bits && bits[i] == '1'))
		{
			// include this gold constraint
			nc++;
			c = realloc(c,sizeof(*c)*nc);
			c[nc-1] = S->gold_dec[i];
			c[nc-1].sign = strdup(c[nc-1].sign);
			S->gold_active[i] = 1;
		}
		else S->gold_active[i] = 0;
	}
	fflush(stdout);

	void	send_escaped(char	*key, char	*value)
	{
		char	*esc = dqescape(value);
		fprintf(f, "%s: \"%s\",\n", key, esc);
		free(esc);
	}

	http_headers(f, "text/javascript");
	fprintf(f, "{");
	send_escaped("profile_id", S->profile_id);
	send_escaped("item_status", status_string(get_t_active(S->profile_id, S->parse_id)));
	send_escaped("comment", get_t_comment(S->profile_id, S->parse_id));
	send_escaped("item_id", S->item_id);
	send_escaped("item", S->input);
	long long	ntrees = count_remaining_trees(S->parse, c, nc);
	fprintf(f, "ntrees: %lld,\n", ntrees);
	fprintf(f, "tokens: [");
	int slen = 1;
	for(i=0;i<S->parse->ntokens;i++)
	{
		struct tb_token	*t = S->parse->tokens[i];
		if(i)fprintf(f, ",");
		char	*foo = malloc(5 * wcslen(t->text));
		wcstombs(foo, t->text, 5 * wcslen(t->text));
		char	*esc = dqescape(foo);
		free(foo);
		fprintf(f, "{from:%d,to:%d,cfrom:%d,cto:%d,text:\"%s\"}", t->from, t->to, t->cfrom, t->cto, esc);
		free(esc);
		if(t->to >= slen)slen = t->to+1;
	}
	fprintf(f, "],\n");
	struct tb_edge	*forced_edges[slen*2+1];
	bzero(forced_edges, sizeof(struct tb_edge*)*(slen*2+1));
	compute_forcings(S->parse, forced_edges, slen*2+1, ntrees);
	fprintf(f, "chunks: [");
	int	nch = 0, l0 = 0, have_error = 0;
	struct reconstruction_error	first_error;
	for(i=1;i<slen*2+1;i++)
	{
		if(forced_edges[l0] != forced_edges[i] || i == slen*2)
		{
			if(nch++)fprintf(f, ",");
			int status = forced_edges[l0]?1:0;
			struct reconstruction_error	error;
			int valid = status && verify(forced_edges[l0], &error);
			if(status && !valid)
			{
				status = 2;
				// inspect error!
				if(!have_error)
				{
					first_error = error;
					have_error = 1;
				}
				else
				{
					if(error.comment)free(error.comment);
					if(error.path)free(error.path);
					if(error.node)free_tree(error.node);
				}
			}
			fprintf(f, "{from:%d,to:%d,state:%d}", l0, i, status);
			//printf("%d - %d   is %d\n", l0, i, status);
			l0 = i;
		}
	}
	fprintf(f, "],\n");
	if(have_error)
	{
		char	*lesc = dqescape(first_error.node->label);
		fprintf(f, "recons_error: {");
		switch(first_error.type)
		{
			case	reconsNoSuchLexeme:
				fprintf(f, "type:\"No such lexeme as %s\",", lesc);
				break;
			case	reconsNoSuchRule:
				fprintf(f, "type:\"No such rule as %s\",", lesc);
				break;
			case	reconsIncompatibleToken:
				fprintf(f, "type:\"Failed to unify token list into lexeme %s.\",", lesc);
				break;
			case	reconsRuleGLB:
				fprintf(f, "type:\"GLB failed for daughter %d [zero-based] into rule %s.\",", first_error.daughter, lesc);
				send_escaped("path", first_error.path);
				send_escaped("type1", first_error.type1);
				send_escaped("type2", first_error.type2);
				break;
			case	reconsRuleCycle:
				fprintf(f, "type:\"Cycle detected with rule %s.\",", lesc);
				break;
			default: fprintf(f, "type:\"Unknown reconstruction error\",");	break;
		}
		if(first_error.type == reconsRuleGLB)fprintf(f, "has_glb: 1,");
		else fprintf(f, "has_glb: 0,");
		send_escaped("comment", first_error.comment?:"");
		fprintf(f, "node:{from:%d, to:%d, label:\"%s\"}}, ",
			first_error.node->tfrom, first_error.node->tto, lesc);
		if(first_error.comment) free(first_error.comment);
		if(first_error.path) free(first_error.path);
		if(first_error.node) free_tree(first_error.node);
		free(lesc);
	}
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
	if(S->ngold_dec)
	{
		fprintf(f, "olddec:[");
		for(i=0;i<S->ngold_dec;i++)
		{
			if(i)fprintf(f, ",");
			struct constraint	*C = &S->gold_dec[i];
			fprintf(f, "{sign:\"%s\",type:\"%s\",from:%d,to:%d,inferred:%d,enabled:%d}",
				C->sign, (C->type == constraintPresent)?"+":"-",
				C->from, C->to, C->inferred, S->gold_active[i]);
		}
		fprintf(f, "],");
	}
	if(S->nlocal_dec)
	{
		fprintf(f, "dec:[");
		for(i=0;i<S->nlocal_dec;i++)
		{
			if(i)fprintf(f, ",");
			struct constraint	*C = &S->local_dec[i];
			char	*ch = "=";
			if(C->type == constraintPresent)ch = "+";
			if(C->type == constraintAbsent)ch = "-";
			if(C->type == constraintIsAConstituent)ch = "@";
			fprintf(f, "{sign:\"%s\",type:\"%s\",from:%d,to:%d,inferred:%d,enabled:1}",
				C->sign, ch, C->from, C->to, C->inferred);
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
			struct reconstruction_error	error;
			int res = send_tree(f, S->parse->edges[i], &error);
			fprintf(f, "],");
			if(res != 0)
				fprintf(f, "error: 'Reconstruction failed on that tree.',");	// shouldn't happen unless we're also sending a detailed description of the error with the chunking code
		}
		else fprintf(f, "trees: [],");
	}
	else if(ntrees == 1)
	{
		fprintf(f, "trees: [");
		for(i=0;i<S->parse->nroots;i++)
			if(has_unpacking(S->parse->roots[i]))break;
		assert(i < S->parse->nroots);
		struct reconstruction_error	error;
		int res = send_tree(f, S->parse->roots[i], &error);
		fprintf(f, "],");
		if(res != 0)
			fprintf(f, "error: 'Reconstruction failed on that tree.',");	// shouldn't happen unless we're also sending a detailed description of the error with the chunking code
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

void	web_comment(FILE	*f, char	*query)
{
	int	id = atoi(query);
	struct session	*S = get_session(id);
	if(!S) { webreply(f, "404 no such session"); return; }

	char	*rest = strchr(query, '&');
	if(!rest) { webreply(f, "500 bad syntax"); return; }
	rest = cgidecode(rest+1);
	S->comment = strdup(rest);
	char	prof_path[10240];
	sprintf(prof_path, "%s/%s", tsdb_home_path, S->profile_id);
	char	t_active[32];
	sprintf(t_active, "%d", get_t_active(S->profile_id, S->parse_id));
	int result = write_tree(prof_path, S->parse_id, "1", t_active, getenv("LOGNAME"), S->comment);
	webreply(f, "200 ok");
}
