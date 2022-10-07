
#include "libdwarf-wrapper.h"

#include "libdwarf.h"
#include <string>

bool readDwarfFile(const char* file_path,
                   std::string& output_path,
                   Dwarf_Debug* dw_dbg,
                   Dwarf_Error* dw_err) {

  return DW_DLV_OK == dwarf_init_path(file_path,
                                      output_path.data(),
                                      output_path.capacity(),
                                      DW_GROUPNUMBER_ANY,
                                      nullptr,
                                      nullptr,
                                      dw_dbg,
                                      dw_err);
}
