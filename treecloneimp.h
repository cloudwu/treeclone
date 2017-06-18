#ifndef tree_clone_imp_h
#define tree_clone_imp_h

/*
tree_blueprint
----------
| int   int           tree_blueprint *[n]  void *
| n     node count    children             userdata

tree_patch
----------
| int   int   void *       tree *[n]
| order n     patchdata    children

tree
----
tree_blueprint *   tree_patch *    int
blurprint          patch           patch version
*/

struct tree;
struct tree_blueprint;

struct tree_manager {
	int cap_tree;
	int cap_blueprint;
	int bp_slot;
	int tree_slot;
	int color;	// for gc
	tree_userdata_free free;
	struct tree ** tree_pool;
	struct tree_blueprint ** bp_pool;
};

struct tree_blueprint {
	void *userdata;
	int color;	// for gc
	int children_cap;
	int children_n;
	int total;
	int child[1];
};

struct tree_patch {
	void * userdata;
	int order;
	int n;
	int child[1];
};

struct tree {
	struct tree_manager *mgr;
	struct tree_patch *patch;
	int blueprint;
	int patch_version;
	int patch_n;
};

static inline void *
advance_ptr(void *ptr, ssize_t offset) {
	return (char *)ptr + offset;
}

static inline struct tree_patch *
next_patch(struct tree_patch *p) {
	size_t sz = sizeof(struct tree_patch) + (p->n - 1) * sizeof(p->child[0]);
	return advance_ptr(p, sz);
}

#endif
