workspace "Gugluu"
configurations {"Debug", "Release"}
system "windows"
architecture "x86_64"
startproject "Gugluu"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Gugluu"
kind "ConsoleApp"
language "C++"
cppdialect "C++20"
characterset "Unicode"
warnings "Default"
location "Gugluu"
targetdir("Bin/" .. outputdir .. "/%{prj.name}")
objdir("Bin-Int/" .. outputdir .. "/%{prj.name}")
debugdir "Gugluu"

files {"Gugluu/src/**"}
includedirs {".", "src", "Vendor/ONNX/include"}

pchheader "Gugluu/src/pch.h"
pchsource "Gugluu/src/pch.cpp"

libdirs {"%{prj.name}/Vendor/ONNX/lib"}
links {"onnxruntime"}

postbuildcommands {'{COPY} "Vendor/ONNX/lib/onnxruntime.dll" "%{cfg.targetdir}"',
                    '{COPY} "Vendor/ONNX/lib/onnxruntime_providers_shared.dll" "%{cfg.targetdir}"',
                    '{COPY} "../AllowedFileTypes.txt" "%{cfg.targetdir}"',
                    '{COPYDIR} "Vendor/Model/all-MiniLM-L6-v2" "%{cfg.targetdir}/Vendor/Model/all-MiniLM-L6-v2"'}

filter "configurations:Debug"
defines {"DEBUG", "_DEBUG", "_CONSOLE"}
runtime "Debug"
optimize "Off"
symbols "On"

filter "configurations:Release"
defines {"NDEBUG", "_CONSOLE"}
runtime "Release"
optimize "Speed"
symbols "On"

filter {}
