#pragma once
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

class Log
{
public:
	static void s_Init();
	static std::shared_ptr<spdlog::logger> s_GetLogger();
	static void s_SetLevel(spdlog::level::level_enum a_Level);

private:
	static std::shared_ptr<spdlog::logger> s_Logger;
};

#define LOG_TRACE(...) Log::s_GetLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...) Log::s_GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) Log::s_GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Log::s_GetLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Log::s_GetLogger()->critical(__VA_ARGS__)