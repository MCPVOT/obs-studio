#pragma once

#include <filesystem>
#include <string>

/* Migration of the legacy single scene collection (basic/scenes.json) into
 * the scene collections directory, kept free of Qt and OBSApp so it can be
 * unit tested. */

/* Moves basicPath/scenes.json to sceneCollectionPath/<untitledName>.json.
 * Does nothing if basicPath or sceneCollectionPath doesn't exist or already
 * exists, respectively. Failures are logged. */
void migrate_basic_scene_collection(const std::filesystem::path &basicPath,
				    const std::filesystem::path &sceneCollectionPath, const std::string &untitledName);
