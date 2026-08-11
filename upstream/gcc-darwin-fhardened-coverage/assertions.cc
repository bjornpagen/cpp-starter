// _GLIBCXX_ASSERTIONS constituent: expect an abort, not silent UB.
#include <vector>
int main(int argc, char **)
{
  std::vector<int> v(1);
  return v[argc + 1];   // out of bounds when run with no arguments
}
