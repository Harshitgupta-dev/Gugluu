#include "Gugluu/src/pch.h"
#include "Gugluu/src/DataLoader.h"
#include "Gugluu/src/Embedding.h"
#include "Gugluu/src/ui.h"

#include <chrono>

int main(int a_ArgumentCount, char* a_Arguments[])
{
	try
	{
		if (a_ArgumentCount > 2)
			throw std::invalid_argument(
				"Usage: Gugluu [directory-to-index]");

		const std::filesystem::path c_RootDirectory = a_ArgumentCount == 2
			? std::filesystem::path(a_Arguments[1])
			: GugluuUI::PromptForDirectory();
		const std::string c_Query = GugluuUI::PromptForSearchQuery();
		const auto c_StartTime = std::chrono::steady_clock::now();

		PRINT("Indexing files...");
		const auto& c_Records = DataLoader::GetData(c_RootDirectory);
		const std::vector<float> c_QueryEmbedding = CreateEmbedding(c_Query);
		const auto c_Matches =
			Gugluu::CalculateSimilarity(c_Records, c_QueryEmbedding);
		GugluuUI::DisplayResults(c_Matches, c_Records.size());

		const auto c_Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - c_StartTime);
		PRINT("Completed in " << c_Elapsed.count() << " ms.");
		return EXIT_SUCCESS;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Gugluu error: " << exception.what() << '\n';
		return EXIT_FAILURE;
	}
}