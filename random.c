#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>
#include	<signal.h>
#include	<sys/types.h>
#include	<dirent.h>
#include	<wchar.h>

#include	<tsdb.h>

#include	<ace/tree.h>
#include	<ace/reconstruct.h>

#include	"treebank.h"

random_decision(struct parse	*P, struct constraint	**Decs, int	*Ndecs, long long	ntrees)
{
	int	nd = 0;
	struct disc	*d = NULL;
	get_discriminants(P, -1, -1, ntrees, &nd, &d);
	printf("%lld trees left ; %d discriminants\n", ntrees, nd);
	if(!nd)return 0;
	// for UNIFORM FOREST:
	// plan: for some discriminant, add positive constraint with probability #(disc)/#trees; otherwise add negative constraint.
	// can only do that for the +/- discriminants, not = ones.  problem: can't always fully disambiguate with +/- discriminants.
	// is there a way to pick an = discriminant fairly?
	// for UNIFORM DISCRIMINANT:
	int	x = ((double)rand()  / RAND_MAX) * nd;
	if(x==nd)x--;
	if(x<0)x=0;
	(*Ndecs)++;
	(*Decs) = realloc(*Decs, sizeof(struct constraint)*(*Ndecs));
	struct constraint	*c = (*Decs)+(-1+*Ndecs);
	struct disc *dx = d+x;
	c->sign = dx->sign;
	c->from = dx->from;
	c->to = dx->to;
	c->type = constraintExactly;
	c->inferred = 0;
	printf("decided: %s [%d-%d]\n", c->sign, c->from, c->to);
	if(d)free(d);
	return 1;
}

int	randomize_item(struct tsdb	*prof, char	*iid, char	*pid)
{
	struct parse	*P = load_forest(prof, pid);

	if(!P)
	{
		printf("%s	unable to get parse forest\n", iid);
		return -1;
	}

	struct parse	*newP = do_unary_closure(P);
	P = newP;
	if(!P)
	{
		printf("%s	unable to perform unary closure\n", iid);
		return -1;
	}

	printf("%s	loaded parse forest with %d edges (%d roots)\n", iid, P->nedges, P->nroots);
	if(!P->nroots)return -1;

	int	retry = 1;
	long long remaining;
	int	ndecs = 0;
	struct constraint	*decs = NULL;
	again:
	ndecs = 0;
	if(retry++>10)
	{
		printf("%s	too many retries; giving up\n", iid);
		return -1;
	}
	while(1)
	{
		remaining = count_remaining_trees(P, decs, ndecs);
		printf("%lld remaining\n", remaining);
		if(remaining==1)break;
		if(!random_decision(P, &decs, &ndecs, remaining))break;
	}
	if(remaining<0)
	{
		printf("%s	somehow eliminated all readings\n", iid);
		return -1;
	}
	int i;
	for(i=0;i<P->nedges;i++)
		if(P->edges[i]->unpackings >= 1 && P->edges[i]->solutions >= 1 && P->edges[i]->is_root)break;
	assert(i < P->nedges);
	struct tree	*t = extract_tree(P->edges[i], 0);
	if(!t)goto again;
	clear_slab();
	void callback(struct tree	*t, struct dg	*d) { }
	struct dg	*d = reconstruct_tree(t, callback);
	clear_slab();
	if(!d)
	{
		printf("latent unification failure; trying again\n");
		goto again;	// failed to reconstruct
	}
	int result = save_decisions(prof->path, pid, ndecs, decs);
	if(!result)result = write_tree(prof->path, pid, "1", "1", "randomizer", "", NULL, NULL);
	if(!result)result = save_tree_for_item(prof->path, pid, t);
	printf("%s	%s\n", iid, result?"fail":"success");
	if(P)free_parse(P);
	if(!result)return 0;
	else return -1;
}

void	random_annotate(char	*fullpath, char	*mode)
{
	srand(time(0));
	int	is_profile = 0;
	int filter(const struct dirent	*d)
	{
		if(d->d_name[0]=='.')return 0;
		if(!strcmp(d->d_name, "relations"))is_profile = 1;
		return 1;
	}
	struct dirent	**names;
	int i,n = scandir(fullpath, &names, (int(*)(const struct dirent*))filter, alphasort);
	if(n < 0) { printf("500 unable to read TSDB home directory\n"); return; }
	for(i=0;i<n;i++)
		free(names[i]);
	free(names);

	if(!is_profile) { printf("500 unable to annotate a directory; specify a profile.\n"); return; }

	struct tsdb	*profile = cached_get_profile_and_pin(fullpath);
	if(!profile) { printf("500 unable to read TSDB profile\n"); return; }

	struct relation	*items = get_relation(profile, "item");
	int	item_input = get_field(items, "i-input", "string");
	int	item_id = get_field(items, "i-id", "integer");
	int	rcount = 0, ecount = 0;

	for(i=0;i<items->ntuples;i++)
	{
		char	*iid = items->tuples[i][item_id];
		if(!iid_is_active(iid))continue;
		char	*input = items->tuples[i][item_input];
		char	*parse_id = get_pid_by_id(profile, iid);
		if(!parse_id)
		{
			printf("%s	no parse found\n", iid);
		}
		else
		{
			int result = randomize_item(profile, iid, parse_id);
			if(result == 0)rcount++;
			else ecount++;
		}
	}
	printf("randomized %d items; failed on %d items\n", rcount, ecount);

	unpin_all();
}
