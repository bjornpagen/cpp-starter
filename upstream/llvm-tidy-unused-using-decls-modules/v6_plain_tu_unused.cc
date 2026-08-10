// v6: baseline, non-module TU, unused using-declaration.
namespace bdb { extern "C" int bdb_open(int fd); }
using bdb::bdb_open;
