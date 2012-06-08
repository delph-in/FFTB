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

#include	"treebank.h"

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
