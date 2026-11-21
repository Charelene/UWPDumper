#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#ifdef IPC_EXPORT
#define IPC_API __declspec(dllexport)
#else
#define IPC_API __declspec(dllimport)
#endif

/// Exports
namespace IPC
{

struct MessageEntry
{
	MessageEntry() = default;
	explicit MessageEntry(const wchar_t* String);

	static constexpr std::size_t StringSize = 1024;
	wchar_t String[StringSize] = { 0 };
};

IPC_API void SetClientProcess(std::uint32_t ProcessID);
IPC_API std::uint32_t GetClientProcess();
IPC_API void SetTargetProcess(std::uint32_t ProcessID);
IPC_API std::uint32_t GetTargetProcess();

// Whether the dumper should also dump the package's optional packages (DLC).
IPC_API void SetDumpOptionalPackages(bool Enabled);
IPC_API bool GetDumpOptionalPackages();

// The dump folder as the injector computed it (the package's TempState\DUMP).
// The injected process sees a redirected LOCALAPPDATA, so it cannot derive the
// same path by itself.
constexpr std::size_t DumpPathSize = 1024;
IPC_API void SetDumpPath(const wchar_t* Path);
IPC_API const wchar_t* GetDumpPath();

constexpr std::int32_t InvalidThread = -1;

IPC_API void SetTargetThread(std::int32_t ThreadID);
IPC_API std::int32_t GetTargetThread();
IPC_API void ClearTargetThread();

// String messaging
IPC_API void PushMessage(const wchar_t* Format, ...);
IPC_API bool PopMessage(std::wstring& Output);
IPC_API std::size_t MessageCount();
}
