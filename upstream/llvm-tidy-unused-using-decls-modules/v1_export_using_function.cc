// v1: export using ::f; (function) in a module interface unit.
module;
extern "C" int bdb_open(int fd);
export module repro;
export using ::bdb_open;
