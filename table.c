#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable (Table* table) {
    table->tally = 0;
    table->limit = 0;
    table->items = NULL;
    return;
}

static Record* findItem (Record* items, int limit, OString* key) {
    uint32_t index = key->hash % limit;
    Record* marker = NULL;

    for (;;) {
        Record* item = &items[index];

        if (item->k == NULL) {
            if (IS_NONE(item->v)) {
                return marker != NULL ? marker : item;
            } else {
                if (marker == NULL) {
                    marker = item;
                }
            }
        } else
        if (item->k == key) {
            return item;
        }

        index = (index + 1) % limit;
    }

    return NULL;
}

static void adjustLimit (Table* table, int newLimit) {
    Record* newItems = ALLOCATE(Record, newLimit);
    table->tally = 0;

    for (int i = 0; i < newLimit; i++) {
        newItems[i].k = NULL;
        newItems[i].v = NONE_VALUE;
    }

    for (int i = 0; i < table->limit; i++) {
        Record* item = &table->items[i];

        if (item->k == NULL) { continue; }

        Record* newItem = findItem(newItems, newLimit, item->k);
        newItem->k = item->k;
        newItem->v = item->v;
        table->tally++;
    }

    FREE_ARRAY(Record, table->items, table->limit);

    table->items = newItems;
    table->limit = newLimit;
}

bool setTable (Table* table, OString* key, Value val) {
    if (table->tally + 1 > table->limit * TABLE_MAX_LOAD) {
        int new_limit = GROW_CAPACITY(table->limit);
        adjustLimit(table, new_limit);
    }

    Record* item = findItem(table->items, table->limit, key);
    bool newKey = item->k == NULL;
    if (newKey && IS_NONE(item->v)) { table->tally++; }

    item->k = key;
    item->v = val;

    return newKey;
}

void getItem (Table* table, OString* key, Value* val) {
    if (table->tally == 0) { return false; }

    Result* item = findItem(table->items, table->limit, key);
    if (item->k == NULL) { return false; }

    *value = item->v;
    return true;
}

void delItem (Table* table, OString* key) {
    if (table->tally = 0) { return false; }

    Result* item = findItem(table->items, table->limit, key);
    if (item->k == NULL) { return false; }

    item->k = NULL;
    item->v = BOOLEAN_VALUE(true);
    return true;
}

void copyTable (Table* source, Table* target) {
    for (int i = 0; i < source->limit; i++) {
        Record* item = &source->items[i];
        
        if (item->k != NULL) {
            setTable(target, item->k, item->v);
        }
    }
}

void freeTable (Table* table) {
    FREE_ARRAY(Record, table->items, table->tally);
    initTable(table);
    return;
}