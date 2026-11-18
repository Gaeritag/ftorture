/* ************************************************************************** */
/*  PART 3 — linked lists (mandatory in subject v19.3).                       */
/*  Every node and content is a guarded malloc block: use-after-free and      */
/*  double free are caught, forgotten frees are reported as leaks.            */
/* ************************************************************************** */
#include "zft.h"
#include "libft_decl.h"
#include "refs.h"

static int	g_del;
static int	g_iter;
static int	g_seen[64];

static void	del_cnt(void *p)
{
	g_del++;
	free(p);
}

static void	del_noop(void *p)
{
	(void)p;
	g_del++;
}

static int	*new_int(int v)
{
	int	*p = malloc(sizeof(int));

	*p = v;
	return (p);
}

static t_list	*mk(void *content)
{
	t_list	*n = malloc(sizeof(t_list));

	n->content = content;
	n->next = NULL;
	return (n);
}

/* list of n nodes, contents are malloc'ed ints 0..n-1 */
static t_list	*mklist(int n)
{
	t_list	*head = NULL;

	while (n-- > 0)
	{
		t_list	*x = mk(new_int(n));

		x->next = head;
		head = x;
	}
	return (head);
}

static void	free_list(t_list *l, int contents)
{
	while (l)
	{
		t_list	*n = l->next;

		if (contents)
			free(l->content);
		free(l);
		l = n;
	}
}

static void	out_list(t_list *l)
{
	int	n = 0;

	while (l && n++ < 50)
	{
		OUT("%d", l->content ? *(int *)l->content : -1);
		l = l->next;
	}
	OUT("end");
}

/* ========================================================== lstnew ===== */
TEST(ft_lstnew, "content conservé, next = NULL")
{
	int		x = 42;
	t_list	*n = ft_lstnew(&x);

	CHECK(n != NULL, "a renvoyé NULL");
	EQ_PTR(n->content, &x);
	EQ_PTR(n->next, NULL);
	CHECK(alloc_size(n) != (size_t)-1 && alloc_size(n) >= sizeof(t_list),
		"le nœud doit être alloué avec malloc(sizeof(t_list))");
	free(n);
}
TEST(ft_lstnew, "content NULL")
{
	t_list	*n = ft_lstnew(NULL);

	CHECK(n != NULL, "a renvoyé NULL");
	EQ_PTR(n->content, NULL);
	EQ_PTR(n->next, NULL);
	free(n);
}
TEST(ft_lstnew, "ne copie pas le contenu")
{
	char	*s = malloc(4);
	t_list	*n;

	strcpy(s, "abc");
	n = ft_lstnew(s);
	CHECK(n != NULL, "a renvoyé NULL");
	EQ_PTR(n->content, s);
	free(s);
	free(n);
}
MTEST(ft_lstnew, "malloc échoue -> NULL")
{
	t_list	*n;
	int		x;

	ARM();
	n = ft_lstnew(&x);
	DISARM();
	if (FAILING)
		CHECK(n == NULL, "doit renvoyer NULL si malloc échoue");
	free(n);
}

/* ===================================================== lstadd_front ===== */
TEST(ft_lstadd_front, "liste vide")
{
	t_list	*l = NULL;
	t_list	*n = mk(new_int(1));

	ft_lstadd_front(&l, n);
	EQ_PTR(l, n);
	EQ_PTR(n->next, NULL);
	free_list(l, 1);
}
TEST(ft_lstadd_front, "ajout en tête")
{
	t_list	*l = mklist(3);
	t_list	*old = l;
	t_list	*n = mk(new_int(-1));

	ft_lstadd_front(&l, n);
	EQ_PTR(l, n);
	EQ_PTR(n->next, old);
	EQ_INT(*(int *)l->next->next->next->content, 2);
	free_list(l, 1);
}
TEST(ft_lstadd_front, "ajouts successifs")
{
	t_list	*l = NULL;

	for (int i = 0; i < 5; i++)
		ft_lstadd_front(&l, mk(new_int(i)));
	int	i = 4;

	for (t_list *c = l; c; c = c->next)
		EQ_INT(*(int *)c->content, i--);
	EQ_INT(i, -1);
	free_list(l, 1);
}
VS(ft_lstadd_front, "lst NULL")
{
	t_list	*n = mk(NULL);

	PICK(ft_lstadd_front, r_lstadd_front)(NULL, n);
	OUT("ok");
	free(n);
}
VS(ft_lstadd_front, "new NULL")
{
	t_list	*l = mklist(2);

	PICK(ft_lstadd_front, r_lstadd_front)(&l, NULL);
	out_list(l);
	free_list(l, 1);
}

/* ========================================================== lstsize ==== */
TEST(ft_lstsize, "NULL -> 0")
{
	EQ_INT(ft_lstsize(NULL), 0);
}
TEST(ft_lstsize, "1, 2, 5, 1000 nœuds")
{
	int	n[] = {1, 2, 5, 1000};

	for (int i = 0; i < 4; i++)
	{
		t_list	*l = mklist(n[i]);

		INPUT("liste de %d nœud(s)", n[i]);
		EQ_INT(ft_lstsize(l), n[i]);
		free_list(l, 1);
	}
}
TEST(ft_lstsize, "ne modifie pas la liste")
{
	t_list	*l = mklist(3);
	t_list	*a = l, *b = l->next, *c = l->next->next;

	ft_lstsize(l);
	EQ_PTR(l, a);
	EQ_PTR(a->next, b);
	EQ_PTR(b->next, c);
	EQ_PTR(c->next, NULL);
	free_list(l, 1);
}

/* ========================================================== lstlast ==== */
TEST(ft_lstlast, "NULL -> NULL")
{
	EQ_PTR(ft_lstlast(NULL), NULL);
}
TEST(ft_lstlast, "1 nœud")
{
	t_list	*l = mklist(1);

	EQ_PTR(ft_lstlast(l), l);
	free_list(l, 1);
}
TEST(ft_lstlast, "plusieurs nœuds")
{
	t_list	*l = mklist(6);
	t_list	*last = l;

	while (last->next)
		last = last->next;
	EQ_PTR(ft_lstlast(l), last);
	EQ_PTR(ft_lstlast(l->next->next), last);
	free_list(l, 1);
}

/* ====================================================== lstadd_back ===== */
TEST(ft_lstadd_back, "liste vide")
{
	t_list	*l = NULL;
	t_list	*n = mk(new_int(7));

	ft_lstadd_back(&l, n);
	EQ_PTR(l, n);
	free_list(l, 1);
}
TEST(ft_lstadd_back, "ajout en fin")
{
	t_list	*l = mklist(3);
	t_list	*n = mk(new_int(3));

	ft_lstadd_back(&l, n);
	EQ_PTR(l->next->next->next, n);
	EQ_PTR(n->next, NULL);
	free_list(l, 1);
}
TEST(ft_lstadd_back, "ajouts successifs depuis vide")
{
	t_list	*l = NULL;
	int		i = 0;

	for (int k = 0; k < 5; k++)
		ft_lstadd_back(&l, mk(new_int(k)));
	for (t_list *c = l; c; c = c->next)
		EQ_INT(*(int *)c->content, i++);
	EQ_INT(i, 5);
	free_list(l, 1);
}
VS(ft_lstadd_back, "lst NULL")
{
	t_list	*n = mk(NULL);

	PICK(ft_lstadd_back, r_lstadd_back)(NULL, n);
	OUT("ok");
	free(n);
}
VS(ft_lstadd_back, "new NULL")
{
	t_list	*l = mklist(2);

	PICK(ft_lstadd_back, r_lstadd_back)(&l, NULL);
	out_list(l);
	free_list(l, 1);
}

/* ======================================================= lstdelone ===== */
TEST(ft_lstdelone, "del(content) une fois, nœud libéré")
{
	t_list	*n = mk(new_int(5));

	g_del = 0;
	ft_lstdelone(n, del_cnt);
	EQ_INT(g_del, 1);
}
TEST(ft_lstdelone, "ne touche pas au nœud suivant")
{
	t_list	*l = mklist(2);
	t_list	*second = l->next;

	g_del = 0;
	ft_lstdelone(l, del_cnt);
	EQ_INT(g_del, 1);
	EQ_INT(*(int *)second->content, 1);
	EQ_PTR(second->next, NULL);
	free_list(second, 1);
}
TEST(ft_lstdelone, "ne free pas content lui-même (c'est le rôle de del)")
{
	static int	x = 3;
	t_list		*n = mk(&x);

	g_del = 0;
	ft_lstdelone(n, del_noop);
	EQ_INT(g_del, 1);
}
VS(ft_lstdelone, "lst NULL")
{
	PICK(ft_lstdelone, r_lstdelone)(NULL, del_cnt);
	OUT("ok");
}
/* free only what the function under test left alive (protected versions
   may return early without freeing anything) */
static void	free_if_live(void *p)
{
	if (p && alloc_size(p) != (size_t)-1)
		free(p);
}

VS(ft_lstdelone, "del NULL")
{
	t_list	*n = mk(new_int(1));
	void	*c = n->content;

	PICK(ft_lstdelone, r_lstdelone)(n, NULL);
	free_if_live(c);
	free_if_live(n);
	OUT("ok");
}

/* ======================================================== lstclear ===== */
TEST(ft_lstclear, "libère tout, *lst = NULL")
{
	t_list	*l = mklist(5);

	g_del = 0;
	ft_lstclear(&l, del_cnt);
	EQ_PTR(l, NULL);
	EQ_INT(g_del, 5);
}
TEST(ft_lstclear, "liste vide")
{
	t_list	*l = NULL;

	g_del = 0;
	ft_lstclear(&l, del_cnt);
	EQ_PTR(l, NULL);
	EQ_INT(g_del, 0);
}
TEST(ft_lstclear, "à partir du milieu (&a->next)")
{
	t_list	*l = mklist(4);

	g_del = 0;
	ft_lstclear(&l->next, del_cnt);
	EQ_INT(g_del, 3);
	EQ_PTR(l->next, NULL);
	EQ_INT(*(int *)l->content, 0);
	free_list(l, 1);
}
TEST(ft_lstclear, "longue liste (10 000)")
{
	t_list	*l = mklist(10000);

	g_del = 0;
	ft_lstclear(&l, del_cnt);
	EQ_PTR(l, NULL);
	EQ_INT(g_del, 10000);
}
VS(ft_lstclear, "lst NULL")
{
	PICK(ft_lstclear, r_lstclear)(NULL, del_cnt);
	OUT("ok");
}
VS(ft_lstclear, "del NULL")
{
	t_list	*l = mklist(1);
	t_list	*node = l;
	void	*c = l->content;

	PICK(ft_lstclear, r_lstclear)(&l, NULL);
	OUT("%s", l ? "lst non NULL" : "lst NULL");
	free_if_live(c);
	free_if_live(node);
}

/* ========================================================= lstiter ===== */
static void	f_rec(void *p)
{
	if (g_iter < 64)
		g_seen[g_iter] = *(int *)p;
	g_iter++;
}

static void	f_double(void *p)
{
	*(int *)p *= 2;
}

TEST(ft_lstiter, "f appelée sur chaque content, dans l'ordre")
{
	t_list	*l = mklist(6);

	g_iter = 0;
	ft_lstiter(l, f_rec);
	EQ_INT(g_iter, 6);
	for (int i = 0; i < 6; i++)
		EQ_INT(g_seen[i], i);
	free_list(l, 1);
}
TEST(ft_lstiter, "f peut modifier le contenu")
{
	t_list	*l = mklist(3);

	ft_lstiter(l, f_double);
	EQ_INT(*(int *)l->next->next->content, 4);
	free_list(l, 1);
}
TEST(ft_lstiter, "liste vide")
{
	g_iter = 0;
	ft_lstiter(NULL, f_rec);
	EQ_INT(g_iter, 0);
}
VS(ft_lstiter, "f NULL")
{
	t_list	*l = mklist(2);

	PICK(ft_lstiter, r_lstiter)(l, NULL);
	OUT("ok");
	free_list(l, 1);
}

/* ========================================================== lstmap ===== */
static void	*f_dup10(void *p)
{
	int	*r = malloc(sizeof(int));

	if (r)
		*r = *(int *)p + 10;
	return (r);
}

static int	g_static_vals[16];

static void	*f_static(void *p)
{
	return (&g_static_vals[*(int *)p]);
}

TEST(ft_lstmap, "applique f, nouvelle liste")
{
	t_list	*l = mklist(4);
	t_list	*m;
	int		i = 0;

	g_del = 0;
	m = ft_lstmap(l, f_dup10, del_cnt);
	CHECK(m != NULL, "a renvoyé NULL");
	for (t_list *c = m, *o = l; c || o; c = c->next, o = o->next, i++)
	{
		CHECK(c && o, "la nouvelle liste n'a pas la même taille (%d)", i);
		CHECK(c != o, "le nœud %d n'est pas un nouveau nœud", i);
		EQ_INT(*(int *)c->content, i + 10);
		EQ_INT(*(int *)o->content, i);
	}
	EQ_INT(g_del, 0);
	free_list(m, 1);
	free_list(l, 1);
}
TEST(ft_lstmap, "liste vide -> NULL")
{
	EQ_PTR(ft_lstmap(NULL, f_dup10, del_cnt), NULL);
}
TEST(ft_lstmap, "nœuds alloués avec malloc")
{
	t_list	*l = mklist(2);
	t_list	*m = ft_lstmap(l, f_dup10, del_cnt);

	CHECK(m != NULL, "a renvoyé NULL");
	CHECK(alloc_size(m) != (size_t)-1 && alloc_size(m) >= sizeof(t_list),
		"les nœuds doivent être alloués avec malloc");
	free_list(m, 1);
	free_list(l, 1);
}
MTEST(ft_lstmap, "malloc d'un nœud échoue -> NULL, sans fuite")
{
	t_list	*l;
	t_list	*m;

	DISARM();
	l = mklist(5);
	for (int i = 0; i < 16; i++)
		g_static_vals[i] = i;
	ARM();
	m = ft_lstmap(l, f_static, del_noop);
	DISARM();
	if (FAILING)
		CHECK(m == NULL, "doit renvoyer NULL si une allocation échoue");
	free_list(m, 0);
	free_list(l, 1);
}
MTEST(ft_lstmap, "malloc de f ou d'un nœud échoue : pas de fuite ni crash")
{
	t_list	*l = mklist(4);
	t_list	*m;

	ARM();
	m = ft_lstmap(l, f_dup10, del_cnt);
	DISARM();
	free_list(m, 1);
	free_list(l, 1);
}
VS(ft_lstmap, "f NULL")
{
	t_list	*l = mklist(2);
	t_list	*m = PICK(ft_lstmap, r_lstmap)(l, NULL, del_cnt);

	OUT("%s", m ? "non-NULL" : "NULL");
	free_list(m, 0);
	free_list(l, 1);
}
VS_SPEC(ft_lstmap, "del NULL (succès)")
{
	t_list	*l = mklist(3);
	t_list	*m = PICK(ft_lstmap, r_lstmap)(l, f_dup10, NULL);

	out_list(m);
	free_list(m, 1);
	free_list(l, 1);
}
