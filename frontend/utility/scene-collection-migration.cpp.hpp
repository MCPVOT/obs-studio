#include "frontend/build/frontend-utility-scene-collection-migration.pch.hpp"

void migrate_basic_scene_collection(const std::filesystem::path &basicPath,
				    const std::filesystem::path &sceneCollectionPath, const std::string &untitledName)
{
	if (!std::filesystem::exists(basicPath)) {
		return;
	}

	if (std::filesystem::exists(sceneCollectionPath)) {
		return;
	}

	const std::filesystem::path sourceFile = basicPath / std::filesystem::u8path("scenes.json");
	if (!std::filesystem::exists(sourceFile)) {
		return; // A fresh profile: nothing to migrate.
	}

	try {
		std::filesystem::create_directories(sceneCollectionPath);
	} catch (const std::filesystem::filesystem_error &error) {
		blog(LOG_ERROR,
		     "Failed to create scene collection directory for migration from basic scene collection\n%s",
		     error.what());
		return;
	}

	const std::filesystem::path destinationFile =
		(sceneCollectionPath / std::filesystem::u8path(untitledName)).replace_extension(".json");

	try {
		std::filesystem::rename(sourceFile, destinationFile);
	} catch (const std::filesystem::filesystem_error &error) {
		blog(LOG_ERROR, "Failed to rename basic scene collection file:\n%s", error.what());
		return;
	}
}
