// v3: export using of a TYPE and of a VARIABLE.
module;
struct bdb_config { int level; };
extern "C" int bdb_verbosity;
export module repro;
export using ::bdb_config;
export using ::bdb_verbosity;
