#include <stdio.h>

#include "treeclone.h"
#include "treecloneimp.h"

static void
debuginfo_dumpbp(int id, struct tree_blueprint *bp) {
	printf("BLUEPRINT [%d] %p\n", id, bp->userdata);
	int i;
	for (i=0;i<bp->children_n;i++) {
		printf("\t%d: %d\n", i, bp->child[i]);
	}
}

static void
debuginfo_dumppatch(struct tree_patch *p) {
	printf("\t%d: %p", p->order, p->userdata);
	int i;
	for (i=0;i<p->n;i++) {
		int id = p->child[i];
		if (id == 0) {
			printf(" 0");
		} else if (id > 0) {
			printf(" B%d", id);
		} else {
			printf(" T%d", -id);
		}
	}
	printf("\n");
}

static void
debuginfo_dumptree(int id, struct tree *t) {
	printf("TREE [%d] BP[%d]\n", id, t->blueprint);
	struct tree_patch *p = t->patch;
	int i;
	for (i=0;i<t->patch_n;i++) {
		debuginfo_dumppatch(p);
		p = next_patch(p);
	}
}

void
tree_debuginfo(struct tree_manager *M) {
	int i;
	for (i=0;i<M->cap_blueprint;i++) {
		struct tree_blueprint *bp = M->bp_pool[i];
		if (bp) {
			debuginfo_dumpbp(i+1, bp);
		}
	}
	for (i=0;i<M->cap_tree;i++) {
		struct tree *t = M->tree_pool[i];
		if (t) {
			debuginfo_dumptree(i+1, t);
		}
	}
}
