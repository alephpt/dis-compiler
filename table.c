#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

    return marker;
}

static void adjustLimit (Table* table, int newLimit) {
    Record* newItems = ALLOCATE(Record, newLimit);

    for (int i = 0; i < newLimit; i++) {
        newItems[i].k = NULL;
        newItems[i].v = NONE_VALUE;
    }

    table->tally = 0;
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
        int new_limit = EXPAND_LIMITS(table->limit);
        adjustLimit(table, new_limit);
    }

    Record* item = findItem(table->items, table->limit, key);
    bool newKey = item->k == NULL;
    if (newKey && IS_NONE(item->v)) { table->tally++; }

    item->k = key;
    item->v = val;

    return newKey;
}

bool getItem (Table* table, OString* key, Value* val) {
    if (table->tally == 0) { return false; }

    Record* item = findItem(table->items, table->limit, key);
    if (item->k == NULL) { return false; }

    *val = item->v;
    return true;
}

bool delItem (Table* table, OString* key) {
    if (table->tally == 0) { return false; }

    Record* item = findItem(table->items, table->limit, key);
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
    return;
}

OString* findString(Table* table, const char* chars, int len, uint32_t hash) {
    if (table->tally == 0) { return NULL; }

    uint32_t index = hash % table->limit;

    for (;;) {
        Record* item = &table->items[index];

        if (item->k == NULL) {
            if (IS_NONE(item->v)) { return NULL; }
        } else if (item->k->length == len && 
                   item->k->hash == hash &&
                   memcmp(item->k->chars, chars, len) == 0) {
            return item->k;
        }

        index = (index + 1) % table->limit;
    }
}

void freeTable (Table* table) {
    FREE_ARRAY(Record, table->items, table->tally);
    initTable(table);
    return;
}