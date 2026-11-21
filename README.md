# UWPDumper [![Build](https://github.com/Charelene/UWPDumper/actions/workflows/ci.yml/badge.svg)](https://github.com/Charelene/UWPDumper/actions/workflows/ci.yml) [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/Wunkolo/UWPDumper/master/LICENSE)
## [Download latest binary here!](https://github.com/Charelene/UWPDumper/releases/)

---

DLL and Injector for dumping UWP applications at run-time to bypass encrypted file system protection.

This fork of [Wunkolo/UWPDumper](https://github.com/Wunkolo/UWPDumper) can also
dump installed optional packages (DLC) with `-o`. See
[Dumping DLC](#dumping-dlc-optional-packages) below.

![Demo1](media/demo1.gif)

![Demo2](media/demo2.gif)


Run `UWPInjector.exe` and enter valid UWP Process ID to inject into.
App file system will be dumped into:

`C:\Users\(Username)\AppData\Local\Packages\(Package Family Name)\TempState\DUMP`

To get a list of command line arguments run the command

`UWPInjector.exe -h`

## Dumping DLC (optional packages)

Games on the Microsoft Store often ship their DLC as separate *optional packages*.
For example the Forza Horizon 3 expansions are the optional packages
`Microsoft.ForzaHorizon3HotWheels` and `Microsoft.ForzaHorizon3BlizzardMountain`,
and they live in their own `WindowsApps` folders. Copying those folders from
outside the game fails on the protected content files, so the files have to be
read from inside the running game.

Pass `-o` to dump the installed optional packages as well:

```
UWPInjector.exe -p 1234 -o
```

The main package is dumped into the dump folder as before. Each installed
optional package gets its own subfolder named after its package identity:

```
DUMP\
    AppxManifest.xml                              <- main package
    media\
    ...
    Microsoft.ForzaHorizon3HotWheels\             <- optional package
        AppxManifest.xml
        media\
        ...
    Microsoft.ForzaHorizon3BlizzardMountain\
        AppxManifest.xml
        ...
```

This works for optional packages that declare the main package through
`uap3:MainPackageDependency`, on Windows 10 version 1703 or newer. On older
Windows versions the dumper dumps the main package and reports that optional
packages are not supported. Apps without optional packages are dumped exactly as
before.

`-o` can be combined with the other options, for example
`UWPInjector.exe -p 1234 -o -d D:\dumps\forza`.

## Command line

```
UWPInjector.exe [-p pid] [-d folder] [-o] [-c] [-l]
```

| Option | Meaning |
| --- | --- |
| `-p {pid}` | Process ID to inject into. Without it the injector lists running UWP apps and asks for one. |
| `-d {folder}` | Dump into a folder of your choice instead of the package's `TempState\DUMP`. |
| `-o` | Also dump the installed optional packages (DLC), each into its own subfolder. |
| `-c` | Continuous/automation mode: do not pause and do not open the dump folder at the end. |
| `-l` | Write the log next to the injector as `<package family name> <timestamp>.txt`. |

`UWPDumper.dll` and `UWPInjector.exe` must stay in the same folder.

## Build

UWPDumper requires the [Windows 10 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk) to be compiled.

With Visual Studio 2022 (or the Build Tools) and the Windows 10/11 SDK:

```
msbuild /m /property:Configuration=Release /property:Platform=x64
```

Or with CMake (MSVC or MinGW-w64):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binaries are written to `build/bin`. MinGW builds link the C++ runtime and
`libwinpthread` statically, so only Windows system DLLs are needed.

## Notes

- The dumper runs inside the target application's process and container. That is
  what allows it to read files that the normal file system denies to other
  processes.
- The dump folder has to be writable by the application's package. The injector
  grants the application package access to a `-d` target folder.
- Dumping is file-by-file and shows progress in the injector window. Large games
  and their DLC can take a while and need the corresponding disk space.

## Credits

- Forked from [UWPDumper](https://github.com/Wunkolo/UWPDumper) by Wunkolo.
  This fork adds dumping of installed optional packages (DLC) with `-o`.
