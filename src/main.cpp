#include "gctracehooks.h"
#include "variabletracehooks.h"
#include "bugfix.h"

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface *F4SE, F4SE::PluginInfo *Info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S:%e TID %5t] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	Info->infoVersion = F4SE::PluginInfo::kVersion;
	Info->name = Version::PROJECT.data();
	Info->version = Version::MAJOR;

	if (F4SE->IsEditor())
	{
		logger::critical("Loaded in editor"sv);
		return false;
	}

	const auto ver = F4SE->RuntimeVersion();

	if (ver < F4SE::RUNTIME_1_10_162)
	{
		logger::critical(FMT_STRING("Unsupported runtime v{}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface *F4SE)
{
	F4SE::Init(F4SE);

	auto iniPath = "Data/F4SE/Plugins/GCBugFix.ini";
	int logFlushTime = GetPrivateProfileIntA("GC", "LogFlushTime", 5, iniPath);
	int logGCPasses = GetPrivateProfileIntA("GC", "LogPasses", 0, iniPath);
	int traceGCVariables = GetPrivateProfileIntA("GC", "TraceVariables", 0, iniPath);
	int applyFix = GetPrivateProfileIntA("GC", "ApplyFix", 0, iniPath);

	spdlog::flush_every(std::chrono::seconds(logFlushTime));
	logger::info(FMT_STRING("LogFlushTime = {}"), logFlushTime);
	logger::info(FMT_STRING("LogPasses = {}"), logGCPasses);
	logger::info(FMT_STRING("TraceVariables = {}"), traceGCVariables);
	logger::info(FMT_STRING("ApplyFix = {}"), applyFix);

	// Alloc trampoline space
	auto& trampoline = F4SE::GetTrampoline();

	if (trampoline.empty())
		F4SE::AllocTrampoline(1024);

	// Set up hooks
	if (logGCPasses)
		GCTraceHooks::InstallHooks();

	if (traceGCVariables)
		VariableTraceHooks::InstallHooks();

	if (applyFix)
		Bugfix::InstallHooks();

	return true;
}
