local host_system = os.host()
local host_architecture = "x86_64"
if host_system ~= "windows" then
    host_architecture = os.outputof("uname -m"):gsub("%s+", ""):lower()
    if host_architecture == "amd64" then
        host_architecture = "x86_64"
    elseif host_system == "linux" and host_architecture == "arm64" then
        host_architecture = "aarch64"
    end
end

local premake_architecture = host_architecture
if host_architecture == "aarch64" or host_architecture == "arm64" then
    premake_architecture = "ARM64"
end

local faiss_root = "Gugluu/Vendor/FAISS"
local openblas_root = "Gugluu/Vendor/openBLAS"
local openblas_debug_config = "DEBUG"
local openblas_release_config = "RELEASE"
if host_system ~= "windows" then
    local dependency_suffix = host_system .. "-" .. host_architecture
    faiss_root = faiss_root .. "/" .. dependency_suffix
    openblas_root = openblas_root .. "/" .. dependency_suffix
    openblas_debug_config = "Debug"
    openblas_release_config = "Release"
end

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

workspace "Gugluu"
    location "."
    configurations { "Debug", "Release" }
    platforms { premake_architecture }
    architecture (premake_architecture)
    startproject "Gugluu"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    flags { "MultiProcessorCompile" }

    filter "configurations:Debug"
        symbols "On"
        defines { "DEBUG", "_DEBUG", "_CONSOLE" }

    filter "configurations:Release"
        optimize "Speed"
        defines { "NDEBUG", "_CONSOLE" }

    filter "system:windows"
        systemversion "latest"
        defines { "UNICODE", "_UNICODE" }
        buildoptions { "/utf-8", "/permissive-", "/W3", "/openmp" }

    filter "system:linux"
        buildoptions { "-fopenmp" }
        linkoptions { "-fopenmp" }
        links { "gomp", "pthread", "dl", "m" }

    filter "system:macosx"
        buildoptions { "-Xpreprocessor", "-fopenmp" }
        linkoptions { "-lomp" }
        links { "pthread" }

    filter {}

project "Gugluu"
kind "ConsoleApp"
targetname "Gugluu"
location "./Gugluu"
targetdir("Bin/" .. outputdir .. "/%{prj.name}")
objdir("Bin-Int/" .. outputdir .. "/%{prj.name}")
debugdir "Gugluu"

    files {
        "./Gugluu/src/**.cpp",
        "./Gugluu/src/**.h",
    }

    includedirs {
        ".",
        "./Gugluu/src",
        "./Gugluu/Vendor/spdlog/include",
        faiss_root .. "/Release/include",
        openblas_root .. "/" .. openblas_release_config .. "/include/openblas",
        "./Gugluu/Vendor/ONNX/include",
    }

    pchheader "Gugluu/src/core/pch.h"
    pchsource "Gugluu/src/core/pch.cpp"

    libdirs { "./Gugluu/Vendor/ONNX/lib" }

    filter "configurations:Debug"
        libdirs {
            faiss_root .. "/Debug/lib",
            openblas_root .. "/" .. openblas_debug_config .. "/lib",
        }

    filter "configurations:Release"
        libdirs {
            faiss_root .. "/Release/lib",
            openblas_root .. "/" .. openblas_release_config .. "/lib",
        }

    filter {}

    links { "faiss", "openblas" }

    filter "system:windows"
        links { "onnxruntime" }
        linkoptions { '/IMPLIB:"Build/Premake/obj/%{cfg.buildcfg}-%{cfg.system}-' .. host_architecture .. '/Gugluu.lib"' }
        debugenvs { "PATH=" .. path.getabsolute("Gugluu/Vendor/ONNX/lib") .. ";%PATH%" }
        postbuildcommands {
            '{COPY} "%{prj.location}/Vendor/ONNX/lib/onnxruntime.dll" "%{cfg.targetdir}"',
            '{COPY} "%{prj.location}/Vendor/ONNX/lib/onnxruntime_providers_shared.dll" "%{cfg.targetdir}"',
        }

    -- filter "system:linux"
    --     links { "onnxruntime" }
    --     debugenvs { "LD_LIBRARY_PATH=Gugluu/Vendor/ONNX/lib:$LD_LIBRARY_PATH" }

    -- filter "system:macosx"
    --     links { "onnxruntime" }
    --     debugenvs { "DYLD_LIBRARY_PATH=Gugluu/Vendor/ONNX/lib:$DYLD_LIBRARY_PATH" }

    filter {}