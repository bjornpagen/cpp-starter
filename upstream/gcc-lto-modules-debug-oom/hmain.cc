#include <string>

std::string greet(std::string const& name);

int main()
{
	return static_cast<int>(greet("x").size());
}
