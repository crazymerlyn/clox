#include "memory.h"
#include "table.h"

#define TABLE_MAX_LOAD 0.75

void init_table(Table *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void free_table(Table *table) {
    FREE_ARRAY(table->entries, Entry, table->capacity);
    init_table(table);
}

static Entry *find_entry(Entry *entries, int capacity, ObjString *key) {
    uint32_t index = key->hash % capacity;
    Entry *tombstone = NULL;
    for (;;) {
        Entry *entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

static void adjust_capacity(Table *table, int capacity) {
    Entry *entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; ++i) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; ++i) {
        Entry *entry = &table->entries[i];
        if (entry->key == NULL) continue;
        Entry *dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(table->entries, Entry, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}

bool table_get(Table *table, ObjString *key, Value *value) {
    if (table->entries == NULL) return false;

    Entry *entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

bool table_set(Table *table, ObjString *key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }
    Entry *entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    entry->key = key;
    entry->value = value;

    if (is_new_key) table->count++;
    return is_new_key;
}

bool table_delete(Table *table, ObjString *key) {
    if (table->count == 0) return false;

    Entry *entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

void table_add_all(Table *from, Table *to) {
    for (int i = 0; i < from->capacity; ++i) {
        if (from->entries[i].key == NULL) continue;
        table_set(to, from->entries[i].key, from->entries[i].value);
    }
}
