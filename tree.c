#include	<math.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>

#include	"tree.h"

int	print_tokens = 1;

char	*tree_trim_string(char	*str)
{
	int	len;

	while(*str==' ' || *str=='\t' || *str=='\n')str++;
	len = strlen(str);
	if(len==0)return str;
	while(len>0 && (str[len-1]==' ' || str[len-1]=='\t' || str[len-1]=='\n'))len--;
	str[len] = 0;
	return str;
}

struct tree	*string_to_tree_leaf(char	*str)
{
	// token node
	struct tree	*t = calloc(sizeof(struct tree), 1);
	t->cfrom = t->cto = -1;
	t->tfrom = t->tto = -1;
	char	*q = str+1;
	int	escape = 0;
	while(*q && !(*q=='"' && !escape))
	{
		if(escape)escape = 0;
		else if(*q=='\\')escape=1;
		q++;
	}
	*q = 0;
	t->label = strdup(str);
	while(q[1])
	{
		// newer style with token structure string
		q++;
		//printf("after passing token id, q = '%s'\n", q);
		while(*q==' ')q++;
		if(*q=='-' || *q=='+')q++;
		while(isdigit(*q))q++;
		while(*q==' ')q++;
		if(*q == '"')
		{
			q++;
			str = q;
			char	*p = q;
			while(*q && !(*q=='"' && !escape))
			{
				if(escape)escape = 0;
				else if(*q=='\\')escape=1;
				if(!escape)*p++ = *q;
				q++;
			}
			*p = 0;
			char	*tok = strdup(str);
			char	*cfrom = strstr(tok, "+FROM \"");
			char	*cto = strstr(tok, "+TO \"");
			if(cfrom && cto)
			{
				cfrom += 7;
				cto += 5;
				int	tcfrom = atoi(cfrom);
				int	tcto = atoi(cto);
				if(t->cfrom==-1)t->cfrom = tcfrom;
				t->cto = tcto;
			}
			t->ntokens++;
			t->tokens = realloc(t->tokens, sizeof(char*) * t->ntokens);
			t->tokens[t->ntokens-1] = tok;
			while(q[1]==' ')q++;
		}
	}
	return t;
}

// this call accounts for most of the time spent parsing trees;
// it visits each position in the input string once per constituent containing that position.
// a better algorithm (if we care) would have string_to_tree1() return an updated string position,
// so that we wouldn't need find_end_of_daughter() to be able to skip over the input consumed by recursive calls.
// I don't know how much faster string_to_tree() would get, but I'm guessing a lot.
// it could be a worthwhile optimization for the profile comparison tool in libtsdb...
// really, this file should be part of some library that everyone links to
// (along with reconstruct.[ch]), rather than having lots of copies of it floating around.
char	*find_end_of_daughter(char	*dp)
{
	char	*edp = dp+1;
	int		npar = 1, inq = 0;
	while(npar>0 && *edp) switch(*edp++) {
		case '\\': if(*edp)edp++; break;
		case '"': inq = !inq; break;
		case '(': if(!inq)npar++; break;
		case ')': if(!inq)npar--; break;
	}
	if(!(npar==0 && inq==0))
	{
		printf("bad daughter tree: '%s'\n", dp);
		assert(npar==0 && inq==0);
	}
	return edp;
}

struct tree	*string_to_tree(char	*str)
{
	str = tree_trim_string(str);
	//printf("parsing tree: `%s'\n", str);
	assert(*str == '(');
	str++;
	int		len = strlen(str);
	assert(len>1);
	char	*end = str + len-1;
	if(!(*end == ')'))
	{
		printf("bad tree string: '%s'\n", str);
		assert(*end == ')');
	}
	*end = 0;
	str = tree_trim_string(str);

	if(isdigit(*str))
	{
		// rule or lexeme node
		char	*dp = strchr(str, '(');
		char	*tmp;
		char	*edge_number_string = strtok_r(str, " \t", &tmp);
		char	*type = strtok_r(NULL, " \t", &tmp);
		char	*score_number_string = strtok_r(NULL, " \t", &tmp);
		assert(score_number_string != NULL);
		char	*token_from_string = strtok_r(NULL, " \t", &tmp);
		assert(token_from_string != NULL);
		char	*token_to_string = strtok_r(NULL, " \t", &tmp);
		assert(token_to_string != NULL);

		struct tree	*t = calloc(sizeof(struct tree), 1);
		t->edge_id = atoi(edge_number_string);
		t->label = strdup(type);
		t->cfrom = t->cto = -1;
		t->tfrom = atoi(token_from_string);
		t->tto = atoi(token_to_string);
		t->score = atof(score_number_string);
		while(dp && *dp)
		{
			char	*edp = find_end_of_daughter(dp);
			*edp++ = 0;	// found end of daughter tree
			struct tree	*d = string_to_tree(dp);
			t->ndaughters++;
			t->daughters = realloc(t->daughters, sizeof(void*)*t->ndaughters);
			t->daughters[t->ndaughters-1] = d;
			if(d->cfrom != -1 && (t->cfrom==-1 || d->cfrom<t->cfrom))
				t->cfrom = d->cfrom;
			if(d->cto != -1 && (t->cto==-1 || d->cto>t->cto))
				t->cto = d->cto;
			if(d->tfrom == -1)d->tfrom = t->tfrom;
			if(d->tto == -1)d->tto = t->tto;
			d->parent = t;
			dp = strchr(edp, '(');
		}
		return t;
	}
	else if(*str=='"')
	{
		return string_to_tree_leaf(str);
	}
	else
	{
		// root node
		char	*dp = strchr(str, '(');
		return string_to_tree(tree_trim_string(dp));
	}
}

print_tree(struct tree	*t, int	indent)
{
	int i;
	for(i=0;i<indent;i++)printf("  ");
	if(t->cfrom==-1 || t->cto==-1)
		printf("%s    [s=%f]\n", t->label, t->score);
	else printf("%s    [s=%f]   [%d-%d]\n", t->label, t->score, t->cfrom, t->cto);
	if(t->ntokens && print_tokens)
	{
		int j;
		for(j=0;j<t->ntokens;j++)
		{
			for(i=0;i<indent;i++)printf("  ");
			printf(" => %s\n", t->tokens[j]);
		}
	}
	/*if(t->decoration)
	{
		for(i=0;i<indent;i++)printf("  ");
		print_tree_decoration(t->decoration);
	}*/
	for(i=0;i<t->ndaughters;i++)
		print_tree(t->daughters[i], indent+1);
}

struct tree	*find_subtree_with_crange(struct tree	*t, int	from, int	to)
{
	if(t->cfrom==from && t->cto==to)return t;

	//printf("looking for subtree [%d-%d] in tree [%d-%d]\n", from, to, t->cfrom, t->cto);

	int i;
	for(i=0;i<t->ndaughters;i++)
	{
		struct tree	*d = t->daughters[i];
		if(d->cfrom > to || d->cto < from)
			continue;
		return find_subtree_with_crange(d, from, to);
	}
	return t;
}

void	free_tree(struct tree	*t)
{
	int	i;
	for(i=0;i<t->ndaughters;i++)
		free_tree(t->daughters[i]);
	for(i=0;i<t->ntokens;i++)
		free(t->tokens[i]);
	if(t->daughters)free(t->daughters);
	if(t->tokens)free(t->tokens);
	if(t->label)free(t->label);
	//if(t->decoration)free_tree_decoration(t->decoration);
	free(t);
}
