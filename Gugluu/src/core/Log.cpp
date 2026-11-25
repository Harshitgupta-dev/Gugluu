#include "Gugluu/src/core/pch.h"
#include "Log.h"

#include <mutex>

std::shared_ptr<spdlog::logger> Log::s_Logger;

namespace
{
	std::once_flag s_LoggerInitialization;
}

void Log::s_Init()
{
	std::call_once(s_LoggerInitialization, []()
		{
			spdlog::set_pattern("%^[%T] %n: %v%$");
			Log::s_Logger = spdlog::get("GUGLUU");
			if (!Log::s_Logger)
				Log::s_Logger = spdlog::stdout_color_mt("GUGLUU");
			Log::s_Logger->set_level(spdlog::level::trace);
		});
}

std::shared_ptr<spdlog::logger> Log::s_GetLogger()
{
	s_Init();
	return s_Logger;
}

void Log::s_SetLevel(spdlog::level::level_enum a_Level)
{
	s_Init();
	s_Logger->set_level(a_Level);
}

