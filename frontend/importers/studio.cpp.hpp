/******************************************************************************
    Copyright (C) 2019-2020 by Dillon Pentz <dillon@vodbox.io>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "frontend/build/frontend-importers.pch.hpp"

using namespace std;
using namespace json11;

void TranslateOSStudio(Json &res)
{
	Json::object out = res.object_items();
	Json::array sources = out["sources"].array_items();
	for (Json &source : sources) {
		TranslatePlatformImportedSource(source);
	}
	out["sources"] = sources;
	res = out;
}

static string CheckPath(const string &path, const string &rootDir)
{
	char root[512];
	*root = 0;
	size_t rootLen = os_get_abs_path(rootDir.c_str(), root, sizeof(root));

	char absPath[512];
	*absPath = 0;
	size_t len = os_get_abs_path((rootDir + path).c_str(), absPath, sizeof(absPath));

	if (len == 0) {
		return path;
	}

	if (strstr(absPath, root) != absPath) {
		return path;
	}

	if (*(absPath + rootLen) != QDir::separator().toLatin1()) {
		return path;
	}

	return absPath;
}

void TranslatePaths(Json &res, const string &rootDir)
{
	if (res.is_object()) {
		Json::object out = res.object_items();

		for (auto it = out.begin(); it != out.end(); it++) {
			Json val = it->second;

			if (val.is_string()) {
				if (val.string_value().rfind("./", 0) != 0) {
					continue;
				}

				out[it->first] = CheckPath(val.string_value(), rootDir);
			} else if (val.is_array() || val.is_object()) {
				TranslatePaths(val, rootDir);
				out[it->first] = val;
			}
		}

		res = out;
	} else if (res.is_array()) {
		Json::array out = res.array_items();

		for (size_t i = 0; i != out.size(); i++) {
			Json val = out[i];

			if (val.is_string()) {
				if (val.string_value().rfind("./", 0) != 0) {
					continue;
				}

				out[i] = CheckPath(val.string_value(), rootDir);
			} else if (val.is_array() || val.is_object()) {
				TranslatePaths(val, rootDir);
				out[i] = val;
			}
		}

		res = out;
	}
}

bool StudioImporter::Check(const string &path)
{
	BPtr<char> file_data = os_quick_read_utf8_file(path.c_str());
	string err;
	Json collection = Json::parse(file_data, err);

	if (err != "") {
		return false;
	}

	if (collection.is_null()) {
		return false;
	}

	if (collection["sources"].is_null()) {
		return false;
	}

	if (collection["name"].is_null()) {
		return false;
	}

	if (collection["current_scene"].is_null()) {
		return false;
	}

	return true;
}

string StudioImporter::Name(const string &path)
{
	BPtr<char> file_data = os_quick_read_utf8_file(path.c_str());
	string err;

	Json d = Json::parse(file_data, err);

	string name = d["name"].string_value();

	return name;
}

int StudioImporter::ImportScenes(const string &path, string &name, Json &res)
{
	if (!os_file_exists(path.c_str())) {
		return IMPORTER_FILE_NOT_FOUND;
	}

	if (!Check(path.c_str())) {
		return IMPORTER_FILE_NOT_RECOGNISED;
	}

	BPtr<char> file_data = os_quick_read_utf8_file(path.c_str());
	string err;
	Json d = Json::parse(file_data, err);

	if (err != "") {
		return IMPORTER_ERROR_DURING_CONVERSION;
	}

	QDir dir(path.c_str());

	TranslateOSStudio(d);
	TranslatePaths(d, QDir::cleanPath(dir.filePath("..")).toStdString());

	Json::object obj = d.object_items();

	if (name != "") {
		obj["name"] = name;
	} else {
		obj["name"] = "OBS Studio Import";
	}

	res = obj;

	return IMPORTER_SUCCESS;
}
