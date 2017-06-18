#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "treeclone.h"
#include "treecloneimp.h"

#define DEFAULT_CAP 256
#define DEFAULT_CHILDREN 4

struct tree_manager *
tree_newmanager(tree_userdata_free f) {
	struct tree_manager *m = malloc(sizeof(*m));
	m->cap_tree = DEFAULT_CAP;
	m->cap_blueprint = DEFAULT_CAP;
	m->bp_slot = 0;
	m->tree_slot = 0;
	m->color = 0;
	m->free = f;
	// todo: check OOM
	m->tree_pool = calloc(m->cap_tree, sizeof(struct tree *));
	m->bp_pool = calloc(m->cap_blueprint, sizeof(struct blueprint *));

	return m;
}

static void
free_patch(struct tree *t) {
	tree_userdata_free f = t->mgr->free;
	struct tree_patch * p = t->patch;
	int i;
	for (i=0;i<t->patch_n;i++) {
		f(p->userdata);
		p = next_patch(p);
	}
	free(t->patch);
}

void
tree_closemanager(struct tree_manager *M) {
	int i;
	for (i=0;i<M->cap_blueprint;i++) {
		struct tree_blueprint * bp = M->bp_pool[i];
		if (bp) {
			M->free(bp->userdata);
			free(bp);
		}
	}
	free(M->bp_pool);
	for (i=0;i<M->cap_tree;i++) {
		struct tree *t = M->tree_pool[i];
		if (t) {
			free_patch(t);
			free(t);
		}
	}
	free(M->tree_pool);
	free(M);
}

static int
find_bp_slot(struct tree_manager *M) {
	int i;
	for (i=M->bp_slot;i<M->cap_blueprint;i++) {
		if (M->bp_pool[i] == NULL) {
			M->bp_slot = i+1;
			return i;
		}
	}
	int cap = M->cap_blueprint;
	int newcap = cap * 2;
	M->bp_pool = realloc(M->bp_pool, newcap * sizeof(struct tree_blueprint *));
	memset(M->bp_pool + M->cap_blueprint, 0, (newcap - cap) * sizeof(struct tree_blueprint *));
	M->cap_blueprint = newcap;
	M->bp_slot = cap + 1;
	return cap;
}

static int
find_tree_slot(struct tree_manager *M) {
	int i;
	for (i=M->tree_slot;i<M->cap_tree;i++) {
		if (M->tree_pool[i] == NULL) {
			M->tree_slot = i+1;
			return i;
		}
	}
	int cap = M->cap_tree;
	int newcap = cap * 2;
	M->tree_pool = realloc(M->tree_pool, newcap * sizeof(struct tree *));
	memset(M->tree_pool + M->cap_tree, 0, (newcap - cap) * sizeof(struct tree *));
	M->cap_tree = newcap;
	M->tree_slot = cap + 1;
	return cap;
}

int
tree_newbp(struct tree_manager *M, void *userdata, int children_cap) {
	int id = find_bp_slot(M);
	struct tree_blueprint *bp = calloc(1, sizeof(*bp) + (children_cap - 1) * sizeof(bp->child[0]));
	bp->color = M->color;
	bp->userdata = userdata;
	bp->children_cap = children_cap;
	M->bp_pool[id] = bp;
	return id + 1;	// blueprint_id is positive
}

int
tree_spawn(struct tree_manager *M, int parent, int child) {
	assert(parent > 0 && child >= 0);	// child & parent should be blueprint id
	assert(parent <= M->cap_blueprint);
	struct tree_blueprint *bp = M->bp_pool[parent-1];
	assert(bp != NULL);
	if (bp->children_n >= bp->children_cap) {
		if (bp->children_n == 0) {
			bp->children_cap = DEFAULT_CHILDREN;
		} else {
			bp->children_cap *= 2;
		}
		bp = realloc(bp, sizeof(*bp) + (bp->children_cap - 1) * sizeof(bp->child[0]));
		M->bp_pool[parent-1] = bp;
	}
	int id = bp->children_n++;
	bp->child[id] = child;
	return id;
}

static int
count_bp(struct tree_manager *M, struct tree_blueprint *bp) {
	int c = 1;
	int i;
	for (i=0;i<bp->children_n;i++) {
		int id = bp->child[i];
		if (id) {
			c += count_bp(M, M->bp_pool[id-1]);
		}
	}
	bp->total = c;
	return c;
}

static inline struct tree_blueprint *
get_bp(struct tree_manager *M, int bp_id) {
	assert(bp_id > 0 && bp_id <= M->cap_blueprint);
	struct tree_blueprint *bp = M->bp_pool[bp_id-1];
	assert(bp != NULL);
	return bp;
}

static inline struct tree *
get_tree(struct tree_manager *M, int tree_id) {
	int id = -tree_id-1;
	assert(id >= 0 && id < M->cap_tree);
	struct tree *t = M->tree_pool[id];
	assert(t != NULL);
	return t;
}

int
tree_build(struct tree_manager *M, int bp_id) {
	struct tree_blueprint *bp = get_bp(M, bp_id);
	return count_bp(M, bp);
}

int
tree_print(struct tree_manager *M, int bp_id) {
	get_bp(M, bp_id);	// check bp_id
	int id = find_tree_slot(M);
	struct tree *t = malloc(sizeof(*t));
	t->mgr = M;
	t->patch = NULL;
	t->blueprint = bp_id;
	t->patch_version = 0;
	t->patch_n = 0;
	M->tree_pool[id] = t;
	return -id-1;	// tree id is negative
}

static struct tree_patch *
find_patch(struct tree * t, int order) {
	int i;
	struct tree_patch * p = t->patch;
	for (i=0;i<t->patch_n;i++) {
		if (p->order == order) {
			return p;
		} else if (p->order > order) {
			return NULL;
		}
		p = next_patch(p);
	}
	return NULL;
}

static int
find_parent(struct tree_manager *M, int root_bp, int root_order, int order, struct tree_node *node) {
	struct tree_blueprint *bp = get_bp(M, root_bp);
	int i;
	int order_from = root_order + 1;
	for (i=0;i<bp->children_n;i++) {
		if (order_from == order) {
			struct tree *t = get_tree(M, node->tree);
			node->blueprint = root_bp;
			node->order = root_order;
			node->patch = find_patch(t, root_order);
			node->patch_version = t->patch_version;
			return i;
		}
		int bp_id = bp->child[i];
		if (bp_id) {
			struct tree_blueprint * c = M->bp_pool[bp_id-1];
			int order_to = c->total + order_from;
			if (order < order_to) {
				return find_parent(M, bp_id, order_from, order, node);
			}
			order_from = order_to;
		}
	}
	return -1;
}

void
tree_root(struct tree_manager *M, int tree, struct tree_node *output) {
	struct tree *t = get_tree(M, tree);
	output->mgr = M;
	output->tree = tree;
	output->blueprint = t->blueprint;
	output->order = 0;	// root is the first one;
	output->patch_version = t->patch_version;
	if (t->patch && t->patch->order == 0) {
		output->patch = t->patch;
	} else {
		output->patch = NULL;
	}
}

static inline struct tree_patch *
sync_patch(struct tree_node *node) {
	struct tree_manager *M = node->mgr;
	struct tree *t = get_tree(M, node->tree);
	if (t->patch_version != node->patch_version) {
		node->patch_version = t->patch_version;
		if (node->order != 0) {
			struct tree_node parent;
			parent.tree = node->tree;
			int n = find_parent(M, t->blueprint, 0, node->order, &parent);
			if (parent.patch && parent.patch->n > 0) {
				int c = parent.patch->child[n];
				if (c < 0) {
					tree_root(M, c, node);
					return node->patch;
				}
			}
		}
		node->patch = find_patch(t, node->order);
	}
	return node->patch;
}

struct tree_node *
tree_fetch(struct tree_node *parent, int n, struct tree_node *output) {
	assert(n >= 0);
	struct tree_manager *M = parent->mgr;
	struct tree_blueprint *bp = get_bp(M, parent->blueprint);
	if (n >= bp->children_n)
		return NULL;
	struct tree_patch * p = sync_patch(parent);
	if (p && p->n > 0) {
		assert(p->n == bp->children_n);
		// fetch child in patch
		int id = p->child[n];
		if (id < 0) {
			// mount tree
			tree_root(M, id, output);
			return output;
		}
	}
	// fetch child in blueprint
	output->mgr = M;
	output->tree = parent->tree;
	output->blueprint = bp->child[n];
	int order = parent->order + 1;
	int i;
	for (i=0;i<n;i++) {
		int bp_id = bp->child[i];
		if (bp_id) {
			order += get_bp(M,bp_id)->total;
		}
	}
	struct tree *t = get_tree(M, parent->tree);
	output->order = order;
	output->patch = find_patch(t, order);
	output->patch_version = t->patch_version;

	return output;
}

int
tree_children(struct tree_node *parent) {
	struct tree_manager *M = parent->mgr;
	struct tree_blueprint *bp = get_bp(M, parent->blueprint);
	return bp->children_n;
}

static void *
expand_memory(void **beginptr, void *endptr, void * expand_point, size_t expand_size) {
	void * begin = *beginptr;
	size_t size = (char *)endptr - (char *)begin;
	size_t offset = (char *)expand_point - (char *)begin;
	begin = realloc(begin, size + expand_size);
	memmove(begin + offset + expand_size, begin + offset, size - offset);
	*beginptr = begin;
	return begin + offset;
}

static struct tree_patch *
new_patch_data(struct tree *t, int order, void *userdata) {
	struct tree_patch *p = t->patch;
	size_t add_sz = sizeof(struct tree_patch) - sizeof(p->child);
	int i;
	for (i=0;i<t->patch_n;i++) {
		assert(p->order != order);
		if (p->order > order)
			break;
		p = next_patch(p);
	}
	struct tree_patch *ep = p;
	int j;
	for (j=i;j<t->patch_n;j++) {
		ep = next_patch(ep);
	}
	p = expand_memory((void **)&t->patch, ep, p, add_sz);
	// insert patch by order
	p->userdata = userdata;
	p->order = order;
	p->n = 0;
	++t->patch_n;
	++t->patch_version;
	return p;
}

void
tree_setpatch(struct tree_node * node, void * userdata) {
	struct tree_manager *M = node->mgr;
	struct tree_patch * p = sync_patch(node);
	if (p == NULL) {
		struct tree *t = get_tree(M, node->tree);
		node->patch = new_patch_data(t, node->order, userdata);
		node->patch_version = t->patch_version;
	} else {
		if (p->userdata != userdata) {
			M->free(p->userdata);
			p->userdata = userdata;
		}
	}
}

void*
tree_getpatch(struct tree_node * node) {
	struct tree_patch * p = sync_patch(node);
	if (p == NULL)
		return NULL;
	return p->userdata;
}

void*
tree_read(struct tree_node * node) {
	struct tree_blueprint * bp = get_bp(node->mgr, node->blueprint);
	return bp->userdata;
}

static struct tree_patch *
new_patch_tree(struct tree *t, struct tree_node *node) {
	struct tree_patch *p = t->patch;
	struct tree_blueprint *bp = get_bp(node->mgr, node->blueprint);
	size_t add_sz;
	if (node->patch) {
		// expand patch
		add_sz = bp->children_n * sizeof(p->child[0]);
	} else {
		add_sz = sizeof(struct tree_patch) + (bp->children_n - 1) * sizeof(p->child[0]);
	}

	int i;
	for (i=0;i<t->patch_n;i++) {
		if (p->order >= node->order)
			break;
		p = next_patch(p);
	}
	struct tree_patch *ep = p;
	int j;
	for (j=i;j<t->patch_n;j++) {
		ep = next_patch(ep);
	}

	if (node->patch) {
		assert(p->order == node->order);
		p = expand_memory((void **)&t->patch, ep, p, add_sz);
		p->n = bp->children_n;
	} else {
		intptr_t offset = (char *)p - (char *)t->patch;
		expand_memory((void **)&t->patch, ep, p->child, add_sz);
		p = advance_ptr(t->patch, offset);
		p->userdata = NULL;
		p->order = node->order;
		p->n = bp->children_n;
		++t->patch_n;
	}
	memcpy(p->child, bp->child, p->n * sizeof(p->child[0]));
	++t->patch_version;

	return p;
}

int
tree_mount(struct tree_node *node, int tree) {
	assert(node->order != 0);	// root can't mount
	struct tree_manager *M = node->mgr;
	get_tree(M, tree);	// check tree
	struct tree * t = get_tree(M, node->tree);
	struct tree_node parent;
	parent.mgr = M;
	parent.tree = node->tree;
	int child_id = find_parent(M, t->blueprint, 0, node->order, &parent);
	if (parent.patch == NULL || parent.patch->n == 0) {
		parent.patch = new_patch_tree(t, &parent);
	}
	int ret = parent.patch->child[child_id];
	if (ret < 0) {
		ret = 0;
	}
	parent.patch->child[child_id] = tree;
	tree_root(M, tree, node);
	return ret;
}

//// traverse tree

struct traverse_arg {
	struct tree_manager *mgr;
	struct tree_patch *patch;
	tree_traverse_func f;
	void * stackframe;
	int framesize;
	int order;
	int patch_count;
	int patch_n;
};

static void traverse(struct traverse_arg *A, struct tree_blueprint *bp);

static void
traverse_children(struct traverse_arg *A, struct tree_blueprint *bp, void *stackframe) {
	int i;
	struct tree_blueprint ** tbp = A->mgr->bp_pool;
	void * savestack = A->stackframe;
	A->stackframe = stackframe;
	for (i=0;i<bp->children_n;i++) {
		int bp_id = bp->child[i];
		if (bp_id) {
			struct tree_blueprint *c = tbp[bp_id-1];
			traverse(A, c);
		}
	}
	A->stackframe = savestack;
}

static void
traverse_patch_children(struct traverse_arg *A, struct tree_blueprint *bp, struct tree_patch *p, void * stackframe) {
	// traverse children in patch
	void *saveframe = A->stackframe;
	A->stackframe = stackframe;
	int i;
	struct tree_manager *M = A->mgr;
	for (i=0;i<p->n;i++) {
		int child_id = p->child[i];
		if (child_id > 0) {
			// it's blueprint
			traverse(A, M->bp_pool[child_id-1]);
		} else {
			int cbp_id = bp->child[i];
			if (cbp_id) {
				struct tree_blueprint *cbp = M->bp_pool[cbp_id-1];
				A->order += cbp->total;
			}
			if (child_id < 0) {
				// it's a sub tree
				struct tree *t = get_tree(M, child_id);
				struct traverse_arg subtree;
				subtree.mgr = M;
				subtree.patch = t->patch;
				subtree.f = A->f;
				subtree.order = 0;
				subtree.framesize = A->framesize;
				subtree.stackframe = stackframe;
				subtree.patch_count = 0;
				subtree.patch_n = t->patch_n;
				traverse(&subtree, get_bp(M, t->blueprint));
			}
		}
	}
	A->stackframe = saveframe;
}

static void
traverse(struct traverse_arg *A, struct tree_blueprint *bp) {
	struct tree_patch *p = A->patch;
	if (p && A->order == p->order) {
		// need patch
		++A->order;
		++A->patch_count;
		if (A->patch_count >= A->patch_n) {
			A->patch = NULL;
		} else {
			A->patch = next_patch(p);
		}
		if (bp->children_n == 0) {
			A->f(bp->userdata, p->userdata, A->stackframe, NULL);
		} else {
			char stackframe[A->framesize];
			A->f(bp->userdata, p->userdata, A->stackframe, stackframe);
			if (p->n == 0) {
				traverse_children(A, bp, stackframe);
			} else {
				traverse_patch_children(A, bp, p, stackframe);
			}
		}
	} else {
		++A->order;
		if (bp->children_n != 0) {
			char stackframe[A->framesize];
			A->f(bp->userdata, NULL, A->stackframe, stackframe);
			traverse_children(A, bp, stackframe);
		} else {
			A->f(bp->userdata, NULL, A->stackframe, NULL);
		}
	}
}

void
tree_traverse(struct tree_node * node, tree_traverse_func f, void *argument, int framesize) {
	struct tree_manager *M = node->mgr;
	int order_from = node->order;
	struct tree * t = get_tree(M, node->tree);
	struct tree_patch * patch_begin = t->patch;
	int i;
	for (i=0;i<t->patch_n;i++) {
		if (patch_begin->order < order_from) {
			patch_begin = next_patch(patch_begin);
		} else {
			break;
		}
	}
	struct traverse_arg args;
	args.mgr = M;
	args.patch = patch_begin;
	args.f = f;
	args.stackframe = argument;
	args.framesize = framesize;
	args.order = order_from;
	args.patch_count = i;
	args.patch_n = t->patch_n;
	if (args.patch_count >= args.patch_n) {
		args.patch = NULL;
	}
	traverse(&args, get_bp(M, node->blueprint));
}

