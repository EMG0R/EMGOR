// fx_registry.h -- list of effect types. Add new effects ONLY by adding a row in fx_registry.cpp.
#pragma once
#include "fx_common.h"
namespace hx {
struct FxTypeInfo {
    const char* id;        // short stable id, lower-case
    const char* name;      // <= 8 chars UI name
    const char* v1;        // v1 origin (one line)
    Fx* (*create)(Arena&, float sr);
};
int fxTypeCount();
const FxTypeInfo* fxType(int i);
int fxTypeById(const char* id);       // -1 if unknown
Fx* fxCreate(int typeIdx, Arena& a, float sr);
}
