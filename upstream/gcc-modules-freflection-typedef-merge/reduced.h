typedef struct { int c; } my_state_t;
template <typename C> struct my_traits { using state_type = my_state_t; };
template <typename S> struct my_pos { S st; long off; };
template <typename C> struct my_str { using pos = my_pos<typename my_traits<C>::state_type>; C ch; pos p; };
using mystring = my_str<char>;
inline mystring make() { return {}; }
