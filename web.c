#define	_GNU_SOURCE
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<assert.h>
#include	<signal.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<dirent.h>
#include	<wchar.h>
#include	<locale.h>
#include	<getopt.h>

#include	<liba.h>
#include	<tsdb.h>

#include	<ace/lexicon.h>
#include	<ace/hash.h>
#include	<ace/mrs.h>

#include	<ace/tree.h>
#include	<ace/reconstruct.h>

#include	"treebank.h"

#define	ERG_PATH	"/home/sweagles/grammars/erg/erg.dat"
#define	DEFAULT_PORT	9080

char	*assets_path = "assets";
char	*tsdb_home_path = "/home/sweagles/logon/lingo/lkb/src/tsdb/home/";

char	*only_tsdb_profile = NULL, *gold_tsdb_profile = NULL;

extern int use_connl_tokenization;
int suppress_bridges = 0;

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

char	*date_string(struct timeval	tv)
{
	char	buffer[1024];
	time_t	time = tv.tv_sec;
	// want:   5-sep-2013 14:05:30
	strftime(buffer, 1000, "%d-%m-%Y %T", localtime(&time));
	printf("date string: %s\n", buffer);
	return strdup(buffer);
}

void	add_tuple(struct relation	*r, char	**tup)
{
	int	j;

	for(j=0;j<r->nfields;j++)
	{
		if(!tup[j])
		{
			char	*type = r->fields[j].type;
			if(!strcasecmp(type, "integer"))tup[j] = strdup("-1");
			else if(!strcasecmp(type, "string"))tup[j] = strdup("");
			else if(!strcasecmp(type, "date"))tup[j] = strdup("23-6-2013 14:28:24");	// XXX fix me
			else
			{
				fprintf(stderr, "unknown tsdb type ':%s'\n", type);
				tup[j] = strdup("");
			}
		}
	}
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

struct tsdb	*get_pinned_profile(char	*path)
{
	unpin_all();
	struct tsdb	*t = cached_get_profile_and_pin(path);
	return t;
}

purge_tuples(struct relation	*r, int	field, char	*value)
{
	int i, j;
	for(i=j=0;i<r->ntuples;i++)
	{
		if(!strcmp(r->tuples[i][field], value))
		{
			// modified libtsdb now stores all tuples in the same buffer (per row)
			free(r->tuples[i][0]);
			//int k;
			//for(k=0;k<r->nfields;k++)free(r->tuples[i][k]);
		}
		else r->tuples[j++] = r->tuples[i];
	}
	r->ntuples = j;
}

int	write_tree(char	*prof_path, char	*parse_id, char	*t_version, char	*t_active, char	*author, char	*comment, char	*t_start, char	*t_end)
{
	struct tsdb	*t = get_pinned_profile(prof_path);
	struct relation	*r = t?get_relation(t, "tree"):NULL;
	if(!t || !r)return -1;

	//printf("write_tree(): t = %p  --  %s - %s\n", t, t_start, t_end);

	int	tree_parse_id = get_field(r, "parse-id", "integer");
	int	tree_t_version = get_field(r, "t-version", "integer");
	int	tree_t_active = get_field(r, "t-active", "integer");
	int	tree_t_author = get_field(r, "t-author", "string");
	int	tree_t_comment = get_field(r, "t-comment", "string");
	int	tree_t_start = get_field(r, "t-start", "date");
	int	tree_t_end = get_field(r, "t-end", "date");

	char	*tup[r->nfields];
	bzero(tup, sizeof(tup));
	tup[tree_parse_id] = strdup(parse_id);
	tup[tree_t_version] = strdup(t_version);
	tup[tree_t_active] = strdup(t_active);
	tup[tree_t_author] = strdup(author?:"");
	tup[tree_t_comment] = strdup(comment);
	tup[tree_t_start] = t_start?strdup(t_start):NULL;
	tup[tree_t_end] = t_end?strdup(t_end):NULL;

	// purge all old records referring to this parse_id from the relation 
	purge_tuples(r, tree_parse_id, parse_id);

	add_tuple(r, tup);
	if(reindex_and_write(t,r) != 0)return -1;

	return 0;
}

int	write_parse_result_count(char	*prof_path, char	*parse_id, char	*readings)
{
	struct tsdb	*t = get_pinned_profile(prof_path);
	struct relation	*r = t?get_relation(t, "parse"):NULL;
	if(!t || !r)return -1;

	int	pref_parse_id = get_field(r, "parse-id", "integer");
	int	pref_readings = get_field(r, "readings", "integer");

	char	**tup = tsdb_lookup_relation_with_key(r, pref_parse_id, parse_id);
	if(!tup)
	{
		fprintf(stderr, "wanted to write readings=%s for parse-id %s, but couldn't find that tuple\n", readings, parse_id);
		return -1;
	}

	tup[pref_readings] = strdup(readings);
	if(reindex_and_write(t,r) != 0)return -1;

	return 0;
}

int	write_preference(char	*prof_path, char	*parse_id, char	*t_version, char	*result_id)
{
	struct tsdb	*t = get_pinned_profile(prof_path);
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

int	write_result(char	*prof_path, char	*parse_id, char	*result_id, char	*derivation, char	*mrs)
{
	struct tsdb	*t = get_pinned_profile(prof_path);
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
	{
		len += 10;
		len += strlen(t->tokens[i]);
	}
	return len;
}

char	*dqescape(char *s)
{
	char *s2 = malloc(strlen(s)*2+1), *p = s2;
	while(*s)
	{
		if(*s=='\b') { *p++ = '\\'; *p++ = 'b'; s++; continue; }
		if(*s=='\f') { *p++ = '\\'; *p++ = 'f'; s++; continue; }
		if(*s=='\n') { *p++ = '\\'; *p++ = 'n'; s++; continue; }
		if(*s=='\r') { *p++ = '\\'; *p++ = 'r'; s++; continue; }
		if(*s=='\t') { *p++ = '\\'; *p++ = 't'; s++; continue; }
		// specs for json apparently say you should escape '/'...
		if(*s == '\'' || *s == '\\' || *s == '"' || *s=='/')*p++='\\';
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
			str += sprintf(str, " %d \"%s\"", t->tokenids[i], e);
			free(e);
		}
	}
	*str++ = ')';
	*str = 0;
	return str;
}

char	*tree_to_derivation(struct tree	*t)
{
	struct dg	*dg = reconstruct_tree_or_error(t, NULL, NULL);
	assert(dg != NULL);
	char	*root = is_root(dg);
	if(root == NULL)
	{
		fprintf(stderr, "WARNING: the selected tree didn't match any root symbols.\n");
		root = "NO_SUCH_ROOT";
	}
	int	len = len_of_derivation(t) + strlen(root) + 10;
	char	*str = malloc(len+1);
	char	*str_orig = str;
	str += sprintf(str, "(%s ", root);
	str = build_derivation(str, t);
	str += sprintf(str, ")");
	return str_orig;
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

int	save_tree_for_item(char	*profile_id, char	*parse_id, struct tree	*t)
{
	void callback(struct tree	*t, struct dg	*d)
		{ t->shortlabel = label_dag(d, "?"); }
	struct dg	*dag = reconstruct_tree(t, callback);
	char	*mrs_string = "";
	if(!dag)
	{
		fprintf(stderr, "unable to reconstruct MRS for the tree I'm supposed to save\n");
		clear_mrs();
		clear_slab();
		return -1;
	}
	else
	{
		clear_mrs();
		struct mrs	*m = extract_mrs(dag);
		if(m)mrs_string = mrs_to_string(m);
		else
		{
			fprintf(stderr, "unable to extract_mrs() from the reconstructed DAG\n");
			clear_mrs();
			clear_slab();
			return -2;
		}
	}
	char	*derivation = tree_to_derivation(t);
	int res = write_result(profile_id, parse_id, "0", derivation, mrs_string);
	if(!res)res = write_preference(profile_id, parse_id, "1", "0");
	if(!res)res = write_parse_result_count(profile_id, parse_id, "1");
	free(derivation);
	clear_mrs();
	clear_slab();
	return res;
}

void	web_save(FILE	*f, char	*query)
{
	int	id, accepted;
	if(2 != sscanf(query, "/save?%d&accepted=%d", &id, &accepted)) { webreply(f, "500 internal error"); return; }
	struct session	*S = get_session(id);
	if(!S) { webreply(f, "404 no such session"); return; }
	if(!S->parse_id) { webreply(f, "500 no parse id"); return; }

	char	prof_path[10240];
	sprintf(prof_path, "%s/%s", tsdb_home_path, S->profile_id);

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
	int result = 0;

	switch(accepted)
	{
		case	-1:
			if(result == 0)webreply(f, "200 ok");
			else webreply(f, "500 fail");
			free(cons);
			return;	// don't write tree, preference, result unless 'accept' or 'reject' was clicked
		case	1:
			result = save_decisions(prof_path, S->parse_id, ncons, cons);
			break;
		case	0:
			result = save_decisions(prof_path, S->parse_id, 0, NULL);	// clear out saved decisions for this item if we reject
			break;
		default:
			webreply(f, "500 invalid acceptance status");
			return;
	}

	long long	remaining = count_remaining_trees(S->parse, cons, ncons);//S->local_dec, S->nlocal_dec);
	printf("remaining = %lld [%d decisions used]\n", remaining, ncons);
	if(accepted)assert(remaining == 1);
	free(cons);

	// write a record to 'tree'

	if(!result)
	{
		struct timeval	t_end;
		gettimeofday(&t_end, NULL);
		char	*st_start = date_string(S->t_start);
		char	*st_end = date_string(t_end);
		result = write_tree(prof_path, S->parse_id, "1", accepted?"1":"0", getenv("LOGNAME"), S->comment, st_start, st_end);
		free(st_start);
		free(st_end);
	}

	if(accepted == 1)
	{
		// save one result to the 'result' relation and a pointer to it in 'preference'
		// count_remaining_trees() has set up the right flags for extract_tree()
		for(i=0;i<S->parse->nedges;i++)
			if(S->parse->edges[i]->unpackings == 1 && S->parse->edges[i]->solutions == 1 && S->parse->edges[i]->is_root)break;
		assert(i < S->parse->nedges);
		struct tree	*t = extract_tree(S->parse->edges[i], 0);
		if(!t)result = -1;
		if(!result)result = save_tree_for_item(prof_path, S->parse_id, t);

		free_tree(t);
	}
	else
	{
		// clear the 'preference' relation and the 'result' relation
		if(!result)result = write_result(prof_path, S->parse_id, NULL, NULL, NULL);
		if(!result)result = write_preference(prof_path, S->parse_id, NULL, NULL);
	}

	if(result == 0)webreply(f, "200 ok");
	else webreply(f, "500 fail");
}

void	web_no_such_direction(FILE	*f, int	dir, int	unan, struct session	*S)
{
	html_headers(f, "Oops!");
	fprintf(f, "<h2>There is no%s active item %s %s.</h2>\n", unan?" unannotated":"", (dir>1)?"after":"before", S->item_id);
	fprintf(f, "<script>window.location=\"/private%s\"</script>\n", S->profile_id);
	html_footers(f);
}

void	web_nav(FILE	*f, char	*query)
{
	assert(strlen(query) >= 6);
	int dir = 0, unan = 0;
	char	*idptr = query + 6;
	if(!strncmp(query, "/next?", 6))dir = +1;
	else if(!strncmp(query, "/next_unannotated?", 18)){dir = +1;unan=1;idptr = query+18;}
	else dir = -1;

	int	id = atoi(idptr);
	struct session	*S = get_session(id);
	if(!S) { webreply(f, "404 no such session"); return; }

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

	// skip forward or backward until we come to an unannotated item
	char	*new_id, *parse_id;
	do
	{
		i += dir;
		if(i < 0 || i >= items->ntuples)
		{ 
			tsdb_free_profile(profile);
			web_no_such_direction(f, dir, unan, S);
			//webreply(f, "404 no such item");
			return;
		}
		new_id = items->tuples[i][item_id];
		parse_id = get_parse_id_p(profile, new_id);
	} while((unan && -1 != get_t_active_p(profile, parse_id)) || !iid_is_active(new_id));

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

int	get_t_active(char	*prof_id, char	*parse_id)
{
	char	prof_path[10240];
	sprintf(prof_path, "%s/%s", tsdb_home_path, prof_id);
	struct tsdb	*t = cached_get_profile_and_pin(prof_path);
	return get_t_active_p(t, parse_id);
}

int	get_t_active_p(struct tsdb	*t, char	*parse_id)
{
	struct relation	*tree = get_relation(t, "tree");
	int	t_parse_id = get_field(tree, "parse-id", "integer"),
		t_t_active = get_field(tree, "t-active", "integer");
	char	**tuple = tsdb_lookup_relation_with_key(tree, t_parse_id, parse_id);
	if(!tuple)return -1;
	return atoi(tuple[t_t_active]);
}

char	*get_t_end_p(struct tsdb	*t, char	*parse_id)
{
	struct relation	*tree = get_relation(t, "tree");
	int	t_parse_id = get_field(tree, "parse-id", "integer"),
		t_t_end = get_field(tree, "t-end", "date");
	char	**tuple = tsdb_lookup_relation_with_key(tree, t_parse_id, parse_id);
	if(!tuple)return "";
	return tuple[t_t_end];
}

char	*get_t_end(char	*prof_id, char	*parse_id)
{
	char	prof_path[10240];
	sprintf(prof_path, "%s/%s", tsdb_home_path, prof_id);
	struct tsdb	*t = cached_get_profile_and_pin(prof_path);
	return get_t_end_p(t, parse_id);
}
char	*get_t_start_p(struct tsdb	*t, char	*parse_id)
{
	struct relation	*tree = get_relation(t, "tree");
	int	t_parse_id = get_field(tree, "parse-id", "integer"),
		t_t_start = get_field(tree, "t-start", "date");
	char	**tuple = tsdb_lookup_relation_with_key(tree, t_parse_id, parse_id);
	if(!tuple)return "";
	return tuple[t_t_start];
}

char	*get_t_start(char	*prof_id, char	*parse_id)
{
	char	prof_path[10240];
	sprintf(prof_path, "%s/%s", tsdb_home_path, prof_id);
	struct tsdb	*t = cached_get_profile_and_pin(prof_path);
	return get_t_start_p(t, parse_id);
}

char	*get_t_comment(char	*prof_id, char	*parse_id)
{
	char	prof_path[10240];
	sprintf(prof_path, "%s/%s", tsdb_home_path, prof_id);
	struct tsdb	*t = cached_get_profile_and_pin(prof_path);
	return get_t_comment_p(t, parse_id);
}

char	*get_t_comment_p(struct tsdb	*t, char	*parse_id)
{
	struct relation	*tree = get_relation(t, "tree");
	int	t_parse_id = get_field(tree, "parse-id", "integer"),
		t_t_comment = get_field(tree, "t-comment", "string");
	char	**tuple = tsdb_lookup_relation_with_key(tree, t_parse_id, parse_id);
	if(!tuple)return "";
	return tuple[t_t_comment];
}

char	*get_parse_id_p(struct tsdb	*t, char	*item_id)
{
	struct relation	*parse = get_relation(t, "parse");
	int	p_parse_id = get_field(parse, "parse-id", "integer"),
		p_i_id = get_field(parse, "i-id", "integer");
	char	**tuple = tsdb_lookup_relation_with_key(parse, p_i_id, item_id);
	if(!tuple)return "-1";
	return tuple[p_parse_id];
}

char	*status_string(int	t_active)
{
	if(t_active==-1)return "unannotated";
	if(t_active==0)return "rejected";
	return "accepted";
}

char	*status_color(int	t_active)
{
	if(t_active==-1)return "#ff0";
	if(t_active==0)return "#f00";
	return "#0f0";
}

void	web_exit(FILE	*f)
{
	http_headers(f, "text/html");
	fprintf(f, "<html><body onload=\"window.open('', '_self', ''); window.close(); self.close();\">Goodbye! You may safely close this window.</body></html>\n");
	fclose(f);
	quit_server_event_loop();
}

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
	int i,n = scandir(fullpath, &names, (int(*)(const struct dirent*))filter, alphasort);
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
		sprintf(title, "FFTB: %s", path);
		html_headers(f, title);
		char	*cgi = cgiencode(path);
		if(gold_tsdb_profile)
			fprintf(f, "<a href=\"/private/update?%s\">Automatic Update</a>  | ", cgi);
		fprintf(f, "<a href=\"/private/exit\">Exit</a><br/>\n");
		fprintf(f, "<table>\n");
		for(i=0;i<items->ntuples;i++)
		{
			char	*input = items->tuples[i][item_input];
			char	*iid = items->tuples[i][item_id];
			if(!iid_is_active(iid))continue;
			fprintf(f, "<tr><td><a href=\"/private/parse?profile=%s&id=%s\">%s</a></td>\n",
				cgi, iid, iid);
			char	*parse_id = get_parse_id_p(profile, iid);
			int	t_active = get_t_active_p(profile, parse_id);
			fprintf(f, "<td></td><td style='background: %s;'>&nbsp;</td><td></td>\n", status_color(t_active));
			if(use_connl_tokenization)
			{
				char	*simplify_connl_input(char	*input);
				input = simplify_connl_input(input);
			}
			fprintf(f, "<td>%s</td></tr>\n",
				input);
			if(use_connl_tokenization)free(input);
		}
		free(cgi);
		fprintf(f, "</table>\n");
		html_footers(f);
		tsdb_free_profile(profile);
	}
	else
	{
		html_headers(f, "FFTB Home");
		fprintf(f, "<a href=\"/private/exit\">[Exit Treebanker]</a><hr/>\n");
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

int	update_manual_decisions_only = 0;

long long	update_item(struct tsdb	*gold, struct tsdb	*prof, char	*iid, char	**color)
{
	int result = 0;
	int	success = 0;
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

	if(update_manual_decisions_only)
	{
		int k, nm = 0;
		struct constraint	*mandecs = calloc(sizeof(*mandecs),ngolddecs);
		for(k=0;k<ngolddecs;k++)
			if(!golddecs[k].inferred)
				mandecs[nm++] = golddecs[k];
		golddecs = mandecs;
		ngolddecs = nm;
	}

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
	if(remaining>1 && suppress_bridges)
	{
		printf(" (suppressing bridges)");
		add_bridge_suppression(P, &golddecs, &ngolddecs);
		remaining = count_remaining_trees(P, golddecs, ngolddecs);
	}
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

		printf(" ... saving this tree as preferred, with decisions from gold");
		int result = save_decisions(prof->path, prof_pid, ngolddecs, golddecs);
		char	*comment = get_t_comment_p(gold, gold_pid);
		char	*gold_t_start = get_t_start_p(gold, gold_pid);
		char	*gold_t_end = get_t_end_p(gold, gold_pid);
		if(!result)result = write_tree(prof->path, prof_pid, "1", "1", getenv("LOGNAME"), comment, gold_t_start, gold_t_end);
		if(!result)result = save_tree_for_item(prof->path, prof_pid, t);
		printf(" [ %s ]", result?"fail":"success");
		if(!result)success = 1;
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
	if(prof && prof_pid && !success)
	{
		char	*comment = (gold && gold_pid)?get_t_comment_p(gold, gold_pid):"";
		write_tree(prof->path, prof_pid, "1", "-1", getenv("LOGNAME"), comment, NULL, NULL);
	}
	return result;
}

char	*get_gold_profile_path(char	*profile_id)
{
	char	*goldpath = NULL;
	/*if(path)
	{
		if(!strcmp(path, "/trec-test/"))
			goldpath = "/gold/erg/trec/";
		if(!strcmp(path, "/ws01-test/"))
			goldpath = "/gold/erg/ws01/";
		if(!strcmp(path, "/wsj/rwsj00a-full/"))
			goldpath = "/wsj/rwsj00a-topn/";
	}*/

	if(!gold_tsdb_profile && !goldpath)return NULL;

	char	goldfullpath[10240];
	if(gold_tsdb_profile)strcpy(goldfullpath, gold_tsdb_profile);
	else sprintf(goldfullpath, "%s%s", tsdb_home_path, goldpath);

	return strdup(goldfullpath);
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
	char	fullpath[10240];
	if(path)
	{
		if(strchr(path, '.') || strlen(path)>512)
		{ webreply(f, "500 bad path"); return; }
		sprintf(fullpath, "%s%s", tsdb_home_path, path);
	}
	else
	{
		assert(only_tsdb_profile && gold_tsdb_profile);
		strcpy(fullpath, only_tsdb_profile);
	}
	struct dirent	**names;
	int i,n = scandir(fullpath, &names, (int(*)(const struct dirent*))filter, alphasort);
	if(n < 0) { webreply(f, "500 unable to read TSDB home directory"); return; }
	for(i=0;i<n;i++)
		free(names[i]);
	free(names);

	if(!is_profile) { webreply(f, "500 unable to update a directory; specify a profile."); return; }

	/*char	*goldpath = NULL;
	if(path)
	{
		if(!strcmp(path, "/trec-test/"))
			goldpath = "/gold/erg/trec/";
		if(!strcmp(path, "/ws01-test/"))
			goldpath = "/gold/erg/ws01/";
		if(!strcmp(path, "/wsj/rwsj00a-full/"))
			goldpath = "/wsj/rwsj00a-topn/";
	}

	if(!gold_tsdb_profile && !goldpath) { webreply(f, "500 I don't know the gold profile for that path."); return; }

	char	goldfullpath[10240];
	if(gold_tsdb_profile)strcpy(goldfullpath, gold_tsdb_profile);
	else sprintf(goldfullpath, "%s%s", tsdb_home_path, goldpath);*/
	char	*goldfullpath = get_gold_profile_path(path);
	if(!goldfullpath) { webreply(f, "500 I don't know the gold profile for that path."); return; }

	struct tsdb	*profile = cached_get_profile_and_pin(fullpath);
	if(!profile) { webreply(f, "500 unable to read TSDB profile"); return; }
	struct tsdb	*goldprof = cached_get_profile_and_pin(goldfullpath);
	free(goldfullpath); goldfullpath = NULL;
	if(!goldprof) { webreply(f, "500 unable to read gold TSDB profile"); return; }

	struct relation	*items = get_relation(profile, "item");
	int	item_input = get_field(items, "i-input", "string");
	int	item_id = get_field(items, "i-id", "integer");

	char	*cgi = "";

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

	if(path)
	{
		char	title[10240];
		sprintf(title, "FFTB: %s", path);
		html_headers(f, title);
		cgi = cgiencode(path);
		fprintf(f, "Automatically updating items in %s <a href=\"/private/exit\">Exit</a><br/>\n", path);
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
	}
	for(i=0;i<items->ntuples;i++)
	{
		char	*iid = items->tuples[i][item_id];
		if(!iid_is_active(iid))continue;
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
		if(path)
		{
			fprintf(f, "<tr style='background: %s;'><td><a href=\"/private/parse?profile=%s&id=%s\">%s</a></td>\n", color, cgi, iid, iid);
			fprintf(f, "<td>%s</td></tr>\n",
				input);
		}
	}
	if(path)
	{
		free(cgi);
		fprintf(f, "</table>\n");
		html_footers(f);
	}

	unpin_all();
}

/* system for receiving web requests */

void	webrelativefile(FILE	*f, char	*mime, char	*parent, char	*sub)
{
	char	path[10240];
	sprintf(path, "%s/%s", parent, sub);
	webfile(f, mime, path);
}

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
			webrelativefile(f, "text/javascript", assets_path, "render.js");
		else if(!strcmp(path, "/assets/control.js"))
			webrelativefile(f, "text/javascript", assets_path, "control.js");
		else if(!strncmp(path, "/session?", 9))
			webrelativefile(f, "text/html", assets_path, "index.html");
		else if(!strncmp(path, "/next?", 6) || !strncmp(path, "/prev?", 6))
			web_nav(f, path);
		else if(!strncmp(path, "/next_unannotated?", 18))
			web_nav(f, path);
		else if(!strncmp(path, "/update?", 8))
			web_update(f, path+8);
		else if(!strcmp(path, "/exit"))
			web_exit(f);
		else web_slash(f, path);
		//else webreply(f, "404 not found");
	}
	else
	{
		if(!strncmp(path, "/session?", 9))
			web_session(f, path+9);
		else if(!strncmp(path, "/comment?", 9))
			web_comment(f, path+9);
		else if(!strncmp(path, "/save?", 6))
			web_save(f, path);
		//else if(!strncmp(path, "/island?", 8))
			//web_island(f, path+8);
		else fclose(f);
	}
	fflush(stdout);
}

void	pipe_handler(int s)
{
	signal(SIGPIPE, pipe_handler);
}

launch_browser(char	*browsername, char	*url)
{
	char	command[10240];
#if defined(__APPLE__)
	sprintf(command, "open %s", url);
#else
	sprintf(command, "%s %s &", browsername, url);
#endif
	int result = system(command);
	if(result != 0)fprintf(stderr, "unable to launch browser: %s\n", browsername);
}

char	*grammar_roots = "root_strict root_inffrag root_informal root_frag";	// used when invoking ACE online

extern int allow_islands;
struct option long_options[] = {
#define	GOLD_OPTION	1001
	{"gold", required_argument, NULL, GOLD_OPTION},
	{"auto", no_argument, NULL, 'a'},
	{"items", required_argument, NULL, 'i'},
	{"browser", optional_argument, NULL, 'b'},
	{"webdir", required_argument, NULL, 'w'},
	{"port", required_argument, NULL, 'p'},
	{"connl-tokenization", no_argument, &use_connl_tokenization, 1},
	{"manual-decisions-only", no_argument, &update_manual_decisions_only, 1},
	{"islands", no_argument, &allow_islands, 1},
	{"suppress-bridges", no_argument, &suppress_bridges, 1},
#define	RANDOM_OPTION	1002
	{"random", optional_argument, NULL, RANDOM_OPTION},
	{NULL,0,NULL,0}
	};

struct hash	*item_list_hash = NULL;

hash_item_list(char	*items)
{
	char	*iid, *sep=" \t\r\n,";	// generous set of iid separators...
	item_list_hash = hash_new("active item list");
	for(iid=strtok(items, sep);iid;iid=strtok(NULL, sep))
		hash_add(item_list_hash, strdup(iid), (void*)0x1);
}

iid_is_active(char	*iid)
{
	if(item_list_hash != NULL)
	{
		if(hash_find(item_list_hash, iid))return 1;
		else return 0;
	}
	else return 1;
}

usage(char	*app, int status)
{
	printf("usage:  %s -g grammar.dat [--gold profile_path [--auto]] [--browser [firefox]] [--webdir web_path] [--port port_number] profile_path\n", app);
	exit(status);
}

main(int	argc, char	*argv[])
{
	int	ch, randomize=0, browser = 0, autoupdate = 0, port = 0;
	char	*browsername = "firefox";
	char	*randomize_mode = "uniform";
	while( (ch = getopt_long(argc, argv, "Vhg:ab;i:w:", long_options, NULL)) != -1) switch(ch)
	{
		case	GOLD_OPTION: gold_tsdb_profile = optarg; break;
		case	'g':
			printf("grammar image: %s\n", optarg);
			ace_load_grammar(optarg);
			break;
		case	'a': autoupdate = 1; break;
		case	'b': browser = 1; if(optarg)browsername = optarg; break;
		case	'i': hash_item_list(optarg); break;
		case	'w': assets_path = optarg; break;
		case	'p': port = atoi(optarg); break;
		case	RANDOM_OPTION: randomize = 1; if(optarg)randomize_mode = optarg; break;
		case	'V': case	'h':	usage(argv[0],0);
		case	0:	continue;
		case	'?': usage(argv[0],-1);
	}
	if(argc != optind+1)usage(argv[0],-1);

	tsdb_home_path = argv[optind];
	if(gold_tsdb_profile)
	{
		only_tsdb_profile = tsdb_home_path;
		printf("Just one TSDB profile: %s\n", only_tsdb_profile);
		printf("Would update from profile: %s\n", gold_tsdb_profile);
	}

	if(autoupdate)
	{
		assert(gold_tsdb_profile != NULL);
		web_update(stdout, NULL);
		return 0;
	}
	if(randomize)
	{
		random_annotate(tsdb_home_path, randomize_mode);
		return 0;
	}
	if(!port && !browser)port = DEFAULT_PORT;
	struct sockaddr_in	addr;
	socklen_t	sl = sizeof(addr);
	bzero(&addr, sl);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = 0; // inet_addr("127.0.0.1");
	addr.sin_port = htons(port);
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd>=0);
	int r = bind(fd, (struct sockaddr*)&addr, sl);
	assert(r == 0);
	if(port == 0)
	{
		r = getsockname(fd,(struct sockaddr*)&addr,&sl);
		assert(r == 0);
		assert(sl == sizeof(addr));
		port = ntohs(addr.sin_port);
	}
	char	url[1024];
	sprintf(url, "http://127.0.0.1:%d/private/", port);
	if(listen(fd, 128)) { perror("listen"); exit(-1); }
	printf("listening on %s\n", url);
	if(browser)launch_browser(browsername, url);
	setlocale(LC_ALL, "");
	register_server_fd(fd, web_callback, NULL);
	signal(SIGINT, quit_server_event_loop);
	signal(SIGPIPE, pipe_handler);
	//daemonize("web.log");
	server_event_loop();
	report_timers();
	return 0;
}
