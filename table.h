#ifndef dis_table_h
#define dis_table_h

#include "header.h"
#include "value.h"

typedef struct {
    OString* k;
    Value v;
} Record;

typedef struct {
    int tally;
    int limit;
    Record items;
} Table;

void initTable(Table* table);
bool setTable(Table* table, OString* key, Value val);
bool getItem(Table* table, OString* key, Value* val);
bool delItem(Table* table, OString* key);
void copyTable(Table* source, Table* target);
void freeTable(Table* table);

#endif