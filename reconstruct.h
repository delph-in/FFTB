#include	"tree.h"

#include	<ace/hash.h>
#include	<ace/dag.h>
#include	<ace/rule.h>
#include	<ace/lexicon.h>
#include	<ace/mrs.h>

struct dg	*reconstruct_tree(struct tree	*t, void	(*callback)(struct tree	*t, struct dg	*d));
void	free_mrs_stuff(struct mrs	*m);
struct lexeme	*get_lex_by_name_hash(char	*name);
