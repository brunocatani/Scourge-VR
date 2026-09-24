#include "LogPath.h"

#include <filesystem>
#include <iostream>

int main()
{
	const auto documents = std::filesystem::temp_directory_path() / "ScourgeLogPathTestDocuments";
	const auto expected = documents / "My Games" / "Fallout4VR" / "F4SE";
	const auto actual = LogPath::Fallout4VRLogDirectoryFromDocuments(documents);

	if (actual != expected) {
		std::cerr << "Expected " << expected << ", got " << actual << '\n';
		return 1;
	}

	if (actual == documents / "My Games" / "F4SE") {
		std::cerr << "Log path omitted the Fallout4VR save folder\n";
		return 1;
	}

	return 0;
}
