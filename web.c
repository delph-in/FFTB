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
	daemonize("web.log");
	server_event_loop();
}
