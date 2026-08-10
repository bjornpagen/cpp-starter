// v2: export { using ::f; } (export block form).
module;
extern "C" int bdb_open(int fd);
export module repro;
export {
}
