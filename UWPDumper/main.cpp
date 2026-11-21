#include <cstddef>
#include <fstream>
#include <iomanip>
#include <string>
#include <system_error>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>
namespace fs = std::filesystem;

#define WIDEIFYIMP(x) L##x
#define WIDEIFY(x) WIDEIFYIMP(x)

#include <UWP/UWP.hpp>

#include <UWP/DumperIPC.hpp>

namespace
{
struct DumpResult
{
	std::size_t Copied = 0;
	std::size_t Failed = 0;
};

std::wstring ErrorMessage(const std::error_code& Error)
{
	const std::string Message = Error.message();
	return std::wstring(Message.begin(), Message.end());
}

std::vector<fs::directory_entry> CollectFiles(const fs::path& Root)
{
	std::vector<fs::directory_entry> FileList;

	std::error_code Error;
	fs::recursive_directory_iterator Entry(
		Root,
		fs::directory_options::skip_permission_denied,
		Error
	);
	const fs::recursive_directory_iterator End;

	while( !Error && Entry != End )
	{
		std::error_code FileError;
		if( Entry->is_regular_file(FileError) && !FileError )
		{
			FileList.push_back(*Entry);
		}
		Entry.increment(Error);
	}

	if( Error )
	{
		IPC::PushMessage(
			L"Error listing %s:\n\t%s\n",
			Root.c_str(),
			ErrorMessage(Error).c_str()
		);
	}

	return FileList;
}

// Copies every regular file below SourceRoot to DestRoot, keeping the relative
// layout. SourceRoot must not end in a separator.
DumpResult DumpTree(const fs::path& SourceRoot, const fs::path& DestRoot)
{
	DumpResult Result;

	const std::wstring SourceRootString = SourceRoot.wstring();
	std::vector<fs::directory_entry> FileList = CollectFiles(SourceRoot);

	IPC::PushMessage(L"\t%zu files\n", FileList.size());

	// Heap buffer: a 1 MB local would overflow the dumper thread's stack.
	std::vector<char> CopyBuffer(1 << 20);

	std::size_t Index = 0;
	for( const auto& File : FileList )
	{
		++Index;

		try
		{
			const std::wstring ReadPath = File.path().wstring();

			if(
				ReadPath.size() <= SourceRootString.size() ||
				ReadPath.compare(0, SourceRootString.size(), SourceRootString) != 0
			)
			{
				IPC::PushMessage(L"Skipping file outside %s:\n\t%s\n", SourceRoot.c_str(), ReadPath.c_str());
				++Result.Failed;
				continue;
			}

			std::wstring Relative = ReadPath.substr(SourceRootString.size());
			while( !Relative.empty() && (Relative.front() == L'\\' || Relative.front() == L'/') )
			{
				Relative.erase(Relative.begin());
			}

			const fs::path WritePath = DestRoot / Relative;

			IPC::PushMessage(
				L"%*.*s %*zu bytes %*zu/%zu\n",
				60, 60,
				ReadPath.c_str() + (ReadPath.length() > 60 ? (ReadPath.length() - (60)) : 0),
				15,
				static_cast<std::size_t>(fs::file_size(File)),
				20,
				Index,
				FileList.size()
			);

			std::error_code ErrorCode;
			fs::create_directories(WritePath.parent_path(), ErrorCode);
			if( ErrorCode )
			{
				IPC::PushMessage(
					L"Error creating subfolder: %s\n\t%s\n",
					WritePath.parent_path().c_str(),
					ErrorMessage(ErrorCode).c_str()
				);
				++Result.Failed;
				continue;
			}

			std::ifstream SourceFile(ReadPath.c_str(), std::ios::binary);
			if( !SourceFile.is_open() )
			{
				IPC::PushMessage(
					L"Error opening %s for reading\n",
					ReadPath.c_str()
				);
				++Result.Failed;
				continue;
			}

			std::ofstream DestFile(WritePath.c_str(), std::ios::binary);
			if( !DestFile.is_open() )
			{
				IPC::PushMessage(
					L"Error opening %s for writing\n",
					WritePath.c_str()
				);
				++Result.Failed;
				continue;
			}

			// Copy in chunks: inserting rdbuf() directly reports an error for
			// empty files and for files that grow while being read.
			while( SourceFile )
			{
				SourceFile.read(
					CopyBuffer.data(),
					static_cast<std::streamsize>(CopyBuffer.size())
				);
				const std::streamsize Count = SourceFile.gcount();
				if( Count > 0 )
				{
					DestFile.write(CopyBuffer.data(), Count);
				}
			}

			if( SourceFile.bad() || !DestFile )
			{
				IPC::PushMessage(
					L"Error copying file data:\n"
					"\t%s\n"
					"\tto\n"
					"\t%s\n",
					ReadPath.c_str(),
					WritePath.c_str()
				);
				++Result.Failed;
				continue;
			}

			++Result.Copied;
		}
		catch( std::exception& Exception )
		{
			IPC::PushMessage(
				L"Exception {%s}:\n"
				"\t[%s]\n",
				File.path().c_str(),
				Exception.what()
			);
			++Result.Failed;
		}
	}

	return Result;
}

std::wstring GetDumpPath()
{
	// The injector computes the real package TempState\DUMP path. The injected
	// process sees a redirected LOCALAPPDATA, so ask the injector first.
	const std::wstring InjectedPath = IPC::GetDumpPath();
	if( !InjectedPath.empty() )
	{
		return InjectedPath;
	}

	const std::wstring TemporaryPath = UWP::Current::Storage::GetTemporaryPath();
	if( !TemporaryPath.empty() )
	{
		return TemporaryPath + L"\\DUMP";
	}

	// Fallback for the unlikely case that the package storage path cannot be
	// determined (the injector normally redirects TempState to the dump folder).
	wchar_t TempPath[MAX_PATH] = { 0 };
	const DWORD Length = GetTempPathW(MAX_PATH, TempPath);
	if( Length != 0 && Length < MAX_PATH )
	{
		return std::wstring(TempPath) + L"DUMP";
	}

	return L"DUMP";
}
}

std::uint32_t __stdcall DumperThread(void* DLLHandle)
{
	const std::wstring DumpPathString = GetDumpPath();
	const fs::path DumpPath(DumpPathString);

	IPC::SetTargetThread(GetCurrentThreadId());

	IPC::PushMessage(L"UWPDumper Build date(%ls : %ls)\n", WIDEIFY(__DATE__), WIDEIFY(__TIME__));
	IPC::PushMessage(L"\t-https://github.com/Wunkolo/UWPDumper\n");
	IPC::PushMessage(L"Publisher:\n\t%s\n", UWP::Current::GetPublisher().c_str());
	IPC::PushMessage(L"Publisher ID:\n\t%s\n", UWP::Current::GetPublisherID().c_str());
	IPC::PushMessage(L"Publisher Path:\n\t%s\n", UWP::Current::Storage::GetPublisherPath().c_str());
	IPC::PushMessage(L"Package Path:\n\t%s\n", UWP::Current::GetPackagePath().c_str());
	IPC::PushMessage(L"Package Name:\n\t%s\n", UWP::Current::GetFullName().c_str());
	IPC::PushMessage(L"Family Name:\n\t%s\n", UWP::Current::GetFamilyName().c_str());

	IPC::PushMessage(L"Dump Path:\n\t%s\n", DumpPath.c_str());

	const std::wstring PackagePath = UWP::Current::GetPackagePath();
	if( PackagePath.empty() )
	{
		IPC::PushMessage(L"Unable to determine the package install location\n");
		IPC::ClearTargetThread();
		FreeLibraryAndExitThread(
			reinterpret_cast<HMODULE>(DLLHandle),
			EXIT_FAILURE
		);
	}

	IPC::PushMessage(L"\nDumping main package:\n\t%s\n", PackagePath.c_str());
	const DumpResult MainResult = DumpTree(fs::path(PackagePath), DumpPath);

	IPC::PushMessage(
		L"Main package: %zu files copied, %zu failed\n",
		MainResult.Copied,
		MainResult.Failed
	);

	if( IPC::GetDumpOptionalPackages() )
	{
		const UWP::Current::OptionalPackages Optional = UWP::Current::GetOptionalPackages();

		if( !Optional.Supported )
		{
			IPC::PushMessage(
				L"Optional packages (DLC) are not supported on this version of Windows (error %ld)\n",
				Optional.Error
			);
		}
		else if( Optional.Packages.empty() )
		{
			IPC::PushMessage(L"No optional packages (DLC) are installed for this package\n");
		}
		else
		{
			IPC::PushMessage(
				L"Optional packages (DLC): %zu\n",
				Optional.Packages.size()
			);

			for( const auto& Package : Optional.Packages )
			{
				IPC::PushMessage(
					L"\t%s\n\t\t%s\n",
					Package.Name.c_str(),
					Package.Path.c_str()
				);
			}

			for( const auto& Package : Optional.Packages )
			{
				const fs::path Destination = DumpPath / Package.Name;

				IPC::PushMessage(
					L"\nDumping optional package %s:\n\t%s\n\tto\n\t%s\n",
					Package.Name.c_str(),
					Package.Path.c_str(),
					Destination.c_str()
				);

				const DumpResult Result = DumpTree(fs::path(Package.Path), Destination);

				IPC::PushMessage(
					L"%s: %zu files copied, %zu failed\n",
					Package.Name.c_str(),
					Result.Copied,
					Result.Failed
				);
			}
		}
	}

	IPC::PushMessage(L"\nDump complete!\n\tPath:\n\t%s\n", DumpPath.c_str());
	IPC::ClearTargetThread();

	FreeLibraryAndExitThread(
		reinterpret_cast<HMODULE>(DLLHandle),
		EXIT_SUCCESS
	);
}

extern "C" std::int32_t __stdcall DllMain(HINSTANCE hDLL, std::uint32_t Reason, void* Reserved)
{
	switch( Reason )
	{
	case DLL_PROCESS_ATTACH:
	{
		IPC::PushMessage(L"DLL Attached to process %u\n", GetCurrentProcessId());
		if( IPC::GetTargetProcess() == GetCurrentProcessId() )
		{
			IPC::PushMessage(L"Creating dumper thread for process %u\n", GetCurrentProcessId());
			CreateThread(
				nullptr,
				0,
				reinterpret_cast<unsigned long(__stdcall*)(void*)>(&DumperThread),
				hDLL,
				0,
				nullptr
			);
		}
		break;
	}
	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
	{
		break;
	}
	}

	return true;
}
