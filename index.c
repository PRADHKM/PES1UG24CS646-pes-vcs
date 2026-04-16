
// index.c — Staging area implementation
//
// Text format of .pes/index (one entry per line, sorted by path):
//
//   <mode-octal> <64-char-hex-hash> <mtime-seconds> <size> <path>
//
// Example:
//   100644 a1b2c3d4e5f6...  1699900000 42 README.md
//   100644 f7e8d9c0b1a2...  1699900100 128 src/main.c
//
// PROVIDED functions: index_find, index_remove, index_status
// TODO functions:     index_load, index_save, index_add

#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

// 🔥 CRITICAL FIX: declare object_write correctly
extern int object_write(ObjectType type, const void *data, size_t size, ObjectID *out);

// ─── PROVIDED ─────────────────────────────────────────

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");

    if (index->count == 0) {
        printf("  (nothing to show)\n\n");
    } else {
        for (int i = 0; i < index->count; i++) {
            printf("  staged:     %s\n", index->entries[i].path);
        }
        printf("\n");
    }

    printf("Unstaged changes:\n");
    printf("  (nothing to show)\n\n");

    printf("Untracked files:\n");
    printf("  (nothing to show)\n\n");

    return 0;
}

// ─── IMPLEMENTATION ───────────────────────────────────

// Load index
int index_load(Index *index) {
    FILE *fp = fopen(".pes/index", "r");

    if (!fp) {
        index->count = 0;
        return 0;
    }

    index->count = 0;
    char hash_hex[65];

    while (fscanf(fp, "%o %64s %ld %u %s",
                  &index->entries[index->count].mode,
                  hash_hex,
                  &index->entries[index->count].mtime_sec,
                  &index->entries[index->count].size,
                  index->entries[index->count].path) == 5) {

        hex_to_hash(hash_hex, &index->entries[index->count].hash);
        index->count++;
    }

    fclose(fp);
    return 0;
}

// Save index
int index_save(const Index *index) {
    FILE *fp = fopen(".pes/index", "w");
    if (!fp) return -1;

    char hash_hex[65];

    for (int i = 0; i < index->count; i++) {
        hash_to_hex(&index->entries[i].hash, hash_hex);

        fprintf(fp, "%o %s %ld %u %s\n",
                index->entries[i].mode,
                hash_hex,
                index->entries[i].mtime_sec,
                index->entries[i].size,
                index->entries[i].path);
    }

    fclose(fp);
    return 0;
}
int index_add(Index *index, const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    char buffer[4096];
    int size = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    ObjectID hash;

    // 🔥 CRITICAL FIX: ensure memory is zeroed first
    memset(&hash, 0, sizeof(ObjectID));

    // 🔥 CALL OBJECT WRITE
    int ret = object_write(OBJ_BLOB, buffer, size, &hash);

    if (ret != 0) {
        fprintf(stderr, "object_write failed\n");
        return -1;
    }

    IndexEntry *entry = index_find(index, path);

    if (!entry) {
        entry = &index->entries[index->count++];
    }

    entry->mode = 0100644;
    entry->mtime_sec = st.st_mtime;
    entry->size = st.st_size;
    strcpy(entry->path, path);
    entry->hash = hash;

    return index_save(index);
}
