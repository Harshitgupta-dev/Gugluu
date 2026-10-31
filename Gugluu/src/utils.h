#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace UtilsGugluu
{
    inline std::filesystem::path ApplicationDirectory()
    {
        std::vector<wchar_t> executablePath(32768);
        const DWORD c_PathLength = GetModuleFileNameW(
            nullptr,
            executablePath.data(),
            static_cast<DWORD>(executablePath.size()));

        if (c_PathLength == 0 || c_PathLength >= executablePath.size())
            throw std::system_error(
                static_cast<int>(GetLastError()),
                std::system_category(),
                "Unable to determine Gugluu's executable path");

        executablePath.resize(c_PathLength);
        const std::wstring c_ExecutablePath(
            executablePath.begin(), executablePath.end());
        return std::filesystem::path(c_ExecutablePath).parent_path();
    }

    inline std::filesystem::path WorkspacePath(
        const std::filesystem::path& a_RelativePath)
    {
        const std::filesystem::path c_CurrentPath = std::filesystem::current_path();

        for (auto candidate = c_CurrentPath;
            !candidate.empty();
            candidate = candidate.parent_path())
        {
            if (std::filesystem::exists(candidate / "premake5.lua")
                && std::filesystem::is_directory(candidate / "Gugluu" / "src"))
                return candidate / a_RelativePath;

            if (candidate == candidate.parent_path())
                break;
        }

        throw std::runtime_error(
            "Unable to locate the Gugluu workspace from working directory: "
            + c_CurrentPath.string());
    }

    inline std::filesystem::path ResourcePath(
        const std::filesystem::path& a_RelativePath)
    {
        const std::filesystem::path c_ExecutableResource =
            ApplicationDirectory() / a_RelativePath;
        if (std::filesystem::exists(c_ExecutableResource))
            return c_ExecutableResource;

        const std::filesystem::path c_WorkspaceRoot =
            WorkspacePath(std::filesystem::path{});
        const std::filesystem::path c_ProjectResource =
            c_WorkspaceRoot / "Gugluu" / a_RelativePath;
        if (std::filesystem::exists(c_ProjectResource))
            return c_ProjectResource;

        return c_WorkspaceRoot / a_RelativePath;
    }

    inline std::string ToLowerCase(const std::string& a_String)
    {
        std::string lowercased = a_String;
        std::transform(lowercased.begin(), lowercased.end(), lowercased.begin(),
            [](unsigned char a_Character)
            {
                return static_cast<char>(std::tolower(a_Character));
            });
        return lowercased;
    }

    inline void NormalizeWhitespace(std::string& a_String)
    {
        std::string normalized;
        normalized.reserve(a_String.size());
        bool previousWasWhitespace = true;

        for (unsigned char character : a_String)
        {
            if (std::isspace(character))
            {
                if (!previousWasWhitespace)
                    normalized.push_back(' ');
                previousWasWhitespace = true;
            }
            else
            {
                normalized.push_back(static_cast<char>(character));
                previousWasWhitespace = false;
            }
        }

        if (!normalized.empty() && normalized.back() == ' ')
            normalized.pop_back();
        a_String = std::move(normalized);
    }
}
