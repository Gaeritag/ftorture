/* get_next_line project descriptor (subject v14.3).
   One suite per BUFFER_SIZE: the script builds the tester once per size
   with -D GNL_SUITE=<key> and runs it with -f <key>.                     */
#include "zft.h"
#include "project.h"
#include "gnl.h"

#define S(key, label, part) { #key, label, part, (void *)get_next_line }
#ifndef Z_SLOT
# define Z_SLOT 0
#endif
#ifndef Z_LABEL
# define Z_LABEL "BUFFER_SIZE=?"
#endif
#define C(k, part) { part == 1 ? "bsc" #k : "bbsc" #k, Z_SLOT == k ? Z_LABEL \
	: "BUFFER_SIZE=?", part, (void *)get_next_line }

static t_func	g_gnl_suites[] = {
	S(bs1, "BUFFER_SIZE=1", 1), S(bs2, "BUFFER_SIZE=2", 1),
	S(bs3, "BUFFER_SIZE=3", 1), S(bs5, "BUFFER_SIZE=5", 1),
	S(bs7, "BUFFER_SIZE=7", 1), S(bs10, "BUFFER_SIZE=10", 1),
	S(bs42, "BUFFER_SIZE=42", 1), S(bs100, "BUFFER_SIZE=100", 1),
	S(bs1024, "BUFFER_SIZE=1024", 1), S(bs9999, "BUFFER_SIZE=9999", 1),
	S(bs100000, "BUFFER_SIZE=100K", 1), S(bs10000000, "BUFFER_SIZE=10M", 1),
	S(bsdef, "sans -D (défaut)", 1), S(bs0, "BUFFER_SIZE=0", 1),
	S(bbs1, "BUFFER_SIZE=1", 2), S(bbs42, "BUFFER_SIZE=42", 2),
	S(bbs9999, "BUFFER_SIZE=9999", 2), S(bbs10000000, "BUFFER_SIZE=10M", 2),
	/* -f with other sizes: one slot per custom BUFFER_SIZE, labelled by the
	   build of that slot (-D Z_SLOT=k -D Z_LABEL="BUFFER_SIZE=n")         */
	C(1, 1), C(2, 1), C(3, 1), C(4, 1), C(5, 1), C(6, 1), C(7, 1), C(8, 1),
	C(1, 2), C(2, 2), C(3, 2), C(4, 2), C(5, 2), C(6, 2), C(7, 2), C(8, 2),
};

t_project	g_proj = {
	"get_next_line", "get_next_line.c", "get_next_line.h",
	"GET_NEXT_LINE · SUJET v14.3 · LINUX",
	"> ta gnl va parler",
	g_gnl_suites,
	(int)(sizeof(g_gnl_suites) / sizeof(g_gnl_suites[0])),
	2,
	{"", "Obligatoire · un BUFFER_SIZE par ligne",
		"Bonus · plusieurs fd à la fois", ""},
	{"", "obligatoire", "bonus", ""},
};
