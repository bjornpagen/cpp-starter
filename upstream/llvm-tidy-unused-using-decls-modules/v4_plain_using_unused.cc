// v4: plain (non-exported) using ::f; in module purview, genuinely unused.
// Control: this SHOULD stay a warning after any fix.
module;
extern "C" int bdb_open(int fd);
export module repro;
using ::bdb_open;
