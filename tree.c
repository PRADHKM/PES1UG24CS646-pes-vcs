// tree.c — Tree object serialization and construction

#include "tree.h"
#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Forward declaration (needed for linking)
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

// ─── Mode Constants ─────────────────────────────────────────

#define MODE_FILE 0100644
#define MODE_EXEC 0100755

// ─── PROVIDED FUNCTIONS ─────────────────────────────────────

// Get file mode
uint32_t get_file_mode(const char *path) {
struct stat st;
if (lstat(path, &st) != 0) return 0;

if (st.st_mode & S_IXUSR) return MODE_EXEC;
return MODE_FILE;

}

// Parse tree (kept minimal but correct)
int tree_parse(const void *data, size_t len, Tree *tree_out) {
tree_out->count = 0;
const uint8_t *ptr = data;
const uint8_t *end = ptr + len;

while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
    TreeEntry *e = &tree_out->entries[tree_out->count];

    const char *space = memchr(ptr, ' ', end - ptr);
    if (!space) return -1;

    char mode_str[16] = {0};
    memcpy(mode_str, ptr, space - (char *)ptr);
    e->mode = strtol(mode_str, NULL, 8);

    ptr = (uint8_t *)space + 1;

    const char *null = memchr(ptr, '\0', end - ptr);
    if (!null) return -1;

    size_t name_len = null - (char *)ptr;
    memcpy(e->name, ptr, name_len);
    e->name[name_len] = '\0';

    ptr = (uint8_t *)null + 1;

    memcpy(e->hash.hash, ptr, HASH_SIZE);
    ptr += HASH_SIZE;

    tree_out->count++;
}
return 0;

}

// Sort helper
static int cmp(const void *a, const void *b) {
return strcmp(((TreeEntry *)a)->name, ((TreeEntry *)b)->name);
}

// Serialize tree
int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
size_t max = tree->count * 300;
uint8_t *buf = malloc(max);
if (!buf) return -1;

Tree tmp = *tree;
qsort(tmp.entries, tmp.count, sizeof(TreeEntry), cmp);

size_t off = 0;

for (int i = 0; i < tmp.count; i++) {
    TreeEntry *e = &tmp.entries[i];

    int n = sprintf((char *)buf + off, "%o %s", e->mode, e->name);
    off += n + 1;

    memcpy(buf + off, e->hash.hash, HASH_SIZE);
    off += HASH_SIZE;
}

*data_out = buf;
*len_out = off;
return 0;

}

// ─── YOUR FUNCTION ─────────────────────────────────────────

// Simple version: flat tree (no directories)
int tree_from_index(ObjectID *id_out) {
Index idx;

if (index_load(&idx) != 0) {
    fprintf(stderr, "error: failed to load index\n");
    return -1;
}

Tree tree;
tree.count = 0;

for (int i = 0; i < idx.count; i++) {
    IndexEntry *ie = &idx.entries[i];
    TreeEntry *te = &tree.entries[tree.count];

    te->mode = MODE_FILE;
    snprintf(te->name, sizeof(te->name), "%s", ie->path);
    te->hash = ie->hash;

    tree.count++;
}

void *data;
size_t len;

if (tree_serialize(&tree, &data, &len) != 0) {
    fprintf(stderr, "error: tree serialize failed\n");
    return -1;
}

if (object_write(OBJ_TREE, data, len, id_out) != 0) {
    fprintf(stderr, "error: failed to write tree\n");
    free(data);
    return -1;
}

free(data);
return 0;

}
