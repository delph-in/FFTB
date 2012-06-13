#ifndef	TREEBANK_H
#define	TREEBANK_H

struct tb_edge
{
	int	id, from, to;
	char	*sign;					// uses lexical types on leaves
	char	*sign_with_lexnames;	// uses lexeme names on leaves
	int	npack, ndaughters;
	struct tb_edge	**pack, **daughter;

	int	nparents;
	int	is_root;
	struct tb_edge	**parents;

	struct tb_edge	*host;	// edge this is packed onto

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

struct parse	*do_unary_closure(struct parse	*Pin);
struct session	*get_session(int	id);
long long	count_remaining_trees(struct parse	*P, struct constraint	*c, int	nc);

extern char *tsdb_home_path;

#define	ERG_PATH	"/home/sweaglesw/cdev/ace/erg-1010.dat"

#endif
