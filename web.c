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
#include	<ace/mrs.h>

#include	<ace/tree.h>
#include	<ace/reconstruct.h>

#include	"treebank.h"

#define	ERG_PATH	"/home/sweaglesw/cdev/ace/erg-1111.dat"
//#define	ERG_PATH	"/home/sweaglesw/answer/ace/erg.dat"

char	*tsdb_home_path = "/home/sweaglesw/logon/lingo/lkb/src/tsdb/home/";
//char	*tsdb_home_path = "/home/sweaglesw/answer/treebank/demo-profiles/";

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

void	add_tuple(struct relation	*r, char	**tup)
{
	int	j;

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

int	reindex_and_write(struct tsdb	*t, struct relation	*r)
{
	tsdb_reindex_relation(r);
	int res = tsdb_write_relation(t, r);
	if(res != 0)
	{
		fprintf(stderr, "failed to write '%s' relation.\n", r->name);
		return -1;
	}
	return 0;
}

struct tsdb	*get_pinned_profile(char	*profile_id)
{
	unpin_all();
	char	path[10240];
	sprintf(path, "%s/%s", tsdb_home_path, profile_id);
	struct tsdb	*t = cached_get_profile_and_pin(path);
	if(!t)return NULL;
	return t;
}

purge_tuples(struct relation	*r, int	field, char	*value)
{
	int i, j;
	for(i=j=0;i<r->ntuples;i++)
	{
		if(!strcmp(r->tuples[i][field], value))
		{
			int k;
			for(k=0;k<r->nfields;k++)free(r->tuples[i][k]);
		}
		else r->tuples[j++] = r->tuples[i];
	}
	r->ntuples = j;
}

int	write_tree(char	*profile_id, char	*parse_id, char	*t_version, char	*t_active, char	*author)
{
	struct tsdb	*t = get_pinned_profile(profile_id);
	struct relation	*r = t?get_relation(t, "tree"):NULL;
	if(!t || !r)return -1;

	int	tree_parse_id = get_field(r, "parse-id", "integer");
	int	tree_t_version = get_field(r, "t-version", "integer");
	int	tree_t_active = get_field(r, "t-active", "integer");
	int	tree_t_author = get_field(r, "t-author", "string");

	// first, purge all old records referring to this parse_id from the relation 
	purge_tuples(r, tree_parse_id, parse_id);

	char	*tup[r->nfields];
	bzero(tup, sizeof(tup));
	tup[tree_parse_id] = strdup(parse_id);
	tup[tree_t_version] = strdup(t_version);
	tup[tree_t_active] = strdup(t_active);
	tup[tree_t_author] = strdup(author);

	add_tuple(r, tup);
	if(reindex_and_write(t,r) != 0)return -1;

	return 0;
}

int	write_preference(char	*profile_id, char	*parse_id, char	*t_version, char	*result_id)
{
	struct tsdb	*t = get_pinned_profile(profile_id);
	struct relation	*r = t?get_relation(t, "preference"):NULL;
	if(!t || !r)return -1;

	int	pref_parse_id = get_field(r, "parse-id", "integer");
	int	pref_t_version = get_field(r, "t-version", "integer");
	int	pref_result_id = get_field(r, "result-id", "integer");

	// first, purge all old records referring to this parse_id from the relation 
	purge_tuples(r, pref_parse_id, parse_id);

	if(t_version && result_id)
	{
		// optionally add a new preference tuple
		char	*tup[r->nfields];
		bzero(tup, sizeof(tup));
		tup[pref_parse_id] = strdup(parse_id);
		tup[pref_t_version] = strdup(t_version);
		tup[pref_result_id] = strdup(result_id);
		add_tuple(r, tup);
	}
	if(reindex_and_write(t,r) != 0)return -1;

	return 0;
}

int	write_result(char	*profile_id, char	*parse_id, char	*result_id, char	*derivation, char	*mrs)
{
	struct tsdb	*t = get_pinned_profile(profile_id);
	struct relation	*r = t?get_relation(t, "result"):NULL;
	if(!t || !r)return -1;

	int	result_parse_id = get_field(r, "parse-id", "integer");
	int	result_result_id = get_field(r, "result-id", "integer");
	int	result_derivation = get_field(r, "derivation", "string");
	int	result_mrs = get_field(r, "mrs", "string");

	// first, purge all old records referring to this parse_id from the relation 
	purge_tuples(r, result_parse_id, parse_id);

	if(derivation && result_id)
	{
		// optionally add a new result tuple
		char	*tup[r->nfields];
		bzero(tup, sizeof(tup));
		tup[result_parse_id] = strdup(parse_id);
		tup[result_result_id] = strdup(result_id);
		tup[result_derivation] = strdup(derivation);
		tup[result_mrs] = strdup(mrs);
		add_tuple(r, tup);
	}
	if(reindex_and_write(t,r) != 0)return -1;

	return 0;
}

int	len_of_derivation(struct tree	*t)
{
	int	len = 2*strlen(t->label) + 100;
	int i;
	for(i=0;i<t->ndaughters;i++)
		len += len_of_derivation(t->daughters[i]);
	for(i=0;i<t->ntokens;i++)
		len += strlen(t->tokens[i]);
	return len;
}

char	*dqescape(char *s)
{
	char *s2 = malloc(strlen(s)*2+1), *p = s2;
	while(*s)
	{
		if(*s == '\'' || *s == '\\' || *s == '"')*p++='\\';
		*p++ = *s++;
	}
	*p++ = 0;
	return s2;
}

char	*build_derivation(char	*str, struct tree	*t)
{
	int i;
	if(t->ndaughters)
	{
		str += sprintf(str, "(%d %s %f %d %d", t->edge_id, t->label, 0.0, t->tfrom, t->tto);
		for(i=0;i<t->ndaughters;i++)
		{
			*str++ = ' ';
			str = build_derivation(str, t->daughters[i]);
		}
	}
	else
	{
		char	*el = dqescape(t->label);
		str += sprintf(str, "(\"%s\"", el);
		free(el);
		for(i=0;i<t->ntokens;i++)
		{
			char	*e = dqescape(t->tokens[i]);
			str += sprintf(str, " \"%s\"", e);
			free(e);
		}
	}
	*str++ = ')';
	*str = 0;
	return str;
}

char	*tree_to_derivation(struct tree	*t)
{
	int	len = len_of_derivation(t);
	char	*str = malloc(len+1);
	build_derivation(str, t);
	return str;
}

char	*mrs_to_string(struct mrs	*m)
{
	extern char	*mrs_end;
	char	*save_end;
	save_end = mrs_end;
	mrs_end = "";
	int	l = snprint_mrs(NULL, 0, m);
	char	*str = malloc(l+10);
	snprint_mrs(str, l+1, m);
	mrs_end = save_end;
	return str;
}

void	web_save(FILE	*f, char	*query)
{
	assert(strlen(query) >= 6);
	if(strncmp(query, "/save?", 6)) { webreply(f, "500 internal error"); return; }
	int	id = atoi(query+6);
	struct session	*S = get_session(id);
	if(!S) { webreply(f, "404 no such session"); return; }
	if(!S->parse_id) { webreply(f, "500 no parse id"); return; }

	// save decisions to the 'decision' relation
	// possibly also record a 'preference' tuple and a 'result' tuple
	struct constraint	*cons = malloc(sizeof(struct constraint) * (S->nlocal_dec + S->ngold_dec));
	int	i, ncons = 0;
	for(i=0;i<S->nlocal_dec;i++)
	{
		struct constraint	*c = S->local_dec+i;
		cons[ncons] = *c; cons[ncons++].sign = c->sign?strdup(c->sign):NULL;
	}
	for(i=0;i<S->ngold_dec;i++)
	{
		struct constraint	*c = S->gold_dec+i;
		if(!S->gold_active[i])continue;
		cons[ncons] = *c; cons[ncons++].sign = c->sign?strdup(c->sign):NULL;
	}
	int result = save_decisions(S->profile_id, S->parse_id, ncons, cons);
	free(cons);

	long long	remaining = count_remaining_trees(S->parse, S->local_dec, S->nlocal_dec);

	// write a record to 'tree'
	if(!result)result = write_tree(S->profile_id, S->parse_id, "1", (remaining==1)?"1":"0", getenv("LOGNAME"));

	if(remaining == 1)
	{
		// save one result to the 'result' relation and a pointer to it in 'preference'
		// count_remaining_trees() has set up the right flags for extract_tree()
		for(i=0;i<S->parse->nedges;i++)
			if(S->parse->edges[i]->unpackings == 1 && S->parse->edges[i]->solutions == 1 && S->parse->edges[i]->is_root)break;
		assert(i < S->parse->nedges);
		struct tree	*t = extract_tree(S->parse->edges[i], 0);
		if(!t)result = -1;
		void callback(struct tree	*t, struct dg	*d)
			{ t->shortlabel = label_dag(d, "?"); }
		struct dg	*dag = reconstruct_tree(t, callback);
		char	*mrs_string = "";
		if(!dag)
			fprintf(stderr, "unable to reconstruct MRS for the tree I'm supposed to save\n");
		else
		{
			struct mrs	*m = extract_mrs(dag);
			if(m)mrs_string = mrs_to_string(m);
			else fprintf(stderr, "unable to extract_mrs() from the reconstructed DAG\n");
		}
		char	*derivation = tree_to_derivation(t);
		if(!result)result = write_result(S->profile_id, S->parse_id, "0", derivation, mrs_string);
		if(!result)result = write_preference(S->profile_id, S->parse_id, "1", "0");

		free(derivation);
		free_tree(t);
		clear_slab();
	}
	else
	{
		// clear the 'preference' relation and the 'result' relation
		if(!result)result = write_result(S->profile_id, S->parse_id, NULL, NULL, NULL);
		if(!result)result = write_preference(S->profile_id, S->parse_id, NULL, NULL);
	}

	if(result == 0)webreply(f, "200 ok");
	else webreply(f, "500 fail");
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

	end_session(S);
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
		char	*cgi = cgiencode(path);
		fprintf(f, "<a href=\"/private/update?%s\">Automatic Update</a>\n", cgi);
		fprintf(f, "<table>\n");
		for(i=0;i<items->ntuples;i++)
		{
			char	*input = items->tuples[i][item_input];
			char	*iid = items->tuples[i][item_id];
			fprintf(f, "<tr><td><a href=\"/private/parse?profile=%s&id=%s\">%s</a></td>\n",
				cgi, iid, iid);
			fprintf(f, "<td>%s</td></tr>\n",
				input);
		}
		free(cgi);
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

int eq_tree(struct tree	*x, struct tree	*y)
{
	if(! (x->tfrom == y->tfrom && x->tto == y->tto))
		return 0;	// need the same span
//	if(x->label[0]=='"' && y->label[0]=='"')
//		return 1;	// engines may output labels differently for leaf nodes; identical token span is enough.
	if(!x->ndaughters)return 1;

	if(strcmp(x->label, y->label))
	{
		//printf("%s != %s\n", x->label, y->label);
		return 0;
	}
	if(x->ndaughters != y->ndaughters)return 0;
	int	i;
	for(i=0;i<x->ndaughters;i++)
		if(!eq_tree(x->daughters[i], y->daughters[i]))
			return 0;
	return 1;
}

int	get_decis_timer = -1;
int	get_tree_timer = -1;
int	get_forest_timer = -1;
int	unary_closure_timer = -1;
int	count_timer = -1;

long long	update_item(struct tsdb	*gold, struct tsdb	*prof, char	*iid, char	**color)
{
	int result = 0;
	// plan:
	//   read the gold decisions from 'gold'
	//   read the forest from 'prof'
	//   apply those decisions and see how many results are left
	//   if there's a unique result, record it.

	if(get_decis_timer == -1)
	{
		get_decis_timer = new_timer("get decisions");
		get_tree_timer = new_timer("load gold tree");
		get_forest_timer = new_timer("load forest");
		unary_closure_timer = new_timer("unary closure");
		count_timer = new_timer("count solutions");
	}

	start_timer(get_decis_timer);

	char	*gold_pid = get_pid_by_id(gold, iid);
	char	*prof_pid = get_pid_by_id(prof, iid);
	assert(prof_pid);

	printf("[%s]	", iid); fflush(stdout);

	struct constraint	*golddecs = NULL;
	int ngolddecs = gold_pid ? get_decisions(gold, gold_pid, &golddecs) : 0;

	stop_timer(get_decis_timer, ngolddecs>=0?ngolddecs:0);

	if(ngolddecs < 0)
	{
		fprintf(stderr, "unable to get gold decisions for %s\n", iid);
		result = -1;
		goto freeup2;
	}

	printf("{%d decisions}	", ngolddecs); fflush(stdout);

	start_timer(get_tree_timer);

	struct tree	*goldpref_tree = NULL;
	if(gold_pid)
	{
		char	*gold_pref = get_pref_rid(gold, gold_pid);
		if(gold_pref)
		{
			char	*deriv = get_result(gold, gold_pid, gold_pref);
			if(deriv)
			{
				char	*copy = strdup(deriv);
				goldpref_tree = string_to_tree(copy);
				free(copy);
			}
		}
	}

	stop_timer(get_tree_timer, goldpref_tree?1:0);
	start_timer(get_forest_timer);

	struct parse	*P = load_forest(prof, prof_pid);
	stop_timer(get_forest_timer, 1);

	if(!P)
	{
		fprintf(stderr, "unable to get parse forest for %s\n", iid);
		result = -1;
		goto freeup1;
	}

	start_timer(unary_closure_timer);
	struct parse	*newP = do_unary_closure(P);
	free_parse(P);
	stop_timer(unary_closure_timer, 1);
	P = newP;
	if(!P)
	{
		fprintf(stderr, "unable to perform unary closure for %s\n", iid);
		result = -1;
		goto freeup1;
	}

	//printf("loaded parse forest with %d edges (%d roots)\n", P->nedges, P->nroots);
	printf("{%d edges}	", P->nedges); fflush(stdout);

	start_timer(count_timer);
	long long	total = count_remaining_trees(P, NULL, 0);
	stop_timer(count_timer, 1);
	if(!goldpref_tree)
	{
		//fprintf(stderr, "no gold parse for %s ; %lld current parses\n", iid, total);
		if(total == 0)*color = "#aaa";
		else if(total == 1)*color = "lightblue";
		else *color = "#ff8";
		printf("{%lld trees} no gold	", total); fflush(stdout);
		result = 0;
		goto freeup;
	}

	if(!total)
	{
		*color = "#a44";
		printf("{0 trees}	"); fflush(stdout);
		//fprintf(stderr, "no new parses\n");
		result = 0;
		goto freeup;
	}

	start_timer(count_timer);
	long long remaining = count_remaining_trees(P, golddecs, ngolddecs);
	stop_timer(count_timer, 1);
	printf("{%lld / %lld trees active}	", remaining, total); fflush(stdout);
	//printf("under gold decisions, %lld possible tree(s)\n", remaining);
	if(remaining == 0)
	{
		*color = "#f88";
		//fprintf(stderr, "all new parses ruled out\n");
		result = 0;
		goto freeup;
		/*if(1 == tree_satisfies_constraints(goldpref_tree, golddecs, ngolddecs))
			printf(" ... but I agree that the gold stored tree satisfies\n");
		int i;
		print_forest(P);printf("...\n\n\n");
		for(i=0;i<P->nedges;i++)
		{
			struct tb_edge	*e = P->edges[i];
			printf("edge #%d (root=%d):  %lld unpackings, %lld solutions\n",
				e->id, e->is_root, e->unpackings, e->solutions);
		}*/
	}
	if(remaining > 1)
	{
		*color = "#c94";
		result = remaining;	// failed to identify exactly 1 tree
		goto freeup;
	}

	//printf(" that means a unique tree.\n");
	int i;
	for(i=0;i<P->nedges;i++)
		if(P->edges[i]->unpackings == 1 && P->edges[i]->solutions == 1 && P->edges[i]->is_root)break;
	assert(i < P->nedges);
	struct tree	*t = extract_tree(P->edges[i], 0);
	if(!t)
	{
		*color = "purple";
		printf("couldn't extract the tree... not sure whether it's gold.\n");
		result = 1;
		goto freeup;
	}

	if(eq_tree(t, goldpref_tree))
	{
		*color = "green";
		printf("identical	"); fflush(stdout);
		//printf(" ... and it is equivalent to the stored gold tree\n");
	}
	else
	{
		*color = "lightblue";
		printf("different	"); fflush(stdout);
		//printf(" ... and it's not equivalent to the stored gold tree\n");
	}
	free_tree(t);
	result = 1;

freeup:
	if(P)free_parse(P);
freeup1:
	if(goldpref_tree)free_tree(goldpref_tree);
	for(i=0;i<ngolddecs;i++)
		free(golddecs[i].sign);
	if(golddecs)free(golddecs);
freeup2:
	return result;
}

void	web_update(FILE	*f, char	*path)
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

	for(i=0;i<n;i++)
		free(names[i]);
	free(names);

	if(!is_profile) { webreply(f, "500 unable to update a directory; specify a profile."); return; }

	char	*goldpath = NULL;
	if(!strcmp(path, "/trec-test/"))
		goldpath = "/gold/erg/trec/";
	if(!strcmp(path, "/ws01-test/"))
		goldpath = "/gold/erg/ws01/";
	if(!strcmp(path, "/wsj/rwsj00a-full/"))
		goldpath = "/wsj/rwsj00a-topn/";

	if(!goldpath) { webreply(f, "500 I don't know the gold profile for that path."); return; }

	char	goldfullpath[10240];
	sprintf(goldfullpath, "%s%s", tsdb_home_path, goldpath);

	struct tsdb	*profile = cached_get_profile_and_pin(fullpath);
	if(!profile) { webreply(f, "500 unable to read TSDB profile"); return; }
	struct tsdb	*goldprof = cached_get_profile_and_pin(goldfullpath);
	if(!goldprof) { webreply(f, "500 unable to read gold TSDB profile"); return; }

	struct relation	*items = get_relation(profile, "item");
	int	item_input = get_field(items, "i-input", "string");
	int	item_id = get_field(items, "i-id", "integer");

	char	title[10240];
	sprintf(title, "ACE Treebanker: %s", path);
	html_headers(f, title);
	char	*cgi = cgiencode(path);

	// new profile cases:
	//		parsing failed
	//		no parses found
	//		no parses remain
	//		exactly one tree remains
	//		multiple trees remain

	// gold profile cases are the same
	// ... so 5x5 = 25 possible conditions.

/*
	if we already have a preference stored for this item, light green
	 if (new) parsing failed, we just want to indicate that, regardless of what happened before.
	 		bright red.
	 if no parses were found
			if gold profile had no active trees, use a light gray color
			otherwise, indicate a regression with a brownish-reddish color
	 if no parses remain (after applying update) but there were some to begin with
			flag the situation with a light-red color
	 if exactly one tree remains
			if gold profile had 1 active tree and this is it, use bright green
			otherwise use a cautiously optimistic color like light blue
	 if multiple trees remain
			if gold profile had 1 active tree, indicate a regression with an orangish color
			otherwise indicate an unsurprising incomplete disambiguation with a yellowish color
*/

	fprintf(f, "<span style='background: lightgreen;'>This profile has an active tree, not updating</span>\n");
	fprintf(f, "<span style='background: red;'>Unexpected error</span>\n");
	fprintf(f, "<span style='background: #aaa;'>No parses now, no active gold exists</span>\n");
	fprintf(f, "<span style='background: #a44;'>No parses now, active gold exists</span>\n");
	fprintf(f, "<span style='background: #f88;'>Overconstrained now, active gold exists</span>\n");
	fprintf(f, "<span style='background: green;'>Fully disambiguated, identical to stored gold tree</span>\n");
	fprintf(f, "<span style='background: lightblue;'>Fully disambiguated, different from stored gold tree</span>\n");
	fprintf(f, "<span style='background: #c94;'>Remaining ambiguity, was resolved in gold</span>\n");
	fprintf(f, "<span style='background: #ff8;'>Remaining ambiguity, was unresolved in gold</span>\n");
	fprintf(f, "<hr />\n");

	fprintf(f, "<table>\n");
	for(i=0;i<items->ntuples;i++)
	{
		char	*iid = items->tuples[i][item_id];
		char	*input = items->tuples[i][item_input];
		char	*parse_id = get_pid_by_id(profile, iid);
		char	*color = "red";
		if(!parse_id)
			color = "red";
		else
		{
			char	*pref = get_pref_rid(profile, parse_id);
			if(pref)color = "#8f8";
			else
			{
				long long status = update_item(goldprof, profile, iid, &color);
				printf("\n");
				/*if(status == 1)color = "green";
				else if(status == 0)color = "#f88";
				else if(status < 0)color = "red";
				else color = "#ff8";*/
			}
		}
		fprintf(f, "<tr style='background: %s;'><td><a href=\"/private/parse?profile=%s&id=%s\">%s</a></td>\n", color, cgi, iid, iid);
		fprintf(f, "<td>%s</td></tr>\n",
			input);
	}
	free(cgi);
	fprintf(f, "</table>\n");
	html_footers(f);

	unpin_all();
}

/* system for receiving web requests */

void	web_callback(int	fd, void	*ptr, struct sockaddr_in	addr)
{
	char	method[128], path[102400];
	FILE	*f = get_long_http_request(fd, 102300, method, path);
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
		else if(!strncmp(path, "/update?", 8))
			web_update(f, path+8);
		else web_slash(f, path);
		//else webreply(f, "404 not found");
	}
	else
	{
		if(!strncmp(path, "/session?", 9))
			web_session(f, path+9);
		else if(!strncmp(path, "/save?", 6))
			web_save(f, path);
		else fclose(f);
	}
	fflush(stdout);
}

void	pipe_handler(int s)
{
	signal(SIGPIPE, pipe_handler);
}

char	*grammar_ace_image_path = NULL;
char	*grammar_roots = "root_strict root_inffrag root_informal root_frag";

main(int	argc, char	*argv[])
{
	if(argc == 1)grammar_ace_image_path = ERG_PATH;
	else
	{
		grammar_ace_image_path = argv[1];
		if(argc > 2)grammar_roots = argv[2];
	}

	int	fd = tcpip_list(9080);
	setlocale(LC_ALL, "");
	register_server_fd(fd, web_callback, NULL);
	signal(SIGINT, quit_server_event_loop);
	signal(SIGPIPE, pipe_handler);
	ace_load_grammar(grammar_ace_image_path);
	//daemonize("web.log");
	server_event_loop();
	report_timers();
}
