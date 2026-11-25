#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace UtilsGugluu
{
    inline std::filesystem::path ApplicationDirectory()
    {
#ifdef _WIN32
        std::vector<wchar_t> c_ExecutablePathBuffer(32768);
        const DWORD c_PathLength = GetModuleFileNameW(
            nullptr,
            c_ExecutablePathBuffer.data(),
            static_cast<DWORD>(c_ExecutablePathBuffer.size()));

        GUGLUU_ASSERT(
            c_PathLength != 0 && c_PathLength < c_ExecutablePathBuffer.size(),
            "Unable to determine Gugluu's executable path (Windows error "
            << GetLastError() << ").");

        c_ExecutablePathBuffer.resize(c_PathLength);
        const std::wstring c_ExecutablePath(
            c_ExecutablePathBuffer.begin(), c_ExecutablePathBuffer.end());
        return std::filesystem::path(c_ExecutablePath).parent_path();
#elif defined(__APPLE__)
        std::uint32_t c_PathLength = 1024;
        std::vector<char> c_ExecutablePath(c_PathLength);
        int c_Result = _NSGetExecutablePath(
            c_ExecutablePath.data(), &c_PathLength);
        if (c_Result != 0)
        {
            GUGLUU_ASSERT(
                c_PathLength > c_ExecutablePath.size(),
                "Unable to determine Gugluu's executable path size.");
            c_ExecutablePath.resize(c_PathLength);
            c_Result = _NSGetExecutablePath(
                c_ExecutablePath.data(), &c_PathLength);
        }

        GUGLUU_ASSERT(
            c_Result == 0,
            "Unable to determine Gugluu's executable path.");
        return std::filesystem::weakly_canonical(
            std::filesystem::path(c_ExecutablePath.data())).parent_path();
#else
        std::vector<char> c_ExecutablePath(256);
        while (true)
        {
            const ssize_t c_PathLength = readlink(
                "/proc/self/exe",
                c_ExecutablePath.data(),
                c_ExecutablePath.size());
            GUGLUU_ASSERT(
                c_PathLength >= 0,
                "Unable to determine Gugluu's executable path.");

            if (static_cast<std::size_t>(c_PathLength) < c_ExecutablePath.size())
            {
                c_ExecutablePath.resize(static_cast<std::size_t>(c_PathLength));
                return std::filesystem::path(
                    std::string(c_ExecutablePath.begin(), c_ExecutablePath.end()))
                    .parent_path();
            }

            GUGLUU_ASSERT(
                c_ExecutablePath.size() <= 1024 * 1024,
                "Executable path exceeds the supported length.");
            c_ExecutablePath.resize(c_ExecutablePath.size() * 2);
        }
#endif
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

        GUGLUU_ASSERT(
            false,
            "Unable to locate the Gugluu workspace from working directory: "
            << c_CurrentPath.string());
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

    inline std::string& ToLowerCase(std::string& a_String)
    {
        std::transform(a_String.begin(), a_String.end(), a_String.begin(),
            [](unsigned char a_Character)
            {
                return static_cast<char>(std::tolower(a_Character));
            });

        return a_String;
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
