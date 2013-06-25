#ifndef	TREEBANK_H
#define	TREEBANK_H

struct tb_edge
{
	int	id, from, to;
	char	*sign;					// uses lexical types on leaves
	char	*sign_with_lexnames;	// uses lexeme names on leaves
	int	npack, ndaughters;
	struct tb_edge	**pack, **daughter;

	struct tb_token	**tokens;
	int				ntokens;

	int	nparents;
	int	is_root;
	struct tb_edge	**parents;

	struct tb_edge	*host;	// edge this is packed onto

	long long	unpackings;	// how many ways to unpack this edge
	long long	solutions;	// how many ways to unpack a root using this edge
};

struct tb_token
{
	wchar_t	*text;
	char	*avmstr;
	int		from, to;
	int		cfrom, cto;
	int		id;
};

struct parse
{
	int	nedges, nroots;
	struct tb_edge	**edges, **roots;
	int	ntokens;
	struct tb_token	**tokens;
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
	int	inferred;
};

struct session
{
	char	*profile_id, *item_id;
	char	*input;
	char	*parse_id;
	struct parse	*parse;
	int		id;
	struct tree	*pref_tree;
	int					nlocal_dec;
	struct constraint	*local_dec;
	int					ngold_dec;
	struct constraint	*gold_dec;
	char				*gold_active;
	char	*comment;
};

struct parse	*do_unary_closure(struct parse	*Pin);
struct session	*get_session(int	id);
long long	count_remaining_trees(struct parse	*P, struct constraint	*c, int	nc);
struct session	*get_session(int	id);
void	free_parse(struct parse	*P);

struct tsdb	*cached_get_profile_and_pin(char	*path);
char	*get_pid_by_id(struct tsdb	*profile, char	*i_id);
char	*get_pref_rid(struct tsdb	*profile, char	*pid);
char	*get_result(struct tsdb	*profile, char	*pid, char	*rid);
int	get_decisions(struct tsdb	*profile, char	*pid, struct constraint	**Decs);
struct parse	*load_forest(struct tsdb	*profile, char	*pid);
struct tree	*extract_tree(struct tb_edge	*e, int	ucdepth);

int	get_t_active(char	*prof_id, char	*parse_id);
int	get_t_active_p(struct tsdb	*t, char	*parse_id);
char	*get_t_comment(char	*prof_id, char	*parse_id);
char	*get_t_comment_p(struct tsdb	*t, char	*parse_id);
char	*get_parse_id_p(struct tsdb	*t, char	*item_id);
char	*status_string(int	t_active);
char	*status_color(int	t_active);

int	iid_is_active(char	*iid);
char	*get_gold_profile_path(char	*profile_id);

extern char *tsdb_home_path;

extern char	*grammar_ace_image_path;
extern char *grammar_roots;

#endif
