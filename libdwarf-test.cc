
#include "libdwarf.h"
#include "dwarf.h"
#include "dwarf_error.h"

#include "libdwarf-wrapper.h"

#include <iostream>
#include <optional>
#include <stack>
#include <vector>

using namespace std;

using TypeNameFilter = optional<char*>;
// There is no particular reason to choose DW_TAG_array_type for the
// decltype expression, it was just the first one and the assumption
// is that all DW_TAG will be of the same type.
using DwarfTagType = decltype<DW_TAG_array_type>;
using DwarfTagFilter = vector<DwarfTagType>;

const char* unknown_struct_name_literal = "unknown_struct";

// A vector of DW_TAG values we care about.
DwarfTagFilter tag_filter =
    {
        DW_TAG_class_type,
        DW_TAG_structure_type
    };


std::optional<const char*> getTypeNameForMemberDie(Dwarf_Debug dw_dbg, Dwarf_Die dw_die) {
    Dwarf_Attribute dw_attr;
    Dwarf_Error dw_err;
    Dwarf_Off dw_offset;
    Dwarf_Die type_die;
    char *dbg_typename = nullptr;

    // Severe abuse of short circuiting booleans (later functions
    // have dependencies on earlier functions)
    if (!dwarf_attr(dw_die, DW_AT_type, &dw_attr, &dw_err) &&
        !dwarf_global_formref(dw_attr, &dw_offset, &dw_err) &&
        !dwarf_offdie_b(dw_dbg, dw_offset, true, &type_die, &dw_err) &&
        !dwarf_diename(type_die, (char **)&dbg_typename, &dw_err)) {
        return dbg_typename;
    }
    return std::nullopt;
}

struct DieProcessingFunction {
    // This stack indicates whether we are inside a struct.  The
    // values indicates the level in the DIE tree, and is for testing
    // when the struct ends.
    stack<int, vector<int>> structs_defined_at_level;

    bool operator()(Dwarf_Debug dw_dbg, Dwarf_Die dw_die, int level) {
        Dwarf_Half dw_tag;
        Dwarf_Error dw_err;

        // level gets incremented when processing children of a struct
        // DIE, so if level is equal to the top, that means we are no
        // longer inside that struct, and we can close the structure
        // definition.
        if (structs_defined_at_level.size() > 0 && level == structs_defined_at_level.top()) {
            for (int i = 0; i < level; ++i) {
                cout << "\t";
            }
            cout << "};" << endl;
            structs_defined_at_level.pop();
        }

        auto dwarfRet = dwarf_tag(dw_die, &dw_tag, &dw_err);
        if (dwarfRet != DW_DLV_OK) {
            for (int i = 0; i < level; ++i) {
                cout << "\t";
            }
            cout << "could not read DW_TAG" << endl;
            return true;
        }

        const char* diename;
        dwarfRet = dwarf_diename(dw_die,
                                 (char **)&diename,
                                 &dw_err);

        if (dwarfRet != DW_DLV_OK) {
            diename = unknown_struct_name_literal;
            // cout << "diename returned " << dwarfRet << endl;
            // cout << "error: " << dw_err << endl;
        }

        if (dw_tag == DW_TAG_structure_type) {
            // make sure we're at a defining declaration by checking
            // for DW_AT_declaration, which is for non-defining decls.
            Dwarf_Attribute dw_attr;
            dwarfRet = dwarf_attr(dw_die, DW_AT_declaration, &dw_attr, &dw_err);
            if (dwarfRet == DW_DLV_OK) {
                return false;
            }

            structs_defined_at_level.push(level);
            for (int i = 0; i < level; ++i) {
                cout << "\t";
            }

            cout << "struct " << diename << " { ";

            dwarfRet = dwarf_attr(dw_die, DW_AT_decl_file, &dw_attr, &dw_err);
            if (dwarfRet != DW_DLV_OK) {
                cout << "// no file index information";
            } else {
                cout << " // file " << dw_attr;
            }
            cout << endl;

            return true;

        } else if (dw_tag == DW_TAG_member) {
            cout << dw_die << endl;
            for (int i = 0; i < level; ++i) {
                cout << "\t";
            }
            cout << getTypeNameForMemberDie(dw_dbg, dw_die).value_or("unknown") << " ";
            cout << diename << ";" << endl;
        }
        //        cout << endl;
        return true;
    }
};

template<typename Fn>
int VisitDieTree(Dwarf_Debug dw_dbg,
                 Dwarf_Die dw_die,
                 Fn DieFunction,
                 TypeNameFilter typeNameFilter,
                 DwarfTagFilter tag_filter,
                 int level);

// Precondition: dw_dbg should have just had dwarf_next_cu_header_d called.
template<typename Fn>
int VisitDIEsOfCU(Dwarf_Debug dw_dbg,
                  Fn DieFunction,
                  TypeNameFilter typeNameFilter,
                  DwarfTagFilter tag_filter) {

    Dwarf_Die dw_die;
    Dwarf_Error dw_err;
    // Get the compilation unit DIE (null second parameter).
    auto dwarf_ret = dwarf_siblingof_b(dw_dbg,
                                       nullptr,
                                       true,
                                       &dw_die,
                                       &dw_err);

    // We want a pre-order, depth first search traversal.
    return VisitDieTree(dw_dbg,
                        dw_die,
                        DieFunction,
                        typeNameFilter,
                        tag_filter,
                        0);
}

template<typename Fn>
int VisitDieTree(Dwarf_Debug dw_dbg,
                 Dwarf_Die dw_die,
                 Fn DieFunction,
                 TypeNameFilter typeNameFilter,
                 DwarfTagFilter tag_filter,
                 int level) {

    Dwarf_Die child_die;
    Dwarf_Error dw_err;
    int dw_ret;
    int nodes_processed = 0;
    Dwarf_Attribute dw_attr;

    do {
        
        if (typeNameFilter && !dwarf_attr(dw_die, DW_AT_type, &dw_attr, &dw_err)) {
            auto dieTypeName = getTypeNameForMemberDie(dw_dbg, dw_die);
            cout << "comparing " << (dieTypeName ? dieTypeName.value() : "no value") << " & " << typeNameFilter.value() << endl;
            if (!dieTypeName || strcmp(dieTypeName.value(), typeNameFilter.value())) {
                dw_ret = dwarf_siblingof_b(dw_dbg, dw_die, true, &dw_die, &dw_err);
                if (dw_ret == DW_DLV_NO_ENTRY) {
                    break;
                }
                continue;
            }
        }
        // Process root node of our tree.
        if (!DieFunction(dw_dbg, dw_die, level)) {

            // Returned false, so do not continue processing subtree.
            dw_ret = dwarf_siblingof_b(dw_dbg, dw_die, true, &dw_die, &dw_err);
            continue;
        }

        // Get the first child for processing, and process siblings while they exist.
        dw_ret = dwarf_child(dw_die, &child_die, &dw_err);

        if (dw_ret == DW_DLV_NO_ENTRY) {
            return 1;
        }

        do {

            VisitDieTree(dw_dbg, child_die, DieFunction, typeNameFilter, level + 1);

            dw_ret = dwarf_siblingof_b(dw_dbg, child_die, true, &child_die, &dw_err);
        } while(dw_ret != DW_DLV_NO_ENTRY);

        dw_ret = dwarf_siblingof_b(dw_dbg, dw_die, true, &dw_die, &dw_err);

    } while (dw_ret != DW_DLV_NO_ENTRY);

    return nodes_processed;
}

int main(int argc, char* argv[]) {
    string output_path;
    Dwarf_Debug dw_dbg;
    Dwarf_Error dw_err = 0x0;
    TypeNameFilter typeNameFilter = nullopt;

    if (argc == 3) {
        typeNameFilter = argv[2];
        cout << "Filtering by type " << typeNameFilter.value() << endl;
    }

    output_path.reserve(1024);
    if (!readDwarfFile(argv[1],
                       output_path,
                       &dw_dbg,
                       &dw_err)) {
        cout << "Error opening DWARF file " << argv[1] << endl;
        return 1;
    } else {
        cout << "DWARF file opened as: " output_path << endl;
    }

    Dwarf_Unsigned cu_header_length;
    Dwarf_Half     version;
    Dwarf_Off      abbrev_offset;
    Dwarf_Half     dw_address_size;
    Dwarf_Half     dw_length_size;
    Dwarf_Half     dw_extension_size;
    Dwarf_Sig8     dw_type_signature;
    Dwarf_Unsigned dw_typeoffset;
    Dwarf_Unsigned dw_next_cu_header_offset;
    Dwarf_Half     dw_header_cu_type;

    int dwarfRead = dwarf_next_cu_header_d(dw_dbg,
                                           true,
                                           &cu_header_length,
                                           &version,
                                           &abbrev_offset,
                                           &dw_address_size,
                                           &dw_length_size,
                                           &dw_extension_size,
                                           &dw_type_signature,
                                           &dw_typeoffset,
                                           &dw_next_cu_header_offset,
                                           &dw_header_cu_type,
                                           &dw_err);

    while (dwarfRead != DW_DLV_NO_ENTRY) {
        cout << dwarfRead << endl;
        cout << dw_err << endl;
        cout << "Dwarf version: " << version << endl;

        VisitDIEsOfCU(dw_dbg,
                      DieProcessingFunction(),
                      typeNameFilter);

        dwarfRead = dwarf_next_cu_header_d(dw_dbg,
                                           true,
                                           &cu_header_length,
                                           &version,
                                           &abbrev_offset,
                                           &dw_address_size,
                                           &dw_length_size,
                                           &dw_extension_size,
                                           &dw_type_signature,
                                           &dw_typeoffset,
                                           &dw_next_cu_header_offset,
                                           &dw_header_cu_type,
                                           &dw_err);

    }
}

    // dwarfRead = dwarf_cu_header_basics(dw_die,
    //                                    &version,
    //                                    &is_info,
    //                                    &is_dwo,
    //                                    &offset_size,
    //                                    &address_size,
    //                                    &extension_size,
    //                                    &signature,
    //                                    &offset_of_length,
    //                                    &total_byte_length,
    //                                    &dw_err);
    // cout << dwarfRead << endl;
    // cout << dw_err << endl;
    // cout << version << endl;
    // cout << is_info << endl;
// Dwarf_Die dw_die;
// dwarfRead = dwarf_siblingof_b(dw_dbg,
//                               nullptr,
//                               true,
//                               &dw_die,
//                               &dw_err);
// cout << dwarfRead << endl;
// cout << dw_err << endl;

// Dwarf_Bool      is_info;
// Dwarf_Bool      is_dwo;
// Dwarf_Half      offset_size;
// Dwarf_Half      address_size;
// Dwarf_Half      extension_size;
// Dwarf_Sig8*     signature;
// Dwarf_Off       offset_of_length;
// Dwarf_Unsigned  total_byte_length;
            // char at_name[100];
            // dwarfRet = dwarf_diename(dw_die, (const char**)at_name, &dw_err);
            // if (dwarfRet == DW_DLV_OK) {
            //   for (int i = 0; i < level; ++i) {
            //     cout << "\t";
            //   }
            //   cout << "dw_AT_name: " << at_name << endl;
            // }

            // cout << dwarfRead << endl;
            // for (int i = 0; i < level; ++i) {
            //   cout << "\t";
            // }
            // cout << dw_err << endl;
 // else {
        //     char* tag_name;
        //     dwarf_get_TAG_name(dw_tag, (const char**)&tag_name);
        //     for (int i = 0; i < level; ++i) {
        //         cout << "\t";
        //     }
        //     //            cout << "DW_TAG: " << dw_tag << ": " << tag_name << endl;
        // }

        // auto dwarfRet = dwarf_attr(dw_die, DW_AT_type, &dw_attr, &dw_err);

        // if (dwarfRet != DW_DLV_OK) {
        //     return nullptr;
        // }

        // dwarfRet = dwarf_global_formref(dw_attr, &dw_offset, &dw_err);

        // if (dwarfRet != DW_DLV_OK) {
        //     return nullptr;
        // }

        // dwarfRet = dwarf_offdie_b(dw_dbg, dw_offset, true, &type_die, &dw_err);

        // if (dwarfRet != DW_DLV_OK) {
        //     return nullptr;
        // }

        //     dwarfRet = dwarf_diename(type_die,
        //                          (char **)&dbg_typename,
        //                              &dw_err);

        // return dbg_typename;
