#include <UWP/DumperIPC.hpp>

#include <array>
#include <atomic>
#include <cstdarg>
#include <cwchar>
#include <Windows.h>

/// API
namespace IPC
{
/// IPC Message Queue
MessageEntry::MessageEntry(const wchar_t* String)
{
	wcscpy_s(this->String, MessageEntry::StringSize, String);
}

// The queue keeps Head and Tail as free-running counters and takes the index
// modulo MaxSize. Head == Tail means empty, and Tail - Head is the number of
// queued entries. Enqueue drops the oldest entry when the queue is full so a
// fast dumper can never make the counters wrap into a bogus "full" state.
template< typename QueueType, std::size_t PoolSize >
class AtomicQueue
{
public:
	using Type = QueueType;
	static constexpr std::size_t MaxSize = PoolSize;

	AtomicQueue() = default;
	~AtomicQueue() = default;

	void Enqueue(const Type& Entry)
	{
		Lock();
		Entries[Tail % MaxSize] = Entry;
		++Tail;
		if( Tail - Head > MaxSize )
		{
			Head = Tail - MaxSize;
		}
		Unlock();
	}

	bool Dequeue(Type& Output)
	{
		Lock();
		if( Head == Tail )
		{
			Unlock();
			return false;
		}
		Output = Entries[Head % MaxSize];
		++Head;
		Unlock();
		return true;
	}

	std::size_t Size()
	{
		Lock();
		const std::size_t Result = Tail - Head;
		Unlock();
		return Result;
	}

	bool Empty()
	{
		return Size() == 0;
	}

private:
	void Lock()
	{
		while( Mutex.test_and_set(std::memory_order_acquire) )
		{
			SwitchToThread();
		}
	}

	void Unlock()
	{
		Mutex.clear(std::memory_order_release);
	}

	std::array<Type, MaxSize> Entries = {};
	std::size_t Head = 0;
	std::size_t Tail = 0;
	std::atomic_flag Mutex = ATOMIC_FLAG_INIT;
};

////// Shared IPC Region //////////////////////////////////////////////////////
// The injector links against UWPDumper.dll, so the loader maps the DLL into
// both processes and these variables live in the same shared section.
#ifdef _MSC_VER
#pragma data_seg("SHARED")
#define SHARED_DATA
#else
#define SHARED_DATA __attribute__((section("SHARED"), shared))
#endif

SHARED_DATA AtomicQueue<MessageEntry, 4096> MessagePool = {};
SHARED_DATA std::atomic<std::size_t> CurMessageCount = 0;

// The process we are sending our data to
SHARED_DATA std::atomic<std::uint32_t> ClientProcess(0);

// The target UWP process we wish to dump
SHARED_DATA std::atomic<std::uint32_t> TargetProcess(0);

// Also dump the optional packages (DLC) of the target package
SHARED_DATA std::atomic<bool> DumpOptional = false;

// Dump folder, filled in by the injector before it injects the DLL
SHARED_DATA wchar_t DumpPathBuffer[DumpPathSize] = { 0 };

SHARED_DATA std::atomic<std::int32_t> TargetThread(InvalidThread);

#ifdef _MSC_VER
#pragma data_seg()
#pragma comment(linker, "/section:SHARED,RWS")
#endif
///////////////////////////////////////////////////////////////////////////////

void SetClientProcess(std::uint32_t ProcessID)
{
	ClientProcess = ProcessID;
}

std::uint32_t GetClientProcess()
{
	return ClientProcess;
}

void SetTargetProcess(std::uint32_t ProcessID)
{
	TargetProcess = ProcessID;
}

std::uint32_t GetTargetProcess()
{
	return TargetProcess;
}

void SetDumpOptionalPackages(bool Enabled)
{
	DumpOptional = Enabled;
}

bool GetDumpOptionalPackages()
{
	return DumpOptional;
}

void SetDumpPath(const wchar_t* Path)
{
	wcsncpy_s(DumpPathBuffer, DumpPathSize, Path != nullptr ? Path : L"", _TRUNCATE);
}

const wchar_t* GetDumpPath()
{
	return DumpPathBuffer;
}

IPC_API void SetTargetThread(std::int32_t ThreadID)
{
	TargetThread = ThreadID;
}

IPC_API std::int32_t GetTargetThread()
{
	return TargetThread;
}

IPC_API void ClearTargetThread()
{
	TargetThread = InvalidThread;
}

void PushMessage(const wchar_t* Format, ...)
{
	std::va_list Args;
	MessageEntry Entry;

	va_start(Args, Format);
	vswprintf_s(
		Entry.String, Entry.StringSize,
		Format, Args
	);
	va_end(Args);

	MessagePool.Enqueue(Entry);
}

bool PopMessage(std::wstring& Output)
{
	MessageEntry Entry;
	if( MessagePool.Dequeue(Entry) )
	{
		Output.assign(Entry.String, wcslen(Entry.String));
		return true;
	}
	return false;
}

std::size_t MessageCount()
{
	return MessagePool.Size();
}
}
