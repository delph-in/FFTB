#ifndef	TREEBANK_H
#define	TREEBANK_H

struct tb_edge
{
	int	id, from, to;
	char	*sign;
	int	npack, ndaughters;
	struct tb_edge	**pack, **daughter;

	long long	unpackings;	// how many ways to unpack this edge
	long long	solutions;	// how many ways to unpack a root using this edge
};

struct token
{
	wchar_t	*text;
	int		from, to;
	int		cfrom, cto;
};

struct parse
{
	int	nedges, nroots;
	struct tb_edge	**edges, **roots;
	int	ntokens;
	struct token	**tokens;
};

enum
{
	constraintIsAConstituent	=	1,
	constraintExactly			=	2,
	constraintPresent			=	3,
	constraintAbsent			=	4
};

struct constraint
{
	char	*sign;
	int		from, to;
	int		type;
};

struct session
{
	char	*profile_id, *item_id;
	char	*input;
	struct parse	*parse;
	int		id;
	struct tree	*pref_tree;
	int					npref_dec;
	struct constraint	*pref_dec;
};

#endif
