#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Gugluu/src/Constants.h"
#include "Gugluu/src/similarity.h"

namespace GugluuUI
{
	inline std::filesystem::path PromptForDirectory()
	{
		std::cout << "Directory to index: ";
		std::string directory;
		if (!std::getline(std::cin, directory) || directory.empty())
			throw std::runtime_error("A directory to index is required.");

		return std::filesystem::path(directory);
	}

	inline std::string PromptForSearchQuery()
	{
		std::cout << "Search: ";
		std::string query;
		if (!std::getline(std::cin, query) || query.empty())
			throw std::runtime_error("A non-empty search query is required.");

		return query;
	}

	inline void DisplayResults(
		const std::vector<Gugluu::SimilarityMatch>& a_Matches,
		std::size_t a_IndexedFileCount)
	{
		if (a_Matches.empty())
		{
			std::cout << "No matching files were found.\n";
			return;
		}

		std::cout << "Top " << GugluuConstants::c_TopResultCount
			<< " matches:\n";
		const std::size_t c_ResultCount = (std::min)(
			GugluuConstants::c_TopResultCount,
			a_Matches.size());

		for (std::size_t index = 0; index < c_ResultCount; ++index)
			std::cout << a_Matches[index].second.string()
				<< " (score: " << a_Matches[index].first << ")\n";

		std::cout << "Searched " << a_IndexedFileCount << " files.\n";
	}
}
