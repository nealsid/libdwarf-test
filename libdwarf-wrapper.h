#pragma once

#include <string>

struct Dwarf_Debug_s;
struct Dwarf_Error_s;
struct Dwarf_Die_s;

typedef Dwarf_Debug_s* Dwarf_Debug;
typedef Dwarf_Error_s* Dwarf_Error;
typedef Dwarf_Die_s* Dwarf_Die;

using namespace std;

bool readDwarfFile(const char* file_path,
                   string& output_path,
                   Dwarf_Debug* dbg,
                   Dwarf_Error* dw_err);

decltype(DW_AT_array) dwarfTag(Dwarf_Die dw_die,
                               
