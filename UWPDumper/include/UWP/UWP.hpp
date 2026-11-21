#pragma once
#include <string>
#include <vector>

// UWP Helper class
namespace UWP
{
namespace Current
{
std::wstring GetFamilyName();

std::wstring GetFullName();

std::wstring GetArchitecture();

std::wstring GetPublisher();

std::wstring GetPublisherID();

std::wstring GetPackagePath();

// An optional package (DLC/content package) that is installed for the current
// package. Optional packages declare the current package as their main package
// (uap3:MainPackageDependency), so they live in the current package's graph and
// are reachable from the process that runs it.
struct OptionalPackage
{
	std::wstring Name; // Package identity name, eg. "Microsoft.ForzaHorizon3HotWheels"
	std::wstring FullName;
	std::wstring FamilyName;
	std::wstring Path;
};

struct OptionalPackages
{
	// False when this version of Windows does not know PACKAGE_FILTER_OPTIONAL
	// (Windows 10 before 1703). Packages is empty in that case.
	bool Supported = false;
	// GetLastError()-style code of the failed query, when Supported is false.
	long Error = 0;
	std::vector<OptionalPackage> Packages;
};

// Optional packages that belong to the current package. Must be called from
// inside the package (the injected dumper thread), not from the injector.
OptionalPackages GetOptionalPackages();

namespace Storage
{
/*
Apps from the same publisher may share files using the Publisher folder located at:
"%user_folder%\AppData\Local\Publishers\%PublisherID%\"
*/
std::wstring GetPublisherPath();

// Package Storage folders
std::wstring GetStoragePath();

std::wstring GetLocalPath();

std::wstring GetRoamingPath();

std::wstring GetTemporaryPath();
}
}
}
