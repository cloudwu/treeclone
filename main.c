#include <stdio.h>
#include <stdlib.h>
#include "treeclone.h"

static void *
new_ud(int id) {
	int * ud = malloc(sizeof(int));
	*ud = id;
	return ud;
}

static int
get_ud(int *ud) {
	if (ud == NULL)
		return -1;
	return *ud;
}

struct targs {
	int level;
	int n;
};

static void 
print_ud (void *bp, void *patch, void *argument, void *stackframe) {
	struct targs * arg = argument;
	if (stackframe) {
		// pass argument to next level
		struct targs * pass = stackframe;
		pass->n = 0;
		pass->level = arg->level+1;
	}
	printf("LEVEL %d [%d], bp = %d, patch = %d\n", arg->level , arg->n, get_ud(bp), get_ud(patch));
	++arg->n;
}

int
main() {
	struct tree_manager *m = tree_newmanager(free);
	int id = tree_newbp(m, new_ud(1), 4);
	int i;
	for (i=0;i<5;i++) {
		int child = tree_newbp(m, new_ud(i+2), 0);
		tree_spawn(m, id, child);
	}
	tree_build(m, id);

	int t = tree_print(m, id);

	struct targs arg;
	arg.level = 0;
	arg.n = 0;
	struct tree_node root;
	tree_root(m, t, &root);
	tree_setpatch(&root, new_ud(100));

	struct tree_node child, child2;


	int newtree = tree_newbp(m, new_ud(101), 0);
	tree_build(m, newtree);

	int subtree = tree_print(m, newtree);

	tree_fetch(&root, 3, &child);
	tree_fetch(&root, 3, &child2);
	// child and child2 are both reference root[3]

	tree_mount(&child, subtree);

	tree_setpatch(&child2, new_ud(102));


	tree_fetch(&root, 1, &child2);
	tree_setpatch(&child2, new_ud(103));

	tree_traverse(&root, print_ud, &arg, sizeof(arg));

	tree_debuginfo(m);
	tree_closemanager(m);
	return 0;
}
