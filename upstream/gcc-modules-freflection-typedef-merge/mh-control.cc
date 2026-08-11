module;
#include <vector>
#include <string>
#include <map>
#include <format>
#include <algorithm>
export module mh;
export struct Widget { int x; };
export auto reflected() -> std::string {
	std::vector<int> v{3, 1, 2};
	std::sort(v.begin(), v.end());
	std::map<std::string, int> m{{"widget", v.front()}};
	return std::format("{}:{}", m.begin()->first, v.size());
}
