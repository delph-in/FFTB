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

// system to count how many trees the forest describes that match our constraints

int	label_is_present(char	*label, char	*uchain)
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
			return label_is_present(c.sign, uc_label)?0:1;	// the label is there or not
		if(c.type == constraintAbsent)
			return label_is_present(c.sign, uc_label)?1:0;	// the label is there or not
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

// system to check whether a stored gold tree matches our constraints

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
