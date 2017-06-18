#ifndef tree_clone_h
#define tree_clone_h

struct tree_node {
	struct tree_manager *mgr;
	struct tree_patch *patch;	// patch ref cache
	int tree;
	int blueprint;
	int order;
	int patch_version;
};

typedef void (*tree_userdata_free)(void *);

struct tree_manager * tree_newmanager(tree_userdata_free f);
void tree_closemanager(struct tree_manager *);

int tree_newbp(struct tree_manager *m, void *userdata, int children_cap);	// return blueprint id, children_cap is optional
int tree_spawn(struct tree_manager *m, int parent, int child);	// return child id
int tree_build(struct tree_manager *m, int bp);	// return count

int tree_print(struct tree_manager *m, int bp);	// return tree id
void tree_root(struct tree_manager *m, int tree, struct tree_node *output);	// fetch root
struct tree_node * tree_fetch(struct tree_node *parent, int n, struct tree_node *output);
int tree_children(struct tree_node *parent);
void tree_setpatch(struct tree_node * node, void * userdata);
void* tree_getpatch(struct tree_node * node);
void* tree_read(struct tree_node * node);	// read userdata of blueprint
int tree_mount(struct tree_node *node, int tree);	// return last tree

typedef void (*tree_traverse_func)(void *bp, void *patch, void *argument, void *stackframe);
void tree_traverse(struct tree_node * node, tree_traverse_func f, void *argument, int framesize);

void tree_collect(struct tree_manager *m, int n, int *id);
void tree_debuginfo(struct tree_manager *m);

#endif
