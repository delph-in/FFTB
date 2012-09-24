#include	<math.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>

#include	"tree.h"

#define	FAIL(reason, location)	do { fprintf(stderr, "string_to_tree: %s near %.30s\n", reason, location); return NULL; } while(0)

int	print_tokens = 1;

/*char	*tree_trim_string(char	*str)
{
	int	len;

	while(*str==' ' || *str=='\t' || *str=='\n')str++;
	len = strlen(str);
	if(len==0)return str;
	while(len>0 && (str[len-1]==' ' || str[len-1]=='\t' || str[len-1]=='\n'))len--;
	str[len] = 0;
	return str;
}*/

static void	skip_space(char	**S)
{
	while(isspace(**S))(*S)++;
}

static char	*read_symbol(char	**S)
{
	skip_space(S);
	char	*tok = *S;
	while(!isspace(**S) && **S!=')' && **S!='"' && **S!='(' && **S!='[' && **S!=']' && **S!=0)(*S)++;
	return tok;
}

static char	*read_string(char	**S)
{
	assert(**S=='"');
	char	*string = *S, *q = 1+string, *p = q;
	int	escape = 0;
	while(*q && !(*q=='"' && !escape))
	{
		if(escape)escape = 0;
		else if(*q=='\\')escape=1;
		if(!escape)*p++ = *q;
		q++;
	}
	if(*q++ != '"')return NULL;
	*p = 0;
	*S = q;
	return string;
}

static int read_number(char	**S)
{
	char	*q = *S;
	int	n = 0;
	int s = 1;
	if(*q=='-' || *q=='+')
	{
		if(*q=='-')n=-1;
		q++;
	}
	while(isdigit(*q))
	{
		n *= 10;
		n += *q - '0';
		q++;
	}
	*S=q;
	return n;
}

/*static inline char	*strdup_sub(char	*s, char	*e)
{
	char	save = *e;
	*e = 0;
	char	*returnme = strdup(s);
	*e = save;
	return returnme;
}*/

struct tree	*string_to_tree_leaf(char	**Str)
{
	// token node
	struct tree	*t = calloc(sizeof(struct tree), 1);
	t->cfrom = t->cto = -1;
	t->tfrom = t->tto = -1;
	assert(**Str=='"');
	char	*q = *Str;
	char	*string = read_string(&q);
	if(!string)FAIL("unterminated string", (*Str));
	t->label = strdup(string);
	while(*q && *q!=')')
	{
		// newer style with token structure string
		//printf("after passing token id, q = '%s'\n", q);
		skip_space(&q);
		int	tok_id = read_number(&q);
		skip_space(&q);
		if(*q == '"')
		{
			char	*token = read_string(&q);
			if(!token)FAIL("unterminated token string", q);
			char	*tok = strdup(token);
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
			skip_space(&q);
		}
	}
	*Str = q;
	return t;
}

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

/*
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
		return string_to_tree_leaf(&str);
	}
	else
	{
		// root node
		char	*dp = strchr(str, '(');
		return string_to_tree(tree_trim_string(dp));
	}
}
*/

static struct tree	*string_to_tree1(char	**S)
{
	char	*s = *S;
	skip_space(&s);
	if(*s++ != '(')FAIL("expected '('", s);
	skip_space(&s);

	struct tree	*returnme = NULL;

	if(isdigit(*s))
	{
		// rule or lexeme node
		int	edge_number = read_number(&s);	// edge ID
		skip_space(&s);
		if(!*s || *s==')')FAIL("expected type", s);
		char	*type = read_symbol(&s);	// type
		if(*s == '[')
		{
			s++;
			if(*s != '"')FAIL("expected '\"'", s);
			char	*gleform = read_string(&s);	// e.g. generic_n_le["dogma"]
			if(*s != ']')FAIL("expected ']'", s);
			s++;
		}
		if(*s)*s++ = 0;
		skip_space(&s);
		if(!*s || *s==')')FAIL("expected score", s);
		char	*scorestr = read_symbol(&s);	// score
		if(*s)*s++ = 0;
		double	score = atof(scorestr);
		skip_space(&s);
		if(!*s || *s==')')FAIL("expected from-vertex", s);
		int	tfrom = read_number(&s);		// from vertex
		skip_space(&s);
		if(!*s || *s==')')FAIL("expected to-vertex", s);
		int	tto = read_number(&s);			// to vertex
		skip_space(&s);

		struct tree	*t = calloc(sizeof(struct tree), 1);
		t->edge_id = edge_number;
		t->label = strdup(type);
		t->cfrom = t->cto = -1;
		t->tfrom = tfrom;
		t->tto = tto;
		t->score = score;

		while(*s == '(')
		{
			struct tree	*d = string_to_tree1(&s);
			if(!d)return NULL;
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
			skip_space(&s);
		}
		returnme = t;
	}
	else if(*s=='"')
	{
		// token node
		returnme = string_to_tree_leaf(&s);
	}
	else
	{
		// root name node
		char	*tok = read_symbol(&s);
		skip_space(&s);
		returnme = string_to_tree1(&s);
	}

	skip_space(&s);
	if(*s++ != ')')FAIL("expected ')'", s);
	*S = s;

	return returnme;
}

struct tree	*string_to_tree(char	*str)
{
	struct tree	*t = string_to_tree1(&str);
	if(!t)return NULL;
	skip_space(&str);
	if(*str)FAIL("trailing junk", str);
	return t;
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
