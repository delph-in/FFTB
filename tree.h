#ifndef	TREE_H
#define	TREE_H

struct tree
{
	char	*label;
	char	**tokens;
	int		ndaughters;
	int		ntokens;
	struct tree	**daughters;

	struct tree			*lexhead;
	//struct decoration	*decoration;

	int	cfrom, cto;
	int	tfrom, tto;
	struct tree	*parent;
	int		edge_id;
	double	score;
};

struct tree	*string_to_tree(char	*str);
struct tree	*find_subtree_with_crange(struct tree	*t, int	from, int	to);
void	free_tree(struct tree	*t);

#endif
