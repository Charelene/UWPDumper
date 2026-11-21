#include <UWP/UWP.hpp>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <appmodel.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <vector>

namespace
{
// Copy a string out of a PACKAGE_ID/PACKAGE_INFO, whose pointers only stay
// valid while the buffer that GetCurrentPackageInfo/GetCurrentPackageId filled
// in is alive.
std::wstring CopyString(const wchar_t* String)
{
	if( String == nullptr )
	{
		return L"";
	}
	return std::wstring(String);
}

// The app-model APIs return the package location without a trailing separator,
// but be defensive: the dump code removes the package path prefix from every
// file path by length.
std::wstring StripTrailingSeparators(std::wstring Path)
{
	while( Path.size() > 1 && (Path.back() == L'\\' || Path.back() == L'/') )
	{
		Path.pop_back();
	}
	return Path;
}

std::wstring GetEnvironmentString(const wchar_t* Name)
{
	const DWORD Length = GetEnvironmentVariableW(Name, nullptr, 0);
	if( Length == 0 )
	{
		return L"";
	}

	std::wstring Value(Length, L'\0');
	const DWORD Written = GetEnvironmentVariableW(Name, Value.data(), Length);
	if( Written == 0 || Written >= Length )
	{
		return L"";
	}

	Value.resize(Written);
	return Value;
}

std::wstring GetUserLocalAppDataPath()
{
	std::wstring LocalAppData = GetEnvironmentString(L"LOCALAPPDATA");
	if( !LocalAppData.empty() )
	{
		return LocalAppData;
	}

	std::wstring UserProfile = GetEnvironmentString(L"USERPROFILE");
	if( UserProfile.empty() )
	{
		return L"";
	}
	return UserProfile + L"\\AppData\\Local";
}
}

std::wstring UWP::Current::GetFamilyName()
{
	UINT32 Length = 0;
	LONG Result = GetCurrentPackageFamilyName(&Length, nullptr);

	if( Result != ERROR_INSUFFICIENT_BUFFER || Length == 0 )
	{
		return L"";
	}

	std::wstring FamilyName(Length, L'\0');
	Result = GetCurrentPackageFamilyName(&Length, FamilyName.data());
	if( Result != ERROR_SUCCESS )
	{
		return L"";
	}

	// On success Length includes the terminating null.
	if( Length > 0 && FamilyName.size() >= Length )
	{
		FamilyName.resize(Length - 1);
	}
	return FamilyName;
}

std::wstring UWP::Current::GetFullName()
{
	UINT32 Length = 0;
	LONG Result = GetCurrentPackageFullName(&Length, nullptr);

	if( Result != ERROR_INSUFFICIENT_BUFFER || Length == 0 )
	{
		return L"";
	}

	std::wstring FullName(Length, L'\0');
	Result = GetCurrentPackageFullName(&Length, FullName.data());
	if( Result != ERROR_SUCCESS )
	{
		return L"";
	}

	if( Length > 0 && FullName.size() >= Length )
	{
		FullName.resize(Length - 1);
	}
	return FullName;
}

std::wstring UWP::Current::GetArchitecture()
{
	UINT32 Size = 0;
	if( GetCurrentPackageId(&Size, nullptr) != ERROR_INSUFFICIENT_BUFFER || Size == 0 )
	{
		return L"Unknown";
	}

	std::vector<BYTE> Buffer(Size);
	if( GetCurrentPackageId(&Size, Buffer.data()) != ERROR_SUCCESS )
	{
		return L"Unknown";
	}

	const auto PackageID = reinterpret_cast<const PACKAGE_ID*>(Buffer.data());

	// PACKAGE_ID.processorArchitecture uses the PROCESSOR_ARCHITECTURE_* values
	// (unlike the WinRT ProcessorArchitecture enum).
	switch( PackageID->processorArchitecture )
	{
	case PROCESSOR_ARCHITECTURE_INTEL:
	{
		return L"x86";
	}
	case PROCESSOR_ARCHITECTURE_ARM:
	{
		return L"ARM";
	}
	case PROCESSOR_ARCHITECTURE_AMD64:
	{
		return L"x64";
	}
	case PROCESSOR_ARCHITECTURE_ARM64:
	{
		return L"ARM64";
	}
	case PROCESSOR_ARCHITECTURE_NEUTRAL:
	{
		return L"Neutral";
	}
	default:
	{
		return L"Unknown";
	}
	}
}

std::wstring UWP::Current::GetPublisher()
{
	UINT32 Size = 0;
	if( GetCurrentPackageId(&Size, nullptr) != ERROR_INSUFFICIENT_BUFFER || Size == 0 )
	{
		return L"";
	}

	std::vector<BYTE> Buffer(Size);
	if( GetCurrentPackageId(&Size, Buffer.data()) != ERROR_SUCCESS )
	{
		return L"";
	}

	const auto PackageID = reinterpret_cast<const PACKAGE_ID*>(Buffer.data());
	return CopyString(PackageID->publisher);
}

std::wstring UWP::Current::GetPublisherID()
{
	UINT32 Size = 0;
	if( GetCurrentPackageId(&Size, nullptr) != ERROR_INSUFFICIENT_BUFFER || Size == 0 )
	{
		return L"";
	}

	std::vector<BYTE> Buffer(Size);
	if( GetCurrentPackageId(&Size, Buffer.data()) != ERROR_SUCCESS )
	{
		return L"";
	}

	const auto PackageID = reinterpret_cast<const PACKAGE_ID*>(Buffer.data());
	return CopyString(PackageID->publisherId);
}

std::wstring UWP::Current::GetPackagePath()
{
	UINT32 Length = 0;
	LONG Result = GetCurrentPackagePath(&Length, nullptr);

	if( Result != ERROR_INSUFFICIENT_BUFFER || Length == 0 )
	{
		return L"";
	}

	std::wstring PackagePath(Length, L'\0');
	Result = GetCurrentPackagePath(&Length, PackagePath.data());
	if( Result != ERROR_SUCCESS )
	{
		return L"";
	}

	if( Length > 0 && PackagePath.size() >= Length )
	{
		PackagePath.resize(Length - 1);
	}
	return StripTrailingSeparators(std::move(PackagePath));
}

UWP::Current::OptionalPackages UWP::Current::GetOptionalPackages()
{
	OptionalPackages Result;

	// PACKAGE_FILTER_OPTIONAL was added with optional package support
	// (Windows 10 1703). Older systems reject it; report that instead of
	// failing the whole dump.
	const UINT32 Filter = PACKAGE_FILTER_HEAD | PACKAGE_FILTER_OPTIONAL;

	UINT32 Size = 0;
	UINT32 Count = 0;
	LONG Error = GetCurrentPackageInfo(Filter, &Size, nullptr, &Count);

	if( (Error != ERROR_INSUFFICIENT_BUFFER && Error != ERROR_SUCCESS) || Size == 0 )
	{
		Result.Error = Error;
		return Result;
	}

	std::vector<BYTE> Buffer(Size);
	Error = GetCurrentPackageInfo(Filter, &Size, Buffer.data(), &Count);
	if( Error != ERROR_SUCCESS )
	{
		Result.Error = Error;
		return Result;
	}

	Result.Supported = true;

	const auto PackageInfo = reinterpret_cast<const PACKAGE_INFO*>(Buffer.data());
	const std::wstring FullName = GetFullName();

	for( UINT32 Index = 0; Index < Count; ++Index )
	{
		const PACKAGE_INFO& Info = PackageInfo[Index];

		// The head package comes back too; only optional packages are dumped
		// into subfolders.
		if( (Info.flags & PACKAGE_PROPERTY_OPTIONAL) == 0 )
		{
			continue;
		}

		OptionalPackage Package;
		Package.Name = CopyString(Info.packageId.name);
		Package.FullName = CopyString(Info.packageFullName);
		Package.FamilyName = CopyString(Info.packageFamilyName);
		Package.Path = StripTrailingSeparators(CopyString(Info.path));

		if( Package.Path.empty() || Package.Name.empty() || Package.FullName == FullName )
		{
			continue;
		}

		Result.Packages.push_back(std::move(Package));
	}

	std::sort(
		Result.Packages.begin(), Result.Packages.end(),
		[](const OptionalPackage& Left, const OptionalPackage& Right)
		{
			return Left.Name < Right.Name;
		}
	);

	return Result;
}

std::wstring UWP::Current::Storage::GetPublisherPath()
{
	std::wstring UserProfile = GetEnvironmentString(L"USERPROFILE");
	if( UserProfile.empty() )
	{
		return L"";
	}

	return UserProfile + L"\\AppData\\Local\\Publishers\\" + GetPublisherID();
}

std::wstring UWP::Current::Storage::GetStoragePath()
{
	std::wstring StoragePath = GetUserLocalAppDataPath();
	if( StoragePath.empty() )
	{
		return L"";
	}

	return StoragePath + L"\\Packages\\" + GetFamilyName();
}

std::wstring UWP::Current::Storage::GetLocalPath()
{
	const std::wstring StoragePath = GetStoragePath();
	return StoragePath.empty() ? L"" : StoragePath + L"\\LocalState";
}

std::wstring UWP::Current::Storage::GetRoamingPath()
{
	const std::wstring StoragePath = GetStoragePath();
	return StoragePath.empty() ? L"" : StoragePath + L"\\RoamingState";
}

std::wstring UWP::Current::Storage::GetTemporaryPath()
{
	const std::wstring StoragePath = GetStoragePath();
	return StoragePath.empty() ? L"" : StoragePath + L"\\TempState";
}
