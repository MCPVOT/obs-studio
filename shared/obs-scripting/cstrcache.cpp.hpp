#include "shared/obs-scripting/build/obs-scripting-cstrcache.pch.hpp"

using namespace std;

struct const_string_table {
	unordered_map<string, string> strings;
};

static struct const_string_table table;

const char *cstrcache_get(const char *str)
{
	if (!str || !*str) {
		return "";
	}

	auto &strings = table.strings;
	auto pair = strings.find(str);

	if (pair == strings.end()) {
		strings[str] = str;
		pair = strings.find(str);
	}

	return pair->second.c_str();
}
