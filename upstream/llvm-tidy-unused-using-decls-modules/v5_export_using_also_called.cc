// v5: export using ::f; where f is also called inside the module.
module;
extern "C" int bdb_open(int fd);
export module repro;
export using ::bdb_open;
export int open_default() { return bdb_open(0); }
