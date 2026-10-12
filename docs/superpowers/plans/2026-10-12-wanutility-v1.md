# WANUtility v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Windows 11–native WinUI 3 app that sends/receives SMS as conversation threads and runs interactive USSD sessions on the built-in Quectel EM120R-GL modem, living in the system tray with toasts and optional auto-start.

**Architecture:** Modem-free logic lives in `Core/` (plain C++20, no WinRT, no precompiled header) and is unit-tested by a separate CppUnitTest DLL. WinRT/Win32 integration lives in `Services/` (one class per responsibility, all owned by a process-wide `AppServices` and used only on the UI thread). XAML pages (`MessagesPage`, `UssdPage`, `SettingsPage`) under a left `NavigationView` talk only to services.

**Tech Stack:** C++20, C++/WinRT, WinUI 3 / Windows App SDK 2.5.1 (packaged, sideloaded), built-in `winsqlite3`, Microsoft CppUnitTest + `vstest.console.exe` (both ship with VS 18 Community), Win32 `Shell_NotifyIcon`.

**Spec:** `docs/superpowers/specs/2026-10-12-wanutility-design.md` — read it alongside this plan.

## Global Constraints

- MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe` (the Build Tools copy is broken: "OutputPath not set").
- vstest: `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\Extensions\TestPlatform\vstest.console.exe`.
- Configuration `Debug`, platform `x64`. Language standard C++20. Every project compiles with `/utf-8` (source files contain Thai literals).
- No new third-party dependencies. SQLite = `winsqlite3` (`#include <winsqlite/winsqlite3.h>`, link `winsqlite3.lib`). Tests = CppUnitTest.
- `Core/` files: no WinRT, no `pch.h`, compiled with `<PrecompiledHeader>NotUsing</PrecompiledHeader>` in the app project. Namespace `wan::core`.
- `Services/` files and pages: `#include "pch.h"` first. Namespace `wan`.
- Threading: all service methods and all XAML access happen on the UI thread. WinRT event handlers (which fire on worker threads) marshal with `wan::Services().dispatcher.TryEnqueue(...)`. Awaiting a WinRT async operation from the UI thread resumes on the UI thread (observed in the spike).
- Restricted capabilities in `Package.appxmanifest`: `runFullTrust`, `cellularMessaging`, `cellularDeviceControl`, `cellularDeviceIdentity`, `chatSystem` (already present).
- Package identity name `29d4b340-cdbf-4ef2-8a8c-cb0bc4b163e0`. Deploy/run only via `scripts/deploy.ps1` (Task 7): it re-registers when the manifest changes and **preserves LocalState** (the DB lives there).
- Logging: `wan::core::Log::Write` writes UTF-8. Never use `std::wofstream` (silently drops Thai).
- **A real SMS costs money: ask the user before every manual send. Never send automatically in tests.**
- **USSD `*103#` may buy a package: navigate menus only, then press End. Never confirm a purchase.**
- Commit after each task (`git commit` needs the repo's `user.name`/`user.email`; prerequisite below). End each commit message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

### Deliberate deviations from the spec (approved in plan review)

- Spec §10.1 says GoogleTest via NuGet. This machine has no `nuget.exe`; Microsoft CppUnitTest + vstest ship with VS 18 Community, so we use them — zero downloads.
- Spec §6.2 lists a `threads` table "maintained alongside messages". We compute threads with a query over `messages` instead: same result, no way for the two to disagree.

## Review Focus

1. **USSD replies that contain numbers but are not menus** (balance `47.79`, dates `12/10/70`, prices `10 บาท`, sizes `1.5GB`) must not produce option buttons. → Task 3 tests.
2. **Outgoing message synced to the Windows store while an incoming SMS is still un-imported** must not hide that incoming SMS (importer must not stop scanning at a known *outgoing* entry). → Task 6 tests.
3. **Blank or whitespace input** (empty USSD code, empty SMS body, empty/space-padded recipient) must be a no-op with the button disabled; `088-960-5613` must land in the same thread as `0889605613`. → Task 2 tests + Task 11 Step "Disabled-state check".
4. **Modem disappears mid-operation** (airplane mode during a USSD session or while sending) must end the session / mark the message failed with readable text, never crash or hang the UI. → Task 9 and Task 11 manual steps, Task 16 checklist.
5. **Long Thai SMS (> 70 characters, multi-part UCS-2)** must either arrive whole or fail with a visible error — never silently truncated. → Task 11 manual step (costs one SMS; ask first).

## Prerequisites (once, before Task 1)

- [ ] Git author identity is set for this repo. Ask the user for their GitHub email if not yet given, then:

```powershell
git config user.name "Nate"
git config user.email "<GitHub email from user>"
git config user.name; git config user.email
```
Expected: both values print.

- [ ] Commit the existing scaffold + spike + docs as the baseline:

```powershell
git add -A
git commit -m "chore: WinUI 3 scaffold, SMS/USSD spike, spec and plan`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git log --oneline
```
Expected: one commit listed.

## File Map

| Path | Responsibility |
|---|---|
| `Core/TextUtil.h` | `Trim`, `TrimView`, `FirstLine` (header-only) |
| `Core/Ucs2.h/.cpp` | `DecodeUtf16BE`, `IsUcs2Dcs` |
| `Core/PhoneNumber.h/.cpp` | `ThreadKeyFor`, `IsPhoneNumberKey` |
| `Core/UssdMenuParser.h/.cpp` | `UssdOption`, `ParseUssdMenu` |
| `Core/MessageDb.h/.cpp` | SQLite persistence (messages, threads query, USSD codes/history, settings) |
| `Core/Log.h/.cpp` | UTF-8 rotating log file |
| `Core/InboxScan.h/.cpp` | Pure stop/collect rule for importing from the Windows store |
| `Tests/WANUtility.Tests.vcxproj` + `Tests/*Tests.cpp` | CppUnitTest DLL compiling `Core/*.cpp` |
| `scripts/test.ps1` | Build + run unit tests |
| `scripts/deploy.ps1` | Build, (re)register package preserving LocalState, launch |
| `scripts/make-tray-icon.py` | Wrap a PNG into `Assets/Tray.ico` |
| `main.cpp` | Custom `wWinMain`: single-instance redirect, then XAML main |
| `Services/Signal.h` | Tiny UI-thread observer list |
| `Services/TimeUtil.h` | `NowMs`, `ToUnixMs`, `FromUnixMs`, `FormatTime` |
| `Services/ErrorText.h` | `DescribeError(hresult_error)`, `Widen(utf8)` |
| `Services/AppServices.h/.cpp` | Process-wide service container, `InitializeServices`, `ReportError` |
| `Services/ModemService.h/.cpp` | Modem readiness, carrier, signal, device info |
| `Services/UssdSessionManager.h/.cpp` | Interactive USSD state machine + payload decoding |
| `Services/InboxImporter.h/.cpp` | Windows chat store → DB import, live + 60 s re-read |
| `Services/SmsSender.h/.cpp` | Send / retry flow |
| `Services/WindowsStoreSync.h/.cpp` | Copy sent messages into the Windows chat store |
| `Services/TrayIcon.h/.cpp` | Tray icon + Open/Exit menu |
| `Services/Notifier.h/.cpp` | Toasts + click-to-open-thread |
| `Services/StartupSetting.h/.cpp` | `StartupTask` wrapper |
| `MainWindow.*` | Mica window, title bar, NavigationView, InfoBars, close-to-tray |
| `MessagesPage.*`, `UssdPage.*`, `SettingsPage.*` | Pages (`.idl`, `.xaml`, `.xaml.h`, `.xaml.cpp` each) |

Build commands used throughout:

```powershell
# Unit tests (Tasks 1–6 and any task touching Core/)
powershell -ExecutionPolicy Bypass -File scripts\test.ps1
# App build only
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" WANUtility.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo
```

---

### Task 1: Test project + UCS-2 decoding

**Files:**
- Create: `Core/TextUtil.h`, `Core/Ucs2.h`, `Core/Ucs2.cpp`
- Create: `Tests/WANUtility.Tests.vcxproj`, `Tests/Ucs2Tests.cpp`, `Tests/TextUtilTests.cpp`
- Create: `scripts/test.ps1`
- Modify: `WANUtility.vcxproj` (add `/utf-8`, include dir, Core item), `WANUtility.slnx` (add test project)

**Interfaces:**
- Produces: `std::wstring wan::core::DecodeUtf16BE(std::span<const uint8_t> bytes)`; `bool wan::core::IsUcs2Dcs(uint32_t dcs)`; `std::wstring_view wan::core::TrimView(std::wstring_view)`; `std::wstring wan::core::Trim(std::wstring_view)`; `std::wstring wan::core::FirstLine(std::wstring_view)`.

- [ ] **Step 1: Create `Core/TextUtil.h`**

```cpp
#pragma once
#include <string>
#include <string_view>

namespace wan::core
{
    inline bool IsSpace(wchar_t c)
    {
        return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == 0x00A0;
    }

    inline std::wstring_view TrimView(std::wstring_view s)
    {
        while (!s.empty() && IsSpace(s.front())) s.remove_prefix(1);
        while (!s.empty() && IsSpace(s.back())) s.remove_suffix(1);
        return s;
    }

    inline std::wstring Trim(std::wstring_view s)
    {
        return std::wstring(TrimView(s));
    }

    // First line of s (up to CR or LF), trimmed.
    inline std::wstring FirstLine(std::wstring_view s)
    {
        return std::wstring(TrimView(s.substr(0, s.find_first_of(L"\r\n"))));
    }
}
```

- [ ] **Step 2: Create `Core/Ucs2.h` (declarations only, no implementation yet)**

```cpp
#pragma once
#include <cstdint>
#include <span>
#include <string>

namespace wan::core
{
    // Decodes UCS-2 / UTF-16 big-endian bytes. A trailing odd byte is ignored.
    std::wstring DecodeUtf16BE(std::span<const uint8_t> bytes);

    // True when a GSM 03.38 / 3GPP TS 23.038 data coding scheme selects the UCS-2 alphabet.
    // dtac USSD replies use 0x48 (observed in the spike).
    bool IsUcs2Dcs(uint32_t dcs);
}
```

- [ ] **Step 3: Create `Core/Ucs2.cpp` with stub bodies so the test project links and the tests fail**

```cpp
#include "Ucs2.h"

namespace wan::core
{
    std::wstring DecodeUtf16BE(std::span<const uint8_t>)
    {
        return {};
    }

    bool IsUcs2Dcs(uint32_t)
    {
        return false;
    }
}
```

- [ ] **Step 4: Create `Tests/WANUtility.Tests.vcxproj`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>18.0</VCProjectVersion>
    <ProjectGuid>{7C1D2B9E-4F7A-4E55-9A51-3B1C2D4E5F60}</ProjectGuid>
    <RootNamespace>WANUtilityTests</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration">
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <PlatformToolset Condition="'$(VisualStudioVersion)' &gt;= '18.0'">v145</PlatformToolset>
    <PlatformToolset Condition="'$(VisualStudioVersion)' &lt; '18.0'">v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
    <UseDebugLibraries Condition="'$(Configuration)'=='Debug'">true</UseDebugLibraries>
    <UseDebugLibraries Condition="'$(Configuration)'=='Release'">false</UseDebugLibraries>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ItemDefinitionGroup>
    <ClCompile>
      <WarningLevel>Level4</WarningLevel>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
      <AdditionalIncludeDirectories>$(VCInstallDir)Auxiliary\VS\UnitTest\include;$(MSBuildThisFileDirectory)..\Core;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions Condition="'$(Configuration)'=='Debug'">_DEBUG;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
    <Link>
      <SubSystem>Windows</SubSystem>
      <AdditionalLibraryDirectories>$(VCInstallDir)Auxiliary\VS\UnitTest\lib\x64;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <AdditionalDependencies>Microsoft.VisualStudio.TestTools.CppUnitTestFramework.lib;winsqlite3.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="..\Core\Ucs2.cpp" />
    <ClCompile Include="Ucs2Tests.cpp" />
    <ClCompile Include="TextUtilTests.cpp" />
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
```

- [ ] **Step 5: Write the failing tests — `Tests/Ucs2Tests.cpp`**

The byte array is the exact `*101#` reply captured from dtac in the spike (102 bytes).

```cpp
#include "CppUnitTest.h"
#include "Ucs2.h"

#include <array>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

TEST_CLASS(Ucs2Tests)
{
public:
    TEST_METHOD(DecodesRealDtacBalanceReply)
    {
        const std::array<uint8_t, 102> raw{
            0x0E, 0x22, 0x0E, 0x2D, 0x0E, 0x14, 0x0E, 0x04, 0x0E, 0x07, 0x0E, 0x40, 0x0E, 0x2B, 0x0E, 0x25,
            0x0E, 0x37, 0x0E, 0x2D, 0x00, 0x34, 0x00, 0x37, 0x00, 0x2E, 0x00, 0x37, 0x00, 0x39, 0x0E, 0x1A,
            0x00, 0x2E, 0x0E, 0x43, 0x0E, 0x0A, 0x0E, 0x49, 0x0E, 0x44, 0x0E, 0x14, 0x0E, 0x49, 0x0E, 0x16,
            0x0E, 0x36, 0x0E, 0x07, 0x00, 0x31, 0x00, 0x32, 0x00, 0x2F, 0x00, 0x31, 0x00, 0x30, 0x00, 0x2F,
            0x00, 0x37, 0x00, 0x30, 0x00, 0x0A, 0x0E, 0x0B, 0x0E, 0x37, 0x0E, 0x49, 0x0E, 0x2D, 0x0E, 0x42,
            0x0E, 0x1B, 0x0E, 0x23, 0x00, 0x20, 0x0E, 0x42, 0x0E, 0x17, 0x0E, 0x23, 0x00, 0x2A, 0x00, 0x31,
            0x00, 0x30, 0x00, 0x33, 0x00, 0x23,
        };
        const std::wstring expected = L"ยอดคงเหลือ47.79บ.ใช้ได้ถึง12/10/70\nซื้อโปร โทร*103#";
        Assert::AreEqual(expected, DecodeUtf16BE(raw));
    }

    TEST_METHOD(IgnoresTrailingOddByte)
    {
        const std::array<uint8_t, 3> raw{ 0x00, 0x41, 0x00 };
        Assert::AreEqual(std::wstring(L"A"), DecodeUtf16BE(raw));
    }

    TEST_METHOD(EmptyInputGivesEmptyString)
    {
        Assert::AreEqual(std::wstring(), DecodeUtf16BE({}));
    }

    TEST_METHOD(DetectsUcs2CodingSchemes)
    {
        Assert::IsTrue(IsUcs2Dcs(0x48));  // dtac (spike)
        Assert::IsTrue(IsUcs2Dcs(0x11));  // UCS-2 preceded by language
        Assert::IsTrue(IsUcs2Dcs(0x98));  // 1001xxxx group, alphabet bits = 10
        Assert::IsFalse(IsUcs2Dcs(0x0F)); // GSM 7-bit, language unspecified
        Assert::IsFalse(IsUcs2Dcs(0x40)); // general group, GSM 7-bit
        Assert::IsFalse(IsUcs2Dcs(0x44)); // general group, 8-bit data
        Assert::IsFalse(IsUcs2Dcs(0xF4)); // 1111xxxx, 8-bit
    }
};
```

- [ ] **Step 6: Write `Tests/TextUtilTests.cpp`**

```cpp
#include "CppUnitTest.h"
#include "TextUtil.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

TEST_CLASS(TextUtilTests)
{
public:
    TEST_METHOD(TrimRemovesSpacesTabsNewlinesAndNbsp)
    {
        Assert::AreEqual(std::wstring(L"*101#"), Trim(L" \t\u00A0*101#\r\n "));
    }

    TEST_METHOD(TrimOfBlankIsEmpty)
    {
        Assert::AreEqual(std::wstring(), Trim(L"  \n "));
    }

    TEST_METHOD(FirstLineStopsAtNewline)
    {
        Assert::AreEqual(std::wstring(L"ยอดคงเหลือ47.79บ."), FirstLine(L" ยอดคงเหลือ47.79บ.\r\nซื้อโปร"));
    }
};
```

- [ ] **Step 7: Create `scripts/test.ps1`**

```powershell
# Builds the unit-test DLL and runs it with vstest. Exit code is non-zero on any failure.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
$vstest = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\Extensions\TestPlatform\vstest.console.exe'
& $msbuild "$root\Tests\WANUtility.Tests.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "Test build failed ($LASTEXITCODE)" }
& $vstest "$root\Tests\x64\Debug\WANUtility.Tests.dll" /Platform:x64
exit $LASTEXITCODE
```

- [ ] **Step 8: Run tests — verify they fail**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: build succeeds; `DecodesRealDtacBalanceReply`, `IgnoresTrailingOddByte`, `DetectsUcs2CodingSchemes` FAIL; `EmptyInputGivesEmptyString` and the three TextUtil tests pass. If the DLL path differs, run `Get-ChildItem Tests -Recurse -Filter WANUtility.Tests.dll` and fix `scripts/test.ps1`.

- [ ] **Step 9: Implement `Core/Ucs2.cpp`**

```cpp
#include "Ucs2.h"

namespace wan::core
{
    std::wstring DecodeUtf16BE(std::span<const uint8_t> bytes)
    {
        std::wstring out;
        out.reserve(bytes.size() / 2);
        for (size_t i = 0; i + 1 < bytes.size(); i += 2)
        {
            out.push_back(static_cast<wchar_t>((bytes[i] << 8) | bytes[i + 1]));
        }
        return out;
    }

    bool IsUcs2Dcs(uint32_t dcs)
    {
        // TS 23.038 (CBS/USSD): 0001 0001 = UCS-2 preceded by language.
        if (dcs == 0x11)
        {
            return true;
        }
        // 01xx xxxx (general data coding) and 1001 xxxx: bits 3..2 select the alphabet, 10 = UCS-2.
        if ((dcs & 0xC0) == 0x40 || (dcs & 0xF0) == 0x90)
        {
            return ((dcs >> 2) & 0x3) == 0x2;
        }
        return false;
    }
}
```

- [ ] **Step 10: Run tests — verify they pass**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: `Total tests: 7. Passed: 7.`

- [ ] **Step 11: Wire `/utf-8`, the include dir and `Core/Ucs2.cpp` into the app project**

In `WANUtility.vcxproj`, in the first `<ItemDefinitionGroup>` (the one with `<PrecompiledHeader>Use</PrecompiledHeader>`), replace

```xml
      <AdditionalOptions>%(AdditionalOptions) /bigobj</AdditionalOptions>
```
with
```xml
      <AdditionalOptions>%(AdditionalOptions) /bigobj /utf-8</AdditionalOptions>
      <AdditionalIncludeDirectories>$(MSBuildThisFileDirectory);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
```

In the `<ItemGroup>` that contains `<ClCompile Include="pch.cpp">`, add:

```xml
    <ClCompile Include="Core\Ucs2.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
```

In the `<ItemGroup>` that contains `<ClInclude Include="pch.h" />`, add:

```xml
    <ClInclude Include="Core\TextUtil.h" />
    <ClInclude Include="Core\Ucs2.h" />
```

In `WANUtility.slnx`, after the existing `<Project Path="WANUtility.vcxproj">…</Project>` block, add:

```xml
  <Project Path="Tests/WANUtility.Tests.vcxproj" />
```

- [ ] **Step 12: Build the app — verify Core compiles without the PCH**

Run the app build command (see "Build commands").
Expected: `WANUtility.vcxproj -> ...\x64\Debug\WANUtility\WANUtility.exe`, no `C1010` (missing pch) error.

- [ ] **Step 13: Commit**

```powershell
git add Core Tests scripts\test.ps1 WANUtility.vcxproj WANUtility.slnx
git commit -m "feat(core): UCS-2 decoding + CppUnitTest project`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Phone number → thread key

**Files:**
- Create: `Core/PhoneNumber.h`, `Core/PhoneNumber.cpp`, `Tests/PhoneNumberTests.cpp`
- Modify: `Tests/WANUtility.Tests.vcxproj`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `wan::core::TrimView` (Task 1).
- Produces: `std::wstring wan::core::ThreadKeyFor(std::wstring_view address)`; `bool wan::core::IsPhoneNumberKey(std::wstring_view key)`.

- [ ] **Step 1: Create `Core/PhoneNumber.h`**

```cpp
#pragma once
#include <string>
#include <string_view>

namespace wan::core
{
    // Thread key for an SMS address so that one contact maps to one thread.
    //  - Thai local numbers "0XXXXXXXX(X)" -> "+66XXXXXXXX(X)"
    //  - "66XXXXXXXXX" (11 digits)        -> "+66XXXXXXXXX"
    //  - "+..." numbers                    -> "+" + digits
    //  - spaces, dashes, dots, parentheses inside numbers are dropped
    //  - other numeric strings (short codes) -> digits as-is
    //  - alphanumeric senders ("dtac")     -> trimmed, verbatim
    std::wstring ThreadKeyFor(std::wstring_view address);

    // True when the key is a number we can send an SMS to ("+" optional, then digits only).
    bool IsPhoneNumberKey(std::wstring_view key);
}
```

- [ ] **Step 2: Create `Core/PhoneNumber.cpp` with stubs**

```cpp
#include "PhoneNumber.h"

namespace wan::core
{
    std::wstring ThreadKeyFor(std::wstring_view) { return {}; }
    bool IsPhoneNumberKey(std::wstring_view) { return false; }
}
```

- [ ] **Step 3: Write failing tests — `Tests/PhoneNumberTests.cpp`**

```cpp
#include "CppUnitTest.h"
#include "PhoneNumber.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

TEST_CLASS(PhoneNumberTests)
{
public:
    TEST_METHOD(ThaiLocalAndInternationalFormsShareAKey)
    {
        const std::wstring key = L"+66889605613";
        Assert::AreEqual(key, ThreadKeyFor(L"0889605613"));
        Assert::AreEqual(key, ThreadKeyFor(L"+66889605613"));
        Assert::AreEqual(key, ThreadKeyFor(L"66889605613"));
        Assert::AreEqual(key, ThreadKeyFor(L"088-960-5613"));
        Assert::AreEqual(key, ThreadKeyFor(L" 088 960 5613 "));
        Assert::AreEqual(key, ThreadKeyFor(L"+66 (88) 960.5613"));
    }

    TEST_METHOD(NineDigitLandlineGetsCountryCode)
    {
        Assert::AreEqual(std::wstring(L"+6621234567"), ThreadKeyFor(L"021234567"));
    }

    TEST_METHOD(AlphanumericSendersAreVerbatim)
    {
        Assert::AreEqual(std::wstring(L"dtac"), ThreadKeyFor(L"dtac"));
        Assert::AreEqual(std::wstring(L"dtacOnline"), ThreadKeyFor(L"  dtacOnline "));
    }

    TEST_METHOD(ShortCodesStayAsDigits)
    {
        Assert::AreEqual(std::wstring(L"1678"), ThreadKeyFor(L"1678"));
    }

    TEST_METHOD(BlankAddressGivesEmptyKey)
    {
        Assert::AreEqual(std::wstring(), ThreadKeyFor(L"   "));
    }

    TEST_METHOD(PhoneNumberKeyDetection)
    {
        Assert::IsTrue(IsPhoneNumberKey(L"+66889605613"));
        Assert::IsTrue(IsPhoneNumberKey(L"1678"));
        Assert::IsFalse(IsPhoneNumberKey(L"dtac"));
        Assert::IsFalse(IsPhoneNumberKey(L"+"));
        Assert::IsFalse(IsPhoneNumberKey(L""));
    }
};
```

Add to `Tests/WANUtility.Tests.vcxproj` inside the `<ItemGroup>` with the other `ClCompile` items:

```xml
    <ClCompile Include="..\Core\PhoneNumber.cpp" />
    <ClCompile Include="PhoneNumberTests.cpp" />
```

- [ ] **Step 4: Run tests — verify the new ones fail**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: 5 of the 6 `PhoneNumberTests` FAIL (`BlankAddressGivesEmptyKey` passes against the stub, which is expected); Task 1 tests still pass.

- [ ] **Step 5: Implement `Core/PhoneNumber.cpp`**

```cpp
#include "PhoneNumber.h"
#include "TextUtil.h"

namespace wan::core
{
    std::wstring ThreadKeyFor(std::wstring_view address)
    {
        const auto t = TrimView(address);
        std::wstring digits;
        bool plus = false;
        bool numeric = !t.empty();
        for (size_t i = 0; i < t.size() && numeric; ++i)
        {
            const wchar_t c = t[i];
            if (c >= L'0' && c <= L'9') digits.push_back(c);
            else if (c == L'+' && i == 0) plus = true;
            else if (c == L' ' || c == L'-' || c == L'(' || c == L')' || c == L'.') continue;
            else numeric = false;
        }
        if (!numeric || digits.empty())
        {
            return std::wstring(t);
        }
        if (plus)
        {
            return L"+" + digits;
        }
        if (digits.size() == 11 && digits.starts_with(L"66"))
        {
            return L"+" + digits;
        }
        if ((digits.size() == 9 || digits.size() == 10) && digits[0] == L'0')
        {
            return L"+66" + digits.substr(1);
        }
        return digits;
    }

    bool IsPhoneNumberKey(std::wstring_view key)
    {
        size_t i = (!key.empty() && key[0] == L'+') ? 1 : 0;
        if (i >= key.size())
        {
            return false;
        }
        for (; i < key.size(); ++i)
        {
            if (key[i] < L'0' || key[i] > L'9') return false;
        }
        return true;
    }
}
```

- [ ] **Step 6: Run tests — verify all pass**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: `Passed: 13` (7 + 6), 0 failed.

- [ ] **Step 7: Add to the app project and build**

In `WANUtility.vcxproj` add next to `Core\Ucs2.cpp`:

```xml
    <ClCompile Include="Core\PhoneNumber.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
```
and next to `Core\Ucs2.h`: `<ClInclude Include="Core\PhoneNumber.h" />`. Run the app build; expect success.

- [ ] **Step 8: Commit**

```powershell
git add Core Tests WANUtility.vcxproj
git commit -m "feat(core): normalize SMS addresses into thread keys`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: USSD menu parser

**Files:**
- Create: `Core/UssdMenuParser.h`, `Core/UssdMenuParser.cpp`, `Tests/UssdMenuParserTests.cpp`
- Modify: `Tests/WANUtility.Tests.vcxproj`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `TrimView` (Task 1).
- Produces: `struct wan::core::UssdOption { std::wstring key; std::wstring label; };` and `std::vector<UssdOption> wan::core::ParseUssdMenu(std::wstring_view text)`.

- [ ] **Step 1: Create `Core/UssdMenuParser.h`**

```cpp
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace wan::core
{
    struct UssdOption
    {
        std::wstring key;   // what to send back, e.g. L"1"
        std::wstring label; // text shown on the button
    };

    // Best-effort extraction of numbered menu options from a USSD reply.
    // A segment is an option when it starts with 1–2 ASCII digits followed by '.', ')', '-', ':'
    // or whitespace, then a non-empty label. Segments are lines, and parts of a line separated by
    // two or more spaces. A '.' directly followed by a digit is a decimal ("1.5GB"), not an option.
    // Fewer than two options means "not a menu": returns empty.
    std::vector<UssdOption> ParseUssdMenu(std::wstring_view text);
}
```

- [ ] **Step 2: Stub `Core/UssdMenuParser.cpp`**

```cpp
#include "UssdMenuParser.h"

namespace wan::core
{
    std::vector<UssdOption> ParseUssdMenu(std::wstring_view) { return {}; }
}
```

- [ ] **Step 3: Write failing tests — `Tests/UssdMenuParserTests.cpp`**

```cpp
#include "CppUnitTest.h"
#include "UssdMenuParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

TEST_CLASS(UssdMenuParserTests)
{
public:
    TEST_METHOD(ParsesNumberedMenuLines)
    {
        auto o = ParseUssdMenu(L"เลือกโปร\n1. เน็ตไม่อั้น 4Mbps 34บ./24ชม.\n2. เน็ต 3GB 7วัน\n0. ถัดไป");
        Assert::AreEqual(3, static_cast<int>(o.size()));
        Assert::AreEqual(std::wstring(L"1"), o[0].key);
        Assert::AreEqual(std::wstring(L"เน็ตไม่อั้น 4Mbps 34บ./24ชม."), o[0].label);
        Assert::AreEqual(std::wstring(L"2"), o[1].key);
        Assert::AreEqual(std::wstring(L"0"), o[2].key);
        Assert::AreEqual(std::wstring(L"ถัดไป"), o[2].label);
    }

    TEST_METHOD(ParsesTwoOptionsOnOneLine)
    {
        auto o = ParseUssdMenu(L"ยืนยันสมัคร?\n1. ยืนยัน  2. ยกเลิก");
        Assert::AreEqual(2, static_cast<int>(o.size()));
        Assert::AreEqual(std::wstring(L"ยืนยัน"), o[0].label);
        Assert::AreEqual(std::wstring(L"2"), o[1].key);
    }

    TEST_METHOD(ParsesParenthesisStyleWithCrLf)
    {
        auto o = ParseUssdMenu(L"Confirm?\r\n1) Yes\r\n2) No");
        Assert::AreEqual(2, static_cast<int>(o.size()));
        Assert::AreEqual(std::wstring(L"No"), o[1].label);
    }

    TEST_METHOD(RealBalanceReplyIsNotAMenu)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"ยอดคงเหลือ47.79บ.ใช้ได้ถึง12/10/70\nซื้อโปร โทร*103#").size()));
    }

    TEST_METHOD(RealProcessingReplyIsNotAMenu)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"ระบบกำลังดำเนินการ กรุณารอรับ SMS ยืนยันการทำรายการค่ะ").size()));
    }

    TEST_METHOD(SingleOptionIsNotAMenu)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"1. OK").size()));
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"10 บาท").size()));
    }

    TEST_METHOD(DecimalsAreNotOptions)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"1.5GB โปรเสริม\n2.5GB โปรเสริม").size()));
    }

    TEST_METHOD(ThreeDigitNumbersAreNotOptions)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"100 บาท\n200 บาท").size()));
    }

    TEST_METHOD(DatesAreNotOptions)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"12/10/70 หมดอายุ\n13/10/70 หมดอายุ").size()));
    }

    TEST_METHOD(EmptyTextGivesNoOptions)
    {
        Assert::AreEqual(0, static_cast<int>(ParseUssdMenu(L"").size()));
    }
};
```

Add to `Tests/WANUtility.Tests.vcxproj`:

```xml
    <ClCompile Include="..\Core\UssdMenuParser.cpp" />
    <ClCompile Include="UssdMenuParserTests.cpp" />
```

- [ ] **Step 4: Run tests — verify the first three fail**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: `ParsesNumberedMenuLines`, `ParsesTwoOptionsOnOneLine`, `ParsesParenthesisStyleWithCrLf` FAIL; the "is not a menu" tests pass against the stub.

- [ ] **Step 5: Implement `Core/UssdMenuParser.cpp`**

```cpp
#include "UssdMenuParser.h"
#include "TextUtil.h"

#include <optional>

namespace wan::core
{
    namespace
    {
        bool IsDigit(wchar_t c) { return c >= L'0' && c <= L'9'; }

        std::optional<UssdOption> ParseSegment(std::wstring_view s)
        {
            s = TrimView(s);
            size_t n = 0;
            while (n < s.size() && IsDigit(s[n])) ++n;
            if (n == 0 || n > 2 || n >= s.size())
            {
                return std::nullopt;
            }
            size_t i = n;
            const wchar_t sep = s[i];
            if (sep == L'.' || sep == L')' || sep == L'-' || sep == L':')
            {
                ++i;
                if (i < s.size() && IsDigit(s[i]))
                {
                    return std::nullopt; // "1.5GB" is a decimal, not option 1
                }
            }
            else if (sep != L' ' && sep != L'\t')
            {
                return std::nullopt;
            }
            const auto label = TrimView(s.substr(i));
            if (label.empty())
            {
                return std::nullopt;
            }
            return UssdOption{ std::wstring(s.substr(0, n)), std::wstring(label) };
        }

        // Splits a line on runs of two or more spaces ("1. ยืนยัน  2. ยกเลิก").
        std::vector<std::wstring_view> SplitSegments(std::wstring_view line)
        {
            std::vector<std::wstring_view> out;
            size_t start = 0;
            size_t i = 0;
            while (i < line.size())
            {
                if (line[i] == L' ' && i + 1 < line.size() && line[i + 1] == L' ')
                {
                    out.push_back(line.substr(start, i - start));
                    while (i < line.size() && line[i] == L' ') ++i;
                    start = i;
                }
                else
                {
                    ++i;
                }
            }
            out.push_back(line.substr(start));
            return out;
        }
    }

    std::vector<UssdOption> ParseUssdMenu(std::wstring_view text)
    {
        std::vector<UssdOption> options;
        size_t pos = 0;
        while (pos <= text.size())
        {
            const size_t end = text.find_first_of(L"\r\n", pos);
            const auto line = text.substr(pos, end == std::wstring_view::npos ? std::wstring_view::npos : end - pos);
            for (auto segment : SplitSegments(line))
            {
                if (auto option = ParseSegment(segment))
                {
                    options.push_back(*option);
                }
            }
            if (end == std::wstring_view::npos)
            {
                break;
            }
            pos = end + 1;
        }
        if (options.size() < 2)
        {
            options.clear();
        }
        return options;
    }
}
```

- [ ] **Step 6: Run tests — verify all pass**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: `Passed: 23`, 0 failed.

- [ ] **Step 7: Add to the app project and build**

`WANUtility.vcxproj`: add `<ClCompile Include="Core\UssdMenuParser.cpp"><PrecompiledHeader>NotUsing</PrecompiledHeader></ClCompile>` and `<ClInclude Include="Core\UssdMenuParser.h" />`. Build; expect success.

- [ ] **Step 8: Commit**

```powershell
git add Core Tests WANUtility.vcxproj
git commit -m "feat(core): parse numbered USSD menu options`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: MessageDb (SQLite)

**Files:**
- Create: `Core/MessageDb.h`, `Core/MessageDb.cpp`, `Tests/MessageDbTests.cpp`
- Modify: `Tests/WANUtility.Tests.vcxproj`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `ThreadKeyFor` (Task 2).
- Produces (exact):

```cpp
namespace wan::core {
enum class Direction : int { In = 0, Out = 1 };
enum class MessageStatus : int { Sending = 0, Sent = 1, Failed = 2, Received = 3 };
enum class SyncState : int { NotApplicable = 0, Pending = 1, Synced = 2, Disabled = 3 };
struct Message { int64_t id; std::wstring threadKey; Direction direction; std::wstring address; std::wstring body;
                 int64_t timestampMs; MessageStatus status; std::wstring errorDetail; std::wstring windowsStoreId;
                 SyncState syncState; bool isRead; };
struct Thread { std::wstring threadKey; std::wstring displayName; int64_t lastMessageAtMs; std::wstring lastBody; int unreadCount; };
struct UssdCode { int64_t id; std::wstring code; std::wstring label; int sortOrder; };
struct UssdHistoryEntry { int64_t id; std::wstring code; std::wstring finalReply; int64_t timestampMs; };
class DbError : public std::runtime_error;           // what() is UTF-8
class MessageDb {
  explicit MessageDb(std::wstring const& path);      // throws DbError
  bool HasWindowsStoreId(std::wstring const& id);
  int64_t InsertIncoming(std::wstring const& address, std::wstring const& body, int64_t timestampMs, std::wstring const& windowsStoreId); // 0 = duplicate
  int64_t InsertOutgoing(std::wstring const& address, std::wstring const& body, int64_t timestampMs);
  void MarkSending(int64_t id);
  void MarkSent(int64_t id);
  void MarkFailed(int64_t id, std::wstring const& detail);
  void MarkSynced(int64_t id, std::wstring const& windowsStoreId);
  void DisableSync(std::wstring const& reason);
  std::vector<Message> PendingSync();
  std::optional<Message> MessageById(int64_t id);
  std::vector<Thread> Threads(std::wstring const& search = L"");
  std::vector<Message> MessagesInThread(std::wstring const& threadKey);
  int MarkThreadRead(std::wstring const& threadKey); // rows changed
  std::vector<UssdCode> UssdCodes();
  int64_t AddUssdCode(std::wstring const& code, std::wstring const& label);
  void RenameUssdCode(int64_t id, std::wstring const& label);
  void RemoveUssdCode(int64_t id);
  void AddUssdHistory(std::wstring const& code, std::wstring const& finalReply, int64_t timestampMs);
  std::vector<UssdHistoryEntry> RecentUssd(int limit);
  std::optional<std::wstring> GetSetting(std::wstring const& key);
  void SetSetting(std::wstring const& key, std::wstring const& value);
};
}
```

Settings keys used across the app: `store_sync_enabled` (`"1"` default / `"0"`), `store_sync_disabled_reason`, `first_import_done` (`"1"` once).

- [ ] **Step 1: Create `Core/MessageDb.h`**

```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct sqlite3;

namespace wan::core
{
    enum class Direction : int { In = 0, Out = 1 };
    enum class MessageStatus : int { Sending = 0, Sent = 1, Failed = 2, Received = 3 };
    enum class SyncState : int { NotApplicable = 0, Pending = 1, Synced = 2, Disabled = 3 };

    struct Message
    {
        int64_t id = 0;
        std::wstring threadKey;
        Direction direction = Direction::In;
        std::wstring address;       // sender (in) or recipient (out), as given
        std::wstring body;
        int64_t timestampMs = 0;    // Unix epoch, milliseconds, UTC
        MessageStatus status = MessageStatus::Received;
        std::wstring errorDetail;
        std::wstring windowsStoreId;
        SyncState syncState = SyncState::NotApplicable;
        bool isRead = false;
    };

    struct Thread
    {
        std::wstring threadKey;
        std::wstring displayName;   // address of the newest message
        int64_t lastMessageAtMs = 0;
        std::wstring lastBody;
        int unreadCount = 0;
    };

    struct UssdCode
    {
        int64_t id = 0;
        std::wstring code;
        std::wstring label;
        int sortOrder = 0;
    };

    struct UssdHistoryEntry
    {
        int64_t id = 0;
        std::wstring code;
        std::wstring finalReply;
        int64_t timestampMs = 0;
    };

    class DbError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // Single-connection SQLite store. Not thread-safe: use from the UI thread only.
    class MessageDb
    {
    public:
        explicit MessageDb(std::wstring const& path);
        ~MessageDb();
        MessageDb(MessageDb const&) = delete;
        MessageDb& operator=(MessageDb const&) = delete;

        bool HasWindowsStoreId(std::wstring const& id);
        int64_t InsertIncoming(std::wstring const& address, std::wstring const& body, int64_t timestampMs, std::wstring const& windowsStoreId);
        int64_t InsertOutgoing(std::wstring const& address, std::wstring const& body, int64_t timestampMs);
        void MarkSending(int64_t id);
        void MarkSent(int64_t id);
        void MarkFailed(int64_t id, std::wstring const& detail);
        void MarkSynced(int64_t id, std::wstring const& windowsStoreId);
        void DisableSync(std::wstring const& reason);
        std::vector<Message> PendingSync();
        std::optional<Message> MessageById(int64_t id);

        std::vector<Thread> Threads(std::wstring const& search = L"");
        std::vector<Message> MessagesInThread(std::wstring const& threadKey);
        int MarkThreadRead(std::wstring const& threadKey);

        std::vector<UssdCode> UssdCodes();
        int64_t AddUssdCode(std::wstring const& code, std::wstring const& label);
        void RenameUssdCode(int64_t id, std::wstring const& label);
        void RemoveUssdCode(int64_t id);
        void AddUssdHistory(std::wstring const& code, std::wstring const& finalReply, int64_t timestampMs);
        std::vector<UssdHistoryEntry> RecentUssd(int limit);

        std::optional<std::wstring> GetSetting(std::wstring const& key);
        void SetSetting(std::wstring const& key, std::wstring const& value);

    private:
        sqlite3* m_db = nullptr;
    };
}
```

- [ ] **Step 2: Stub `Core/MessageDb.cpp`** (every method throws so all tests fail)

```cpp
#include "MessageDb.h"

namespace wan::core
{
    namespace { [[noreturn]] void NotYet() { throw DbError("not implemented"); } }

    MessageDb::MessageDb(std::wstring const&) { NotYet(); }
    MessageDb::~MessageDb() = default;
    bool MessageDb::HasWindowsStoreId(std::wstring const&) { NotYet(); }
    int64_t MessageDb::InsertIncoming(std::wstring const&, std::wstring const&, int64_t, std::wstring const&) { NotYet(); }
    int64_t MessageDb::InsertOutgoing(std::wstring const&, std::wstring const&, int64_t) { NotYet(); }
    void MessageDb::MarkSending(int64_t) { NotYet(); }
    void MessageDb::MarkSent(int64_t) { NotYet(); }
    void MessageDb::MarkFailed(int64_t, std::wstring const&) { NotYet(); }
    void MessageDb::MarkSynced(int64_t, std::wstring const&) { NotYet(); }
    void MessageDb::DisableSync(std::wstring const&) { NotYet(); }
    std::vector<Message> MessageDb::PendingSync() { NotYet(); }
    std::optional<Message> MessageDb::MessageById(int64_t) { NotYet(); }
    std::vector<Thread> MessageDb::Threads(std::wstring const&) { NotYet(); }
    std::vector<Message> MessageDb::MessagesInThread(std::wstring const&) { NotYet(); }
    int MessageDb::MarkThreadRead(std::wstring const&) { NotYet(); }
    std::vector<UssdCode> MessageDb::UssdCodes() { NotYet(); }
    int64_t MessageDb::AddUssdCode(std::wstring const&, std::wstring const&) { NotYet(); }
    void MessageDb::RenameUssdCode(int64_t, std::wstring const&) { NotYet(); }
    void MessageDb::RemoveUssdCode(int64_t) { NotYet(); }
    void MessageDb::AddUssdHistory(std::wstring const&, std::wstring const&, int64_t) { NotYet(); }
    std::vector<UssdHistoryEntry> MessageDb::RecentUssd(int) { NotYet(); }
    std::optional<std::wstring> MessageDb::GetSetting(std::wstring const&) { NotYet(); }
    void MessageDb::SetSetting(std::wstring const&, std::wstring const&) { NotYet(); }
}
```

- [ ] **Step 3: Write failing tests — `Tests/MessageDbTests.cpp`**

```cpp
#include "CppUnitTest.h"
#include "MessageDb.h"

#include <filesystem>
#include <windows.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

TEST_CLASS(MessageDbTests)
{
    std::filesystem::path m_path;

    static std::filesystem::path NewTempPath()
    {
        static int counter = 0;
        return std::filesystem::temp_directory_path() /
            (L"wanutility_test_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(++counter) + L".db");
    }

public:
    TEST_METHOD_INITIALIZE(Init)
    {
        m_path = NewTempPath();
        std::filesystem::remove(m_path);
    }

    TEST_METHOD_CLEANUP(Cleanup)
    {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }

    TEST_METHOD(NewDatabaseSeedsThreeUssdCodesOnce)
    {
        {
            MessageDb db(m_path.wstring());
            auto codes = db.UssdCodes();
            Assert::AreEqual(3, static_cast<int>(codes.size()));
            Assert::AreEqual(std::wstring(L"*101#"), codes[0].code);
            Assert::AreEqual(std::wstring(L"Balance"), codes[0].label);
            Assert::AreEqual(std::wstring(L"*103#"), codes[1].code);
            Assert::AreEqual(std::wstring(L"*101*1#"), codes[2].code);
        }
        MessageDb reopened(m_path.wstring());
        Assert::AreEqual(3, static_cast<int>(reopened.UssdCodes().size()));
    }

    TEST_METHOD(DuplicateWindowsStoreIdIsIgnored)
    {
        MessageDb db(m_path.wstring());
        auto first = db.InsertIncoming(L"dtac", L"hello", 1000, L"store-1");
        auto second = db.InsertIncoming(L"dtac", L"hello", 1000, L"store-1");
        Assert::IsTrue(first > 0);
        Assert::IsTrue(second == 0);
        Assert::IsTrue(db.HasWindowsStoreId(L"store-1"));
        Assert::IsFalse(db.HasWindowsStoreId(L"store-2"));
        Assert::AreEqual(1, static_cast<int>(db.MessagesInThread(L"dtac").size()));
    }

    TEST_METHOD(LocalAndInternationalNumbersShareOneThread)
    {
        MessageDb db(m_path.wstring());
        db.InsertIncoming(L"+66889605613", L"hi", 1000, L"s1");
        db.InsertOutgoing(L"0889605613", L"hello back", 2000);
        auto threads = db.Threads();
        Assert::AreEqual(1, static_cast<int>(threads.size()));
        Assert::AreEqual(std::wstring(L"+66889605613"), threads[0].threadKey);
        Assert::AreEqual(std::wstring(L"0889605613"), threads[0].displayName); // newest message's address
        Assert::AreEqual(std::wstring(L"hello back"), threads[0].lastBody);
        Assert::AreEqual(2, static_cast<int>(db.MessagesInThread(L"+66889605613").size()));
    }

    TEST_METHOD(ThreadsAreNewestFirstWithUnreadCounts)
    {
        MessageDb db(m_path.wstring());
        db.InsertIncoming(L"dtac", L"old", 1000, L"a");
        db.InsertIncoming(L"dtacOnline", L"newer", 3000, L"b");
        db.InsertIncoming(L"dtac", L"newest dtac", 2000, L"c");
        auto threads = db.Threads();
        Assert::AreEqual(2, static_cast<int>(threads.size()));
        Assert::AreEqual(std::wstring(L"dtacOnline"), threads[0].threadKey);
        Assert::AreEqual(std::wstring(L"dtac"), threads[1].threadKey);
        Assert::AreEqual(2, threads[1].unreadCount);
        Assert::AreEqual(2, db.MarkThreadRead(L"dtac"));
        Assert::AreEqual(0, db.Threads()[1].unreadCount);
        Assert::AreEqual(0, db.MarkThreadRead(L"dtac"));
    }

    TEST_METHOD(MessagesInThreadAreOldestFirst)
    {
        MessageDb db(m_path.wstring());
        db.InsertIncoming(L"dtac", L"second", 2000, L"b");
        db.InsertIncoming(L"dtac", L"first", 1000, L"a");
        auto m = db.MessagesInThread(L"dtac");
        Assert::AreEqual(std::wstring(L"first"), m[0].body);
        Assert::AreEqual(std::wstring(L"second"), m[1].body);
    }

    TEST_METHOD(SearchMatchesBodyOrAddress)
    {
        MessageDb db(m_path.wstring());
        db.InsertIncoming(L"dtac", L"ยอดคงเหลือ 47.79", 1000, L"a");
        db.InsertIncoming(L"dtacOnline", L"promo", 2000, L"b");
        Assert::AreEqual(1, static_cast<int>(db.Threads(L"47.79").size()));
        Assert::AreEqual(2, static_cast<int>(db.Threads(L"dtac").size()));
        Assert::AreEqual(0, static_cast<int>(db.Threads(L"100%").size())); // % is literal, not a wildcard
    }

    TEST_METHOD(OutgoingLifecycleAndSync)
    {
        MessageDb db(m_path.wstring());
        auto id = db.InsertOutgoing(L"0889605613", L"test", 1000);
        Assert::IsTrue(db.MessageById(id)->status == MessageStatus::Sending);
        Assert::IsTrue(db.MessageById(id)->syncState == SyncState::NotApplicable);
        Assert::IsTrue(db.MessageById(id)->isRead);

        db.MarkFailed(id, L"Modem error 5");
        Assert::IsTrue(db.MessageById(id)->status == MessageStatus::Failed);
        Assert::AreEqual(std::wstring(L"Modem error 5"), db.MessageById(id)->errorDetail);
        Assert::AreEqual(0, static_cast<int>(db.PendingSync().size()));

        db.MarkSending(id);
        db.MarkSent(id);
        auto sent = *db.MessageById(id);
        Assert::IsTrue(sent.status == MessageStatus::Sent);
        Assert::AreEqual(std::wstring(), sent.errorDetail);
        Assert::IsTrue(sent.syncState == SyncState::Pending);
        Assert::AreEqual(1, static_cast<int>(db.PendingSync().size()));

        db.MarkSynced(id, L"store-out-1");
        Assert::IsTrue(db.MessageById(id)->syncState == SyncState::Synced);
        Assert::IsTrue(db.HasWindowsStoreId(L"store-out-1"));
        Assert::AreEqual(0, static_cast<int>(db.PendingSync().size()));
    }

    TEST_METHOD(DisableSyncStopsPendingAndFutureSync)
    {
        MessageDb db(m_path.wstring());
        auto a = db.InsertOutgoing(L"0889605613", L"a", 1000);
        db.MarkSent(a);
        db.DisableSync(L"Access denied");
        Assert::IsTrue(db.MessageById(a)->syncState == SyncState::Disabled);
        Assert::AreEqual(std::wstring(L"0"), *db.GetSetting(L"store_sync_enabled"));
        Assert::AreEqual(std::wstring(L"Access denied"), *db.GetSetting(L"store_sync_disabled_reason"));

        auto b = db.InsertOutgoing(L"0889605613", L"b", 2000);
        db.MarkSent(b);
        Assert::IsTrue(db.MessageById(b)->syncState == SyncState::Disabled);
        Assert::AreEqual(0, static_cast<int>(db.PendingSync().size()));
    }

    TEST_METHOD(MarkSyncedWithEmptyStoreIdKeepsColumnNull)
    {
        MessageDb db(m_path.wstring());
        auto a = db.InsertOutgoing(L"1678", L"a", 1000);
        auto b = db.InsertOutgoing(L"1678", L"b", 2000);
        db.MarkSent(a);
        db.MarkSent(b);
        db.MarkSynced(a, L"");
        db.MarkSynced(b, L""); // must not violate UNIQUE
        Assert::IsTrue(db.MessageById(b)->syncState == SyncState::Synced);
    }

    TEST_METHOD(UnicodeAndEmptyBodiesRoundTrip)
    {
        MessageDb db(m_path.wstring());
        auto id = db.InsertIncoming(L"dtac", L"", 1000, L"x");
        Assert::AreEqual(std::wstring(), db.MessageById(id)->body);
        auto id2 = db.InsertIncoming(L"dtac", L"คุณมีโปรโมชั่น 3GB", 2000, L"y");
        Assert::AreEqual(std::wstring(L"คุณมีโปรโมชั่น 3GB"), db.MessageById(id2)->body);
        Assert::IsFalse(db.MessageById(999).has_value());
    }

    TEST_METHOD(UssdCodesCanBeAddedRenamedRemoved)
    {
        MessageDb db(m_path.wstring());
        auto id = db.AddUssdCode(L"*121#", L"Promo");
        auto codes = db.UssdCodes();
        Assert::AreEqual(4, static_cast<int>(codes.size()));
        Assert::AreEqual(std::wstring(L"*121#"), codes[3].code);
        db.RenameUssdCode(id, L"Promotions");
        Assert::AreEqual(std::wstring(L"Promotions"), db.UssdCodes()[3].label);
        db.RemoveUssdCode(id);
        Assert::AreEqual(3, static_cast<int>(db.UssdCodes().size()));
    }

    TEST_METHOD(RecentUssdIsNewestFirstAndLimited)
    {
        MessageDb db(m_path.wstring());
        db.AddUssdHistory(L"*101#", L"one", 1000);
        db.AddUssdHistory(L"*101#", L"two", 2000);
        db.AddUssdHistory(L"*103#", L"three", 3000);
        auto recent = db.RecentUssd(2);
        Assert::AreEqual(2, static_cast<int>(recent.size()));
        Assert::AreEqual(std::wstring(L"three"), recent[0].finalReply);
        Assert::AreEqual(std::wstring(L"two"), recent[1].finalReply);
    }

    TEST_METHOD(SettingsOverwrite)
    {
        MessageDb db(m_path.wstring());
        Assert::IsFalse(db.GetSetting(L"first_import_done").has_value());
        db.SetSetting(L"first_import_done", L"1");
        db.SetSetting(L"first_import_done", L"2");
        Assert::AreEqual(std::wstring(L"2"), *db.GetSetting(L"first_import_done"));
    }

    TEST_METHOD(OpeningAnInvalidPathThrowsDbError)
    {
        auto bad = std::filesystem::temp_directory_path() / L"no_such_dir_wanutility" / L"x" / L"y.db";
        Assert::ExpectException<DbError>([&] { MessageDb db(bad.wstring()); });
    }
};
```

Add to `Tests/WANUtility.Tests.vcxproj`:

```xml
    <ClCompile Include="..\Core\MessageDb.cpp" />
    <ClCompile Include="MessageDbTests.cpp" />
```
(`..\Core\PhoneNumber.cpp`, which `MessageDb.cpp` calls, is already in the project since Task 2.)

- [ ] **Step 4: Run tests — verify the MessageDb tests fail**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: 13 of the 14 `MessageDbTests` FAIL; `OpeningAnInvalidPathThrowsDbError` passes (the stub throws `DbError`). Earlier tests pass.

- [ ] **Step 5: Implement `Core/MessageDb.cpp`**

```cpp
#include "MessageDb.h"
#include "PhoneNumber.h"

#include <winsqlite/winsqlite3.h>

namespace wan::core
{
    namespace
    {
        constexpr const char* kSchema = R"SQL(
            CREATE TABLE IF NOT EXISTS messages(
                id               INTEGER PRIMARY KEY AUTOINCREMENT,
                thread_key       TEXT    NOT NULL,
                direction        INTEGER NOT NULL,
                address          TEXT    NOT NULL,
                body             TEXT    NOT NULL,
                timestamp_ms     INTEGER NOT NULL,
                status           INTEGER NOT NULL,
                error_detail     TEXT    NOT NULL DEFAULT '',
                windows_store_id TEXT    UNIQUE,
                sync_state       INTEGER NOT NULL,
                is_read          INTEGER NOT NULL);
            CREATE INDEX IF NOT EXISTS ix_messages_thread ON messages(thread_key, timestamp_ms);
            CREATE TABLE IF NOT EXISTS ussd_codes(
                id INTEGER PRIMARY KEY AUTOINCREMENT, code TEXT NOT NULL, label TEXT NOT NULL, sort_order INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS ussd_history(
                id INTEGER PRIMARY KEY AUTOINCREMENT, code TEXT NOT NULL, final_reply TEXT NOT NULL, timestamp_ms INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT NOT NULL);
        )SQL";

        constexpr const wchar_t* kMessageColumns =
            L"id, thread_key, direction, address, body, timestamp_ms, status, error_detail, "
            L"COALESCE(windows_store_id, ''), sync_state, is_read";

        // RAII prepared statement using the UTF-16 sqlite API (wchar_t is UTF-16 on Windows).
        class Stmt
        {
        public:
            Stmt(sqlite3* db, std::wstring const& sql) : m_db(db)
            {
                if (sqlite3_prepare16_v2(db, sql.c_str(), -1, &m_stmt, nullptr) != SQLITE_OK)
                {
                    throw DbError(std::string("prepare: ") + sqlite3_errmsg(db));
                }
            }
            ~Stmt() { sqlite3_finalize(m_stmt); }
            Stmt(Stmt const&) = delete;
            Stmt& operator=(Stmt const&) = delete;

            Stmt& Bind(int index, int64_t value) { Check(sqlite3_bind_int64(m_stmt, index, value)); return *this; }
            Stmt& Bind(int index, std::wstring const& value)
            {
                // c_str() is never null, so an empty string binds '' (not NULL).
                Check(sqlite3_bind_text16(m_stmt, index, value.c_str(), static_cast<int>(value.size() * sizeof(wchar_t)), SQLITE_TRANSIENT));
                return *this;
            }
            Stmt& BindNullable(int index, std::wstring const& value)
            {
                if (value.empty()) { Check(sqlite3_bind_null(m_stmt, index)); return *this; }
                return Bind(index, value);
            }

            bool Step()
            {
                const int rc = sqlite3_step(m_stmt);
                if (rc == SQLITE_ROW) return true;
                if (rc == SQLITE_DONE) return false;
                throw DbError(std::string("step: ") + sqlite3_errmsg(m_db));
            }
            void Run() { while (Step()) {} }

            int64_t Int(int col) { return sqlite3_column_int64(m_stmt, col); }
            std::wstring Text(int col)
            {
                auto p = static_cast<const wchar_t*>(sqlite3_column_text16(m_stmt, col));
                if (!p) return {};
                return std::wstring(p, sqlite3_column_bytes16(m_stmt, col) / sizeof(wchar_t));
            }

        private:
            void Check(int rc)
            {
                if (rc != SQLITE_OK) throw DbError(std::string("bind: ") + sqlite3_errmsg(m_db));
            }
            sqlite3* m_db;
            sqlite3_stmt* m_stmt = nullptr;
        };

        void Exec(sqlite3* db, const char* sql)
        {
            char* err = nullptr;
            if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK)
            {
                std::string message = err ? err : "unknown error";
                sqlite3_free(err);
                throw DbError("exec: " + message);
            }
        }

        Message ReadMessage(Stmt& s)
        {
            Message m;
            m.id = s.Int(0);
            m.threadKey = s.Text(1);
            m.direction = static_cast<Direction>(s.Int(2));
            m.address = s.Text(3);
            m.body = s.Text(4);
            m.timestampMs = s.Int(5);
            m.status = static_cast<MessageStatus>(s.Int(6));
            m.errorDetail = s.Text(7);
            m.windowsStoreId = s.Text(8);
            m.syncState = static_cast<SyncState>(s.Int(9));
            m.isRead = s.Int(10) != 0;
            return m;
        }

        std::wstring LikePattern(std::wstring const& search)
        {
            std::wstring p = L"%";
            for (wchar_t c : search)
            {
                if (c == L'%' || c == L'_' || c == L'\\') p.push_back(L'\\');
                p.push_back(c);
            }
            p.push_back(L'%');
            return p;
        }

        int64_t I(auto e) { return static_cast<int64_t>(e); }
    }

    MessageDb::MessageDb(std::wstring const& path)
    {
        if (sqlite3_open16(path.c_str(), &m_db) != SQLITE_OK)
        {
            std::string message = m_db ? sqlite3_errmsg(m_db) : "out of memory";
            sqlite3_close(m_db);
            m_db = nullptr;
            throw DbError("open: " + message);
        }
        try
        {
            Exec(m_db, kSchema);
            int64_t version = 0;
            {
                Stmt v(m_db, L"PRAGMA user_version;");
                if (v.Step()) version = v.Int(0);
            }
            if (version == 0)
            {
                AddUssdCode(L"*101#", L"Balance");
                AddUssdCode(L"*103#", L"Packages");
                AddUssdCode(L"*101*1#", L"Status SMS");
                Exec(m_db, "PRAGMA user_version = 1;");
            }
        }
        catch (...)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
            throw;
        }
    }

    MessageDb::~MessageDb()
    {
        sqlite3_close(m_db);
    }

    bool MessageDb::HasWindowsStoreId(std::wstring const& id)
    {
        Stmt s(m_db, L"SELECT 1 FROM messages WHERE windows_store_id = ?1;");
        s.Bind(1, id);
        return s.Step();
    }

    int64_t MessageDb::InsertIncoming(std::wstring const& address, std::wstring const& body, int64_t timestampMs, std::wstring const& windowsStoreId)
    {
        Stmt s(m_db, L"INSERT OR IGNORE INTO messages(thread_key, direction, address, body, timestamp_ms, status, windows_store_id, sync_state, is_read) "
                     L"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 0);");
        s.Bind(1, ThreadKeyFor(address)).Bind(2, I(Direction::In)).Bind(3, address).Bind(4, body).Bind(5, timestampMs)
         .Bind(6, I(MessageStatus::Received)).BindNullable(7, windowsStoreId).Bind(8, I(SyncState::NotApplicable));
        s.Run();
        return sqlite3_changes(m_db) == 0 ? 0 : sqlite3_last_insert_rowid(m_db);
    }

    int64_t MessageDb::InsertOutgoing(std::wstring const& address, std::wstring const& body, int64_t timestampMs)
    {
        Stmt s(m_db, L"INSERT INTO messages(thread_key, direction, address, body, timestamp_ms, status, sync_state, is_read) "
                     L"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, 1);");
        s.Bind(1, ThreadKeyFor(address)).Bind(2, I(Direction::Out)).Bind(3, address).Bind(4, body).Bind(5, timestampMs)
         .Bind(6, I(MessageStatus::Sending)).Bind(7, I(SyncState::NotApplicable));
        s.Run();
        return sqlite3_last_insert_rowid(m_db);
    }

    void MessageDb::MarkSending(int64_t id)
    {
        Stmt s(m_db, L"UPDATE messages SET status = ?1, error_detail = '' WHERE id = ?2;");
        s.Bind(1, I(MessageStatus::Sending)).Bind(2, id).Run();
    }

    void MessageDb::MarkSent(int64_t id)
    {
        const bool syncOn = GetSetting(L"store_sync_enabled").value_or(L"1") != L"0";
        Stmt s(m_db, L"UPDATE messages SET status = ?1, error_detail = '', sync_state = ?2 WHERE id = ?3;");
        s.Bind(1, I(MessageStatus::Sent)).Bind(2, I(syncOn ? SyncState::Pending : SyncState::Disabled)).Bind(3, id).Run();
    }

    void MessageDb::MarkFailed(int64_t id, std::wstring const& detail)
    {
        Stmt s(m_db, L"UPDATE messages SET status = ?1, error_detail = ?2 WHERE id = ?3;");
        s.Bind(1, I(MessageStatus::Failed)).Bind(2, detail).Bind(3, id).Run();
    }

    void MessageDb::MarkSynced(int64_t id, std::wstring const& windowsStoreId)
    {
        Stmt s(m_db, L"UPDATE messages SET sync_state = ?1, windows_store_id = ?2 WHERE id = ?3;");
        s.Bind(1, I(SyncState::Synced)).BindNullable(2, windowsStoreId).Bind(3, id).Run();
    }

    void MessageDb::DisableSync(std::wstring const& reason)
    {
        SetSetting(L"store_sync_enabled", L"0");
        SetSetting(L"store_sync_disabled_reason", reason);
        Stmt s(m_db, L"UPDATE messages SET sync_state = ?1 WHERE sync_state = ?2;");
        s.Bind(1, I(SyncState::Disabled)).Bind(2, I(SyncState::Pending)).Run();
    }

    std::vector<Message> MessageDb::PendingSync()
    {
        Stmt s(m_db, std::wstring(L"SELECT ") + kMessageColumns + L" FROM messages WHERE sync_state = ?1 AND status = ?2 ORDER BY id;");
        s.Bind(1, I(SyncState::Pending)).Bind(2, I(MessageStatus::Sent));
        std::vector<Message> out;
        while (s.Step()) out.push_back(ReadMessage(s));
        return out;
    }

    std::optional<Message> MessageDb::MessageById(int64_t id)
    {
        Stmt s(m_db, std::wstring(L"SELECT ") + kMessageColumns + L" FROM messages WHERE id = ?1;");
        s.Bind(1, id);
        if (!s.Step()) return std::nullopt;
        return ReadMessage(s);
    }

    std::vector<Thread> MessageDb::Threads(std::wstring const& search)
    {
        Stmt s(m_db, LR"SQL(
            SELECT m.thread_key, m.address, m.timestamp_ms, m.body,
                   (SELECT COUNT(*) FROM messages u WHERE u.thread_key = m.thread_key AND u.direction = 0 AND u.is_read = 0)
            FROM messages m
            WHERE m.id = (SELECT x.id FROM messages x WHERE x.thread_key = m.thread_key ORDER BY x.timestamp_ms DESC, x.id DESC LIMIT 1)
              AND (?1 = '' OR m.thread_key IN (
                    SELECT f.thread_key FROM messages f
                    WHERE f.body LIKE ?2 ESCAPE '\' OR f.address LIKE ?2 ESCAPE '\' OR f.thread_key LIKE ?2 ESCAPE '\'))
            ORDER BY m.timestamp_ms DESC, m.id DESC;)SQL");
        s.Bind(1, search).Bind(2, LikePattern(search));
        std::vector<Thread> out;
        while (s.Step())
        {
            out.push_back(Thread{ s.Text(0), s.Text(1), s.Int(2), s.Text(3), static_cast<int>(s.Int(4)) });
        }
        return out;
    }

    std::vector<Message> MessageDb::MessagesInThread(std::wstring const& threadKey)
    {
        Stmt s(m_db, std::wstring(L"SELECT ") + kMessageColumns + L" FROM messages WHERE thread_key = ?1 ORDER BY timestamp_ms, id;");
        s.Bind(1, threadKey);
        std::vector<Message> out;
        while (s.Step()) out.push_back(ReadMessage(s));
        return out;
    }

    int MessageDb::MarkThreadRead(std::wstring const& threadKey)
    {
        Stmt s(m_db, L"UPDATE messages SET is_read = 1 WHERE thread_key = ?1 AND is_read = 0;");
        s.Bind(1, threadKey).Run();
        return sqlite3_changes(m_db);
    }

    std::vector<UssdCode> MessageDb::UssdCodes()
    {
        Stmt s(m_db, L"SELECT id, code, label, sort_order FROM ussd_codes ORDER BY sort_order, id;");
        std::vector<UssdCode> out;
        while (s.Step()) out.push_back(UssdCode{ s.Int(0), s.Text(1), s.Text(2), static_cast<int>(s.Int(3)) });
        return out;
    }

    int64_t MessageDb::AddUssdCode(std::wstring const& code, std::wstring const& label)
    {
        Stmt s(m_db, L"INSERT INTO ussd_codes(code, label, sort_order) VALUES(?1, ?2, (SELECT COALESCE(MAX(sort_order), 0) + 1 FROM ussd_codes));");
        s.Bind(1, code).Bind(2, label).Run();
        return sqlite3_last_insert_rowid(m_db);
    }

    void MessageDb::RenameUssdCode(int64_t id, std::wstring const& label)
    {
        Stmt s(m_db, L"UPDATE ussd_codes SET label = ?1 WHERE id = ?2;");
        s.Bind(1, label).Bind(2, id).Run();
    }

    void MessageDb::RemoveUssdCode(int64_t id)
    {
        Stmt s(m_db, L"DELETE FROM ussd_codes WHERE id = ?1;");
        s.Bind(1, id).Run();
    }

    void MessageDb::AddUssdHistory(std::wstring const& code, std::wstring const& finalReply, int64_t timestampMs)
    {
        Stmt s(m_db, L"INSERT INTO ussd_history(code, final_reply, timestamp_ms) VALUES(?1, ?2, ?3);");
        s.Bind(1, code).Bind(2, finalReply).Bind(3, timestampMs).Run();
    }

    std::vector<UssdHistoryEntry> MessageDb::RecentUssd(int limit)
    {
        Stmt s(m_db, L"SELECT id, code, final_reply, timestamp_ms FROM ussd_history ORDER BY timestamp_ms DESC, id DESC LIMIT ?1;");
        s.Bind(1, static_cast<int64_t>(limit));
        std::vector<UssdHistoryEntry> out;
        while (s.Step()) out.push_back(UssdHistoryEntry{ s.Int(0), s.Text(1), s.Text(2), s.Int(3) });
        return out;
    }

    std::optional<std::wstring> MessageDb::GetSetting(std::wstring const& key)
    {
        Stmt s(m_db, L"SELECT value FROM settings WHERE key = ?1;");
        s.Bind(1, key);
        if (!s.Step()) return std::nullopt;
        return s.Text(0);
    }

    void MessageDb::SetSetting(std::wstring const& key, std::wstring const& value)
    {
        Stmt s(m_db, L"INSERT INTO settings(key, value) VALUES(?1, ?2) ON CONFLICT(key) DO UPDATE SET value = excluded.value;");
        s.Bind(1, key).Bind(2, value).Run();
    }
}
```

- [ ] **Step 6: Run tests — verify all pass**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: `Passed: 37` (23 + 14), 0 failed. If `sqlite3_*` link errors appear, confirm `winsqlite3.lib` is in the test project's `AdditionalDependencies`.

- [ ] **Step 7: Add to the app project and build**

`WANUtility.vcxproj`:
- add `<ClCompile Include="Core\MessageDb.cpp"><PrecompiledHeader>NotUsing</PrecompiledHeader></ClCompile>` and `<ClInclude Include="Core\MessageDb.h" />`;
- in the first `<ItemDefinitionGroup>`, after `</ClCompile>`, add:

```xml
    <Link>
      <AdditionalDependencies>winsqlite3.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
```
Build; expect success.

- [ ] **Step 8: Commit**

```powershell
git add Core Tests WANUtility.vcxproj
git commit -m "feat(core): SQLite message store with threads, sync state, USSD codes`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: UTF-8 rotating log

**Files:**
- Create: `Core/Log.h`, `Core/Log.cpp`, `Tests/LogTests.cpp`
- Modify: `Tests/WANUtility.Tests.vcxproj`, `WANUtility.vcxproj`

**Interfaces:**
- Produces: `static void wan::core::Log::Init(std::filesystem::path const& file, std::uintmax_t maxBytes = 1'000'000)`; `static void wan::core::Log::Write(std::wstring_view line)`; `static std::string wan::core::Log::ToUtf8(std::wstring_view)`. Thread-safe.

- [ ] **Step 1: Create `Core/Log.h`**

```cpp
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace wan::core
{
    // Process-wide UTF-8 log. When the file exceeds maxBytes it is renamed to "<file>.1"
    // (replacing any previous one) and a new file is started. Safe to call from any thread.
    class Log
    {
    public:
        static void Init(std::filesystem::path const& file, std::uintmax_t maxBytes = 1'000'000);
        static void Write(std::wstring_view line);
        static std::string ToUtf8(std::wstring_view text);
    };
}
```

- [ ] **Step 2: Stub `Core/Log.cpp`**

```cpp
#include "Log.h"

namespace wan::core
{
    void Log::Init(std::filesystem::path const&, std::uintmax_t) {}
    void Log::Write(std::wstring_view) {}
    std::string Log::ToUtf8(std::wstring_view) { return {}; }
}
```

- [ ] **Step 3: Write failing tests — `Tests/LogTests.cpp`**

```cpp
#include "CppUnitTest.h"
#include "Log.h"

#include <fstream>
#include <sstream>
#include <windows.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

namespace
{
    std::string ReadAll(std::filesystem::path const& p)
    {
        std::ifstream f(p, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
}

TEST_CLASS(LogTests)
{
    std::filesystem::path m_dir;

public:
    TEST_METHOD_INITIALIZE(Init)
    {
        m_dir = std::filesystem::temp_directory_path() / (L"wanutility_log_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()));
        std::filesystem::remove_all(m_dir);
    }

    TEST_METHOD_CLEANUP(Cleanup)
    {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
    }

    TEST_METHOD(ToUtf8EncodesThai)
    {
        // U+0E22 THAI CHARACTER YO YAK = E0 B8 A2
        Assert::AreEqual(std::string("\xE0\xB8\xA2" "47"), Log::ToUtf8(L"ย47"));
    }

    TEST_METHOD(WritesThaiAsUtf8AndCreatesDirectories)
    {
        auto file = m_dir / L"logs" / L"test.log";
        Log::Init(file);
        Log::Write(L"ยอดคงเหลือ47.79บ.");
        Log::Write(L"second line");
        auto text = ReadAll(file);
        Assert::IsTrue(text.find(Log::ToUtf8(L"ยอดคงเหลือ47.79บ.")) != std::string::npos);
        Assert::IsTrue(text.find("second line\n") != std::string::npos); // line after Thai is not lost
    }

    TEST_METHOD(RotatesWhenTooLarge)
    {
        auto file = m_dir / L"r.log";
        Log::Init(file, 200);
        for (int i = 0; i < 20; ++i) Log::Write(L"0123456789012345678901234567890123456789");
        Assert::IsTrue(std::filesystem::exists(file.wstring() + L".1"));
        Assert::IsTrue(std::filesystem::file_size(file) < 400);
    }
};
```

Add to `Tests/WANUtility.Tests.vcxproj`:

```xml
    <ClCompile Include="..\Core\Log.cpp" />
    <ClCompile Include="LogTests.cpp" />
```

- [ ] **Step 4: Run tests — verify the 3 Log tests fail**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: 3 `LogTests` FAIL, 37 earlier tests pass.

- [ ] **Step 5: Implement `Core/Log.cpp`**

```cpp
#include "Log.h"

#include <fstream>
#include <mutex>
#include <windows.h>

namespace wan::core
{
    namespace
    {
        std::mutex g_mutex;
        std::filesystem::path g_file;
        std::uintmax_t g_maxBytes = 1'000'000;
    }

    std::string Log::ToUtf8(std::wstring_view text)
    {
        if (text.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
        return out;
    }

    void Log::Init(std::filesystem::path const& file, std::uintmax_t maxBytes)
    {
        std::lock_guard lock(g_mutex);
        g_file = file;
        g_maxBytes = maxBytes;
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
    }

    void Log::Write(std::wstring_view line)
    {
        std::lock_guard lock(g_mutex);
        if (g_file.empty()) return;

        std::error_code ec;
        if (std::filesystem::exists(g_file, ec) && std::filesystem::file_size(g_file, ec) > g_maxBytes)
        {
            std::filesystem::rename(g_file, g_file.wstring() + L".1", ec); // replaces an existing .1
        }

        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t stamp[32];
        swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u:%02u.%03u ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);

        std::ofstream f(g_file, std::ios::app | std::ios::binary);
        f << ToUtf8(stamp) << ToUtf8(line) << "\n";
    }
}
```

- [ ] **Step 6: Run tests — verify all pass**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: `Passed: 40`, 0 failed.

- [ ] **Step 7: Add to the app project and build**

`WANUtility.vcxproj`: `<ClCompile Include="Core\Log.cpp"><PrecompiledHeader>NotUsing</PrecompiledHeader></ClCompile>`, `<ClInclude Include="Core\Log.h" />`. Build; expect success.

- [ ] **Step 8: Commit**

```powershell
git add Core Tests WANUtility.vcxproj
git commit -m "feat(core): UTF-8 rotating log`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Inbox scan rule

**Files:**
- Create: `Core/InboxScan.h`, `Core/InboxScan.cpp`, `Tests/InboxScanTests.cpp`
- Modify: `Tests/WANUtility.Tests.vcxproj`, `WANUtility.vcxproj`

**Interfaces:**
- Produces: `struct wan::core::StoreEntry { std::wstring id; bool incoming = false; };` and `bool wan::core::ScanBatch(std::vector<StoreEntry> const& batch, std::function<bool(std::wstring const&)> const& isKnown, std::vector<StoreEntry>& fresh)`. Used by `InboxImporter` (Task 10).

- [ ] **Step 1: Create `Core/InboxScan.h`**

```cpp
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace wan::core
{
    struct StoreEntry
    {
        std::wstring id;
        bool incoming = false;
    };

    // Scans one newest-first batch read from the Windows chat store.
    // - Unknown incoming entries are appended to `fresh` (newest first).
    // - Outgoing entries are skipped, known or not.
    // - Returns true at the first KNOWN INCOMING entry: everything older was imported before.
    // A known OUTGOING entry must not stop the scan: a message we just synced into the store can be
    // newer than incoming messages that have not been imported yet.
    bool ScanBatch(std::vector<StoreEntry> const& batch,
                   std::function<bool(std::wstring const&)> const& isKnown,
                   std::vector<StoreEntry>& fresh);
}
```

- [ ] **Step 2: Stub `Core/InboxScan.cpp`**

```cpp
#include "InboxScan.h"

namespace wan::core
{
    bool ScanBatch(std::vector<StoreEntry> const&, std::function<bool(std::wstring const&)> const&, std::vector<StoreEntry>&)
    {
        return false;
    }
}
```

- [ ] **Step 3: Write failing tests — `Tests/InboxScanTests.cpp`**

```cpp
#include "CppUnitTest.h"
#include "InboxScan.h"

#include <set>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

namespace
{
    auto KnownSet(std::set<std::wstring> ids)
    {
        return [ids](std::wstring const& id) { return ids.contains(id); };
    }
}

TEST_CLASS(InboxScanTests)
{
public:
    TEST_METHOD(CollectsUnknownIncomingNewestFirst)
    {
        std::vector<StoreEntry> fresh;
        bool stop = ScanBatch({ { L"c", true }, { L"b", true }, { L"a", true } }, KnownSet({}), fresh);
        Assert::IsFalse(stop);
        Assert::AreEqual(3, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"c"), fresh[0].id);
    }

    TEST_METHOD(StopsAtFirstKnownIncoming)
    {
        std::vector<StoreEntry> fresh;
        bool stop = ScanBatch({ { L"c", true }, { L"b", true }, { L"a", true } }, KnownSet({ L"b" }), fresh);
        Assert::IsTrue(stop);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"c"), fresh[0].id);
    }

    TEST_METHOD(KnownOutgoingDoesNotStopTheScan)
    {
        // We synced outgoing "out1" after incoming "new1" arrived but before it was imported.
        std::vector<StoreEntry> fresh;
        bool stop = ScanBatch({ { L"out1", false }, { L"new1", true }, { L"old1", true } }, KnownSet({ L"out1", L"old1" }), fresh);
        Assert::IsTrue(stop);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"new1"), fresh[0].id);
    }

    TEST_METHOD(UnknownOutgoingIsSkipped)
    {
        std::vector<StoreEntry> fresh;
        ScanBatch({ { L"x", false }, { L"y", true } }, KnownSet({}), fresh);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"y"), fresh[0].id);
    }

    TEST_METHOD(EmptyBatchDoesNotStop)
    {
        std::vector<StoreEntry> fresh;
        Assert::IsFalse(ScanBatch({}, KnownSet({}), fresh));
        Assert::AreEqual(0, static_cast<int>(fresh.size()));
    }
};
```

Add to `Tests/WANUtility.Tests.vcxproj`:

```xml
    <ClCompile Include="..\Core\InboxScan.cpp" />
    <ClCompile Include="InboxScanTests.cpp" />
```

- [ ] **Step 4: Run tests — verify they fail**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: `CollectsUnknownIncomingNewestFirst`, `StopsAtFirstKnownIncoming`, `KnownOutgoingDoesNotStopTheScan`, `UnknownOutgoingIsSkipped` FAIL; `EmptyBatchDoesNotStop` passes.

- [ ] **Step 5: Implement `Core/InboxScan.cpp`**

```cpp
#include "InboxScan.h"

namespace wan::core
{
    bool ScanBatch(std::vector<StoreEntry> const& batch,
                   std::function<bool(std::wstring const&)> const& isKnown,
                   std::vector<StoreEntry>& fresh)
    {
        for (auto const& entry : batch)
        {
            if (!entry.incoming)
            {
                continue;
            }
            if (isKnown(entry.id))
            {
                return true;
            }
            fresh.push_back(entry);
        }
        return false;
    }
}
```

- [ ] **Step 6: Run tests — verify all pass**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: `Passed: 45`, 0 failed.

- [ ] **Step 7: Add to the app project and build**

`WANUtility.vcxproj`: `<ClCompile Include="Core\InboxScan.cpp"><PrecompiledHeader>NotUsing</PrecompiledHeader></ClCompile>`, `<ClInclude Include="Core\InboxScan.h" />`. Build; expect success.

- [ ] **Step 8: Commit**

```powershell
git add Core Tests WANUtility.vcxproj
git commit -m "feat(core): inbox scan rule that never skips un-imported SMS`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: App shell — single instance, services container, Mica window, navigation

Replaces the spike UI. After this task the app launches into a Windows 11 shell with three empty pages.

**Files:**
- Create: `scripts/deploy.ps1`, `main.cpp`
- Create: `Services/Signal.h`, `Services/TimeUtil.h`, `Services/ErrorText.h`, `Services/AppServices.h`, `Services/AppServices.cpp`
- Create: `MessagesPage.idl/.xaml/.xaml.h/.xaml.cpp`, `UssdPage.idl/.xaml/.xaml.h/.xaml.cpp`, `SettingsPage.idl/.xaml/.xaml.h/.xaml.cpp` (placeholders; real content in later tasks)
- Replace: `MainWindow.idl`, `MainWindow.xaml`, `MainWindow.xaml.h`, `MainWindow.xaml.cpp` (spike code is discarded)
- Modify: `App.xaml.h`, `App.xaml.cpp`, `pch.h`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `core::MessageDb`, `core::DbError`, `core::Log` (Tasks 4–5).
- Produces:
  - `template <typename... Args> class wan::Signal { int Add(std::function<void(Args const&...)>); void Remove(int); void Raise(Args const&...); }`
  - `int64_t wan::NowMs()`, `int64_t wan::ToUnixMs(DateTime)`, `DateTime wan::FromUnixMs(int64_t)`, `std::wstring wan::FormatTime(int64_t ms)`
  - `std::wstring wan::DescribeError(winrt::hresult_error const&)`, `std::wstring wan::Widen(std::string_view utf8)`
  - `struct wan::AppServices` with: `dispatcher`, `localState`, `std::unique_ptr<core::MessageDb> db`, `std::wstring fatalError`, `bool windowFocused`, `std::wstring threadInView`, `std::wstring pendingOpenThread`, `Signal<std::wstring> ErrorRaised`, `Signal<std::wstring> MessagesChanged` (thread key, or `L""` = several), `Signal<std::wstring> OpenThreadRequested`
  - `wan::AppServices& wan::Services()`, `void wan::InitializeServices()`, `void wan::ReportError(std::wstring const&)`
  - `implementation::MainWindow::ShowAndFocus()`, `ShowThread(std::wstring const&)`, `AllowCloseAndClose()`; `implementation::App::ShowMainWindow()`, `ShowThread(std::wstring const&)`, `ExitApp()`
  - Marker comments later tasks insert code before: `// [members]` (AppServices.h), `// [services]` (AppServices.cpp), `// [app-init]`, `// [activation]`, `// [exit]` (App.xaml.cpp), `// [window-init]` (MainWindow.xaml.cpp)

- [ ] **Step 1: Create `scripts/deploy.ps1`**

```powershell
# Builds the app, (re)registers the dev package only when the manifest changed, preserves
# LocalState (message DB + logs) across re-registration, and launches the app.
# Why: re-registering the same version is blocked (0x80073CFB), and Remove-AppxPackage deletes LocalState.
param([switch]$NoLaunch)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
$name = '29d4b340-cdbf-4ef2-8a8c-cb0bc4b163e0'

Get-Process WANUtility -ErrorAction SilentlyContinue | Stop-Process -Force
& $msbuild "$root\WANUtility.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }

$layout = "$root\x64\Debug\WANUtility"
$manifest = "$layout\AppxManifest.xml"
$stamp = "$layout\.registered-manifest-hash"
$hash = (Get-FileHash $manifest).Hash
$pkg = Get-AppxPackage $name
$upToDate = $pkg -and ($pkg.InstallLocation -eq $layout) -and (Test-Path $stamp) -and ((Get-Content $stamp) -eq $hash)

if (-not $upToDate) {
    $backup = Join-Path $env:TEMP 'WANUtility-LocalState-backup'
    if ($pkg) {
        $ls = "$env:LOCALAPPDATA\Packages\$($pkg.PackageFamilyName)\LocalState"
        if (Test-Path $ls) {
            Remove-Item $backup -Recurse -Force -ErrorAction SilentlyContinue
            Copy-Item $ls $backup -Recurse
            Write-Host "Backed up LocalState to $backup"
        }
        Remove-AppxPackage $pkg.PackageFullName
    }
    Add-AppxPackage -Register $manifest
    $pkg = Get-AppxPackage $name
    $ls = "$env:LOCALAPPDATA\Packages\$($pkg.PackageFamilyName)\LocalState"
    if (Test-Path $backup) {
        New-Item -ItemType Directory -Force $ls | Out-Null
        Copy-Item "$backup\*" $ls -Recurse -Force
        Write-Host "Restored LocalState"
    }
    $hash | Set-Content $stamp
    Write-Host "Registered package from $layout"
}

if (-not $NoLaunch) { Start-Process "shell:AppsFolder\$($pkg.PackageFamilyName)!App" }
Write-Host "LocalState: $env:LOCALAPPDATA\Packages\$($pkg.PackageFamilyName)\LocalState"
```

- [ ] **Step 2: Create `Services/Signal.h`**

```cpp
#pragma once
#include <functional>
#include <utility>
#include <vector>

namespace wan
{
    // Minimal observer list for service -> UI notifications. UI thread only.
    // Raise() iterates a copy, so handlers may Add/Remove while being raised.
    template <typename... Args>
    class Signal
    {
    public:
        using Handler = std::function<void(Args const&...)>;

        int Add(Handler handler)
        {
            m_handlers.emplace_back(++m_next, std::move(handler));
            return m_next;
        }

        void Remove(int token)
        {
            std::erase_if(m_handlers, [token](auto const& entry) { return entry.first == token; });
        }

        void Raise(Args const&... args)
        {
            auto copy = m_handlers;
            for (auto& [token, handler] : copy)
            {
                handler(args...);
            }
        }

    private:
        std::vector<std::pair<int, Handler>> m_handlers;
        int m_next = 0;
    };
}
```

- [ ] **Step 3: Create `Services/TimeUtil.h`**

```cpp
#pragma once
#include <chrono>
#include <ctime>
#include <string>

namespace wan
{
    inline int64_t NowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    inline int64_t ToUnixMs(winrt::Windows::Foundation::DateTime t)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(winrt::clock::to_sys(t).time_since_epoch()).count();
    }

    inline winrt::Windows::Foundation::DateTime FromUnixMs(int64_t ms)
    {
        return winrt::clock::from_sys(std::chrono::system_clock::time_point(std::chrono::milliseconds(ms)));
    }

    // "13:54" for today, "12/10 13:54" otherwise (local time).
    inline std::wstring FormatTime(int64_t ms)
    {
        const time_t secs = static_cast<time_t>(ms / 1000);
        const time_t now = time(nullptr);
        tm t{};
        tm n{};
        localtime_s(&t, &secs);
        localtime_s(&n, &now);
        wchar_t buf[32];
        const bool today = t.tm_year == n.tm_year && t.tm_yday == n.tm_yday;
        wcsftime(buf, 32, today ? L"%H:%M" : L"%d/%m %H:%M", &t);
        return buf;
    }
}
```

- [ ] **Step 4: Create `Services/ErrorText.h`**

```cpp
#pragma once
#include <string>
#include <string_view>

namespace wan
{
    // "0x80070005 Access is denied." — the hex code is always included (the spike showed
    // some codes, e.g. 0xD0000022, have no system message text).
    inline std::wstring DescribeError(winrt::hresult_error const& e)
    {
        wchar_t code[16];
        swprintf_s(code, L"0x%08X", static_cast<uint32_t>(e.code().value));
        std::wstring message{ e.message() };
        return message.empty() ? std::wstring(code) : std::wstring(code) + L" " + message;
    }

    inline std::wstring Widen(std::string_view utf8)
    {
        if (utf8.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), size);
        return out;
    }
}
```

- [ ] **Step 5: Create `Services/AppServices.h`**

```cpp
#pragma once
#include "Core/MessageDb.h"
#include "Services/Signal.h"

#include <filesystem>
#include <memory>
#include <string>

namespace wan
{
    // Process-wide service container. Created once in App::OnLaunched, used on the UI thread only.
    struct AppServices
    {
        winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
        std::filesystem::path localState;
        std::unique_ptr<core::MessageDb> db;   // null when fatalError is set
        std::wstring fatalError;               // startup failure shown in the error bar

        bool windowFocused = false;            // main window visible and active
        std::wstring threadInView;             // thread shown on MessagesPage, empty otherwise
        std::wstring pendingOpenThread;        // thread to open when MessagesPage loads

        Signal<std::wstring> ErrorRaised;        // non-fatal error text for the error bar
        Signal<std::wstring> MessagesChanged;    // thread key, or L"" when several threads changed
        Signal<std::wstring> OpenThreadRequested;

        // [members]
    };

    AppServices& Services();
    void InitializeServices();
    void ReportError(std::wstring const& message);
}
```

- [ ] **Step 6: Create `Services/AppServices.cpp`**

```cpp
#include "pch.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Core/Log.h"

namespace wan
{
    AppServices& Services()
    {
        // Intentionally never destroyed: services hold COM objects, and static destructors run
        // after the apartment is torn down. SQLite has committed every statement already.
        static AppServices* instance = new AppServices();
        return *instance;
    }

    void ReportError(std::wstring const& message)
    {
        core::Log::Write(L"ERROR " + message);
        Services().ErrorRaised.Raise(message);
    }

    void InitializeServices()
    {
        auto& s = Services();
        s.dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        s.localState = std::filesystem::path(std::wstring(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()));
        core::Log::Init(s.localState / L"logs" / L"wanutility.log");
        core::Log::Write(L"--- WANUtility starting ---");

        try
        {
            s.db = std::make_unique<core::MessageDb>((s.localState / L"wanutility.db").wstring());
        }
        catch (core::DbError const& e)
        {
            s.fatalError = L"The message database could not be opened: " + Widen(e.what());
            core::Log::Write(L"FATAL " + s.fatalError);
        }

        // [services]
    }
}
```

- [ ] **Step 7: Update `pch.h`** — insert before the final `#include <wil/cppwinrt_helpers.h>` line:

```cpp
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Chat.h>
#include <winrt/Windows.Devices.Sms.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Networking.NetworkOperators.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include <winrt/Microsoft.Windows.AppNotifications.h>
#include <winrt/Microsoft.Windows.AppNotifications.Builder.h>
#include <microsoft.ui.xaml.window.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>
```

- [ ] **Step 8: Create `main.cpp`** (single instance before any XAML starts)

```cpp
#include "pch.h"
#include "App.xaml.h"

#include <wil/resource.h>

// Generated by the XAML compiler (App.xaml.g.hpp) when DISABLE_XAML_GENERATED_MAIN is defined.
int __stdcall wXamlGeneratedMain();

namespace
{
    // Returns true when another WANUtility process already runs; this launch's activation
    // (normal launch, toast click, startup task) is forwarded to it and this process exits.
    bool RedirectToRunningInstance()
    {
        using namespace winrt::Microsoft::Windows::AppLifecycle;
        auto args = AppInstance::GetCurrent().GetActivatedEventArgs();
        auto main = AppInstance::FindOrRegisterForKey(L"WANUtility.Main");
        if (main.IsCurrent())
        {
            return false;
        }

        // We were launched by the user, so we may hand foreground rights to the running instance.
        AllowSetForegroundWindow(main.ProcessId());

        // RedirectActivationToAsync must not block this STA thread: run it on a worker thread and
        // wait while pumping COM (Windows App SDK single-instancing pattern).
        wil::unique_event done;
        done.create();
        HANDLE doneHandle = done.get();
        std::thread worker([args, main, doneHandle]
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            main.RedirectActivationToAsync(args).get();
            SetEvent(doneHandle);
            winrt::uninit_apartment();
        });
        DWORD index = 0;
        CoWaitForMultipleObjects(CWMO_DEFAULT, INFINITE, 1, &doneHandle, &index);
        worker.join();
        return true;
    }
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (RedirectToRunningInstance())
    {
        return 0;
    }
    // Calls init_apartment again (returns S_FALSE: already initialized) and starts XAML.
    return wXamlGeneratedMain();
}
```

- [ ] **Step 9: Replace `MainWindow.idl`**

```
namespace WANUtility
{
    [default_interface]
    runtimeclass MainWindow : Microsoft.UI.Xaml.Window
    {
        MainWindow();
    }
}
```

- [ ] **Step 10: Replace `MainWindow.xaml`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<Window
    x:Class="WANUtility.MainWindow"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:local="using:WANUtility"
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
    xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
    mc:Ignorable="d"
    Title="WANUtility">

    <Window.SystemBackdrop>
        <MicaBackdrop />
    </Window.SystemBackdrop>

    <Grid>
        <Grid.RowDefinitions>
            <RowDefinition Height="48" />
            <RowDefinition Height="*" />
        </Grid.RowDefinitions>

        <Grid x:Name="AppTitleBar" Padding="16,0,0,0">
            <StackPanel Orientation="Horizontal" Spacing="12" VerticalAlignment="Center">
                <Image Source="ms-appx:///Assets/Square44x44Logo.targetsize-24_altform-unplated.png" Width="16" Height="16" />
                <TextBlock Text="WANUtility" Style="{StaticResource CaptionTextBlockStyle}" VerticalAlignment="Center" />
            </StackPanel>
        </Grid>

        <NavigationView
            x:Name="Nav"
            Grid.Row="1"
            PaneDisplayMode="Left"
            IsBackButtonVisible="Collapsed"
            IsSettingsVisible="True"
            OpenPaneLength="220"
            SelectionChanged="Nav_SelectionChanged">
            <NavigationView.MenuItems>
                <NavigationViewItem Content="Messages" Tag="messages">
                    <NavigationViewItem.Icon>
                        <FontIcon Glyph="&#xE8BD;" />
                    </NavigationViewItem.Icon>
                </NavigationViewItem>
                <NavigationViewItem Content="USSD" Tag="ussd">
                    <NavigationViewItem.Icon>
                        <FontIcon Glyph="&#xE75F;" />
                    </NavigationViewItem.Icon>
                </NavigationViewItem>
            </NavigationView.MenuItems>

            <NavigationView.PaneFooter>
                <StackPanel Orientation="Horizontal" Spacing="8" Padding="16,8,16,8">
                    <FontIcon x:Name="SignalIcon" Glyph="&#xEC3B;" FontSize="14" Opacity="0.4" />
                    <TextBlock x:Name="CarrierText" Text="—" Style="{StaticResource CaptionTextBlockStyle}" VerticalAlignment="Center" />
                </StackPanel>
            </NavigationView.PaneFooter>

            <Grid>
                <Grid.RowDefinitions>
                    <RowDefinition Height="Auto" />
                    <RowDefinition Height="*" />
                </Grid.RowDefinitions>
                <StackPanel Padding="16,0,16,0">
                    <InfoBar x:Name="ModemBar" Severity="Warning" IsClosable="False" />
                    <InfoBar x:Name="ErrorBar" Severity="Error" IsClosable="True" />
                </StackPanel>
                <Frame x:Name="ContentFrame" Grid.Row="1" />
            </Grid>
        </NavigationView>
    </Grid>
</Window>
```

- [ ] **Step 11: Replace `MainWindow.xaml.h`**

```cpp
#pragma once

#include "MainWindow.g.h"

namespace winrt::WANUtility::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow() {}

        // Runs after XAML loads (C++/WinRT deferred InitializeComponent pattern).
        void InitializeComponent();

        void Nav_SelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const& sender,
                                  Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);

        void ShowAndFocus();
        void ShowThread(std::wstring const& threadKey);
        void AllowCloseAndClose();

    private:
        void OnClosing(Microsoft::UI::Windowing::AppWindow const& sender, Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args);
        void ShowError(std::wstring const& message);

        HWND m_hwnd{};
        bool m_closeToTray = false; // Task 13 turns this on
        bool m_allowClose = false;
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
```

- [ ] **Step 12: Replace `MainWindow.xaml.cpp`**

```cpp
#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "Services/AppServices.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Windowing;

namespace winrt::WANUtility::implementation
{
    void MainWindow::InitializeComponent()
    {
        MainWindowT::InitializeComponent();

        this->try_as<::IWindowNative>()->get_WindowHandle(&m_hwnd);
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
        AppWindow().Resize({ 1100, 720 });
        AppWindow().Closing({ this, &MainWindow::OnClosing });
        Activated([](IInspectable const&, WindowActivatedEventArgs const& e)
        {
            wan::Services().windowFocused = e.WindowActivationState() != WindowActivationState::Deactivated;
        });

        auto& s = wan::Services();
        s.ErrorRaised.Add([this](std::wstring const& message) { ShowError(message); });
        if (!s.fatalError.empty())
        {
            ShowError(s.fatalError);
        }

        // [window-init]

        Nav().SelectedItem(Nav().MenuItems().GetAt(0));
    }

    void MainWindow::Nav_SelectionChanged(NavigationView const&, NavigationViewSelectionChangedEventArgs const& args)
    {
        if (args.IsSettingsSelected())
        {
            ContentFrame().Navigate(xaml_typename<WANUtility::SettingsPage>());
            return;
        }
        auto item = args.SelectedItemContainer();
        if (!item)
        {
            return;
        }
        auto tag = unbox_value<hstring>(item.Tag());
        if (tag == L"messages")
        {
            ContentFrame().Navigate(xaml_typename<WANUtility::MessagesPage>());
        }
        else if (tag == L"ussd")
        {
            ContentFrame().Navigate(xaml_typename<WANUtility::UssdPage>());
        }
    }

    void MainWindow::ShowAndFocus()
    {
        AppWindow().Show();
        Activate();
        SetForegroundWindow(m_hwnd);
    }

    void MainWindow::ShowThread(std::wstring const& threadKey)
    {
        auto& s = wan::Services();
        s.pendingOpenThread = threadKey;
        auto messagesItem = Nav().MenuItems().GetAt(0);
        if (Nav().SelectedItem() != messagesItem)
        {
            Nav().SelectedItem(messagesItem); // MessagesPage consumes pendingOpenThread when it loads
        }
        else
        {
            s.OpenThreadRequested.Raise(threadKey);
        }
    }

    void MainWindow::AllowCloseAndClose()
    {
        m_allowClose = true;
        Close();
    }

    void MainWindow::OnClosing(Microsoft::UI::Windowing::AppWindow const&, AppWindowClosingEventArgs const& args)
    {
        if (m_closeToTray && !m_allowClose)
        {
            args.Cancel(true);
            AppWindow().Hide();
            wan::Services().windowFocused = false;
        }
    }

    void MainWindow::ShowError(std::wstring const& message)
    {
        ErrorBar().Title(L"Something went wrong");
        ErrorBar().Message(message);
        ErrorBar().IsOpen(true);
    }
}
```

- [ ] **Step 13: Create the three placeholder pages**

For each `Name` in `MessagesPage`, `UssdPage`, `SettingsPage` (with `Title` = `Messages`, `USSD`, `Settings`), create four files. Shown for `SettingsPage`; repeat with the name and title substituted.

`SettingsPage.idl`:
```
namespace WANUtility
{
    [default_interface]
    runtimeclass SettingsPage : Microsoft.UI.Xaml.Controls.Page
    {
        SettingsPage();
    }
}
```

`SettingsPage.xaml`:
```xml
<?xml version="1.0" encoding="utf-8"?>
<Page
    x:Class="WANUtility.SettingsPage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <TextBlock Text="Settings" Style="{StaticResource TitleTextBlockStyle}" Margin="24,16,24,16" />
</Page>
```

`SettingsPage.xaml.h`:
```cpp
#pragma once

#include "SettingsPage.g.h"

namespace winrt::WANUtility::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage() {}
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}
```

`SettingsPage.xaml.cpp`:
```cpp
#include "pch.h"
#include "SettingsPage.xaml.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif
```

- [ ] **Step 14: Replace `App.xaml.h`**

```cpp
#pragma once

#include "App.xaml.g.h"

namespace winrt::WANUtility::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

        void ShowMainWindow();
        void ShowThread(std::wstring const& threadKey);
        void ExitApp();

    private:
        void HandleActivation(Microsoft::Windows::AppLifecycle::AppActivationArguments const& args, bool fromRedirect);

        WANUtility::MainWindow m_window{ nullptr };
    };
}
```

- [ ] **Step 15: Replace `App.xaml.cpp`**

```cpp
#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

#include "Core/Log.h"
#include "Services/AppServices.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::Windows::AppLifecycle;

namespace winrt::WANUtility::implementation
{
    App::App()
    {
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            wan::core::Log::Write(L"UNHANDLED " + std::wstring(e.Message()));
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
            if (IsDebuggerPresent())
            {
                __debugbreak();
            }
#endif
        });
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        wan::InitializeServices();

        // [app-init]

        m_window = make<MainWindow>();

        // A second launch (or a toast click) in another process is redirected here by main.cpp.
        AppInstance::GetCurrent().Activated([this](IInspectable const&, AppActivationArguments const& args)
        {
            wan::Services().dispatcher.TryEnqueue([this, args] { HandleActivation(args, true); });
        });
        HandleActivation(AppInstance::GetCurrent().GetActivatedEventArgs(), false);
    }

    void App::HandleActivation(AppActivationArguments const& args, bool fromRedirect)
    {
        wan::core::Log::Write(L"Activation kind " + std::to_wstring(static_cast<int>(args.Kind())) + (fromRedirect ? L" (redirected)" : L""));

        // [activation]

        ShowMainWindow();
    }

    void App::ShowMainWindow()
    {
        get_self<MainWindow>(m_window)->ShowAndFocus();
    }

    void App::ShowThread(std::wstring const& threadKey)
    {
        ShowMainWindow();
        get_self<MainWindow>(m_window)->ShowThread(threadKey);
    }

    void App::ExitApp()
    {
        wan::core::Log::Write(L"Exit requested");

        // [exit]

        get_self<MainWindow>(m_window)->AllowCloseAndClose();
        Application::Current().Exit();
    }
}
```

- [ ] **Step 16: Update `WANUtility.vcxproj`**

In the first `<ItemDefinitionGroup>`, inside `<ClCompile>`, add:

```xml
      <PreprocessorDefinitions>DISABLE_XAML_GENERATED_MAIN;%(PreprocessorDefinitions)</PreprocessorDefinitions>
```

Add to the `<ItemGroup>` containing `<Page Include="MainWindow.xaml" />`:

```xml
    <Page Include="MessagesPage.xaml" />
    <Page Include="UssdPage.xaml" />
    <Page Include="SettingsPage.xaml" />
```

Add to the `ClInclude` item group:

```xml
    <ClInclude Include="Services\AppServices.h" />
    <ClInclude Include="Services\ErrorText.h" />
    <ClInclude Include="Services\Signal.h" />
    <ClInclude Include="Services\TimeUtil.h" />
    <ClInclude Include="MessagesPage.xaml.h"><DependentUpon>MessagesPage.xaml</DependentUpon></ClInclude>
    <ClInclude Include="UssdPage.xaml.h"><DependentUpon>UssdPage.xaml</DependentUpon></ClInclude>
    <ClInclude Include="SettingsPage.xaml.h"><DependentUpon>SettingsPage.xaml</DependentUpon></ClInclude>
```

Add to the `ClCompile` item group:

```xml
    <ClCompile Include="main.cpp" />
    <ClCompile Include="Services\AppServices.cpp" />
    <ClCompile Include="MessagesPage.xaml.cpp"><DependentUpon>MessagesPage.xaml</DependentUpon></ClCompile>
    <ClCompile Include="UssdPage.xaml.cpp"><DependentUpon>UssdPage.xaml</DependentUpon></ClCompile>
    <ClCompile Include="SettingsPage.xaml.cpp"><DependentUpon>SettingsPage.xaml</DependentUpon></ClCompile>
```

Add to the `Midl` item group:

```xml
    <Midl Include="MessagesPage.idl"><SubType>Code</SubType><DependentUpon>MessagesPage.xaml</DependentUpon></Midl>
    <Midl Include="UssdPage.idl"><SubType>Code</SubType><DependentUpon>UssdPage.xaml</DependentUpon></Midl>
    <Midl Include="SettingsPage.idl"><SubType>Code</SubType><DependentUpon>SettingsPage.xaml</DependentUpon></Midl>
```

- [ ] **Step 17: Build and deploy**

Run: `powershell -ExecutionPolicy Bypass -File scripts\deploy.ps1`
Expected: build succeeds; "Registered package from …\x64\Debug\WANUtility" (first run re-registers because the stamp file does not exist yet); the app launches.

If the build reports `wWinMain` defined twice, `DISABLE_XAML_GENERATED_MAIN` is not applied — check Step 16.

- [ ] **Step 18: Verify — observe, don't assume**

```powershell
$ls = "$env:LOCALAPPDATA\Packages\29d4b340-cdbf-4ef2-8a8c-cb0bc4b163e0_gpfrfcskskzsw\LocalState"
Get-Content "$ls\logs\wanutility.log" -Encoding UTF8 -Tail 5
Test-Path "$ls\wanutility.db"
Start-Process "shell:AppsFolder\29d4b340-cdbf-4ef2-8a8c-cb0bc4b163e0_gpfrfcskskzsw!App"; Start-Sleep 3
(Get-Process WANUtility).Count
Get-Content "$ls\logs\wanutility.log" -Encoding UTF8 -Tail 3
```
Expected: log contains `--- WANUtility starting ---` and `Activation kind 0`; `True` for the DB; process count `1` after the second launch; the last log line is `Activation kind 0 (redirected)`.

Ask the user to look at the window and confirm: Mica background, custom title bar with icon + "WANUtility", left navigation with Messages / USSD and Settings at the bottom, clicking each shows its page title. Record their answer in the task notes.

- [ ] **Step 19: Commit**

```powershell
git add -A
git commit -m "feat(app): Windows 11 shell with single instance and services container`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: ModemService + carrier/signal footer + modem warning bar

**Files:**
- Create: `Services/ModemService.h`, `Services/ModemService.cpp`
- Modify: `Services/AppServices.h`, `Services/AppServices.cpp`, `MainWindow.xaml.h`, `MainWindow.xaml.cpp`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `Services()`, `Signal`, `DescribeError`, `core::Log`.
- Produces: `struct wan::ModemStatus { bool ready; std::wstring carrier; int signalBars /* 0–5, -1 unknown */; std::wstring problem; }`, `struct wan::ModemInfo { std::wstring model, firmware, carrier, smsc; }`, `class wan::ModemService { void Initialize(); void Refresh(); ModemStatus const& Status() const; ModemInfo Info() const; SmsDevice2 SmsDevice() const; hstring NetworkAccountId() const; Signal<> StatusChanged; }`, member `Services().modem`.

- [ ] **Step 1: Create `Services/ModemService.h`**

```cpp
#pragma once
#include "Services/Signal.h"

#include <string>

namespace wan
{
    struct ModemStatus
    {
        bool ready = false;
        std::wstring carrier;  // empty when unknown
        int signalBars = -1;   // 0–5, -1 when unknown (e.g. no active cellular connection)
        std::wstring problem;  // user-facing reason when !ready
    };

    struct ModemInfo
    {
        std::wstring model;
        std::wstring firmware;
        std::wstring carrier;
        std::wstring smsc;
    };

    class ModemService
    {
    public:
        void Initialize();
        void Refresh();

        ModemStatus const& Status() const { return m_status; }
        ModemInfo Info() const;
        winrt::Windows::Devices::Sms::SmsDevice2 SmsDevice() const { return m_sms; }
        winrt::hstring NetworkAccountId() const { return m_accountId; }

        Signal<> StatusChanged;

    private:
        winrt::Windows::Devices::Sms::SmsDevice2 m_sms{ nullptr };
        winrt::hstring m_accountId;
        ModemStatus m_status;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
    };
}
```

- [ ] **Step 2: Create `Services/ModemService.cpp`**

```cpp
#include "pch.h"
#include "Services/ModemService.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Core/Log.h"

using namespace winrt;
using namespace winrt::Windows::Devices::Sms;
using namespace winrt::Windows::Networking::Connectivity;
using namespace winrt::Windows::Networking::NetworkOperators;

namespace wan
{
    namespace
    {
        std::wstring DescribeDeviceStatus(SmsDeviceStatus status)
        {
            switch (status)
            {
            case SmsDeviceStatus::Off: return L"The cellular radio is off. Check airplane mode.";
            case SmsDeviceStatus::SimNotInserted: return L"No SIM card is inserted.";
            case SmsDeviceStatus::BadSim: return L"The SIM card is not working.";
            case SmsDeviceStatus::DeviceFailure: return L"The modem reported a failure.";
            case SmsDeviceStatus::SubscriptionNotActivated: return L"The SIM subscription is not activated.";
            case SmsDeviceStatus::DeviceLocked: return L"The SIM is locked (PIN required).";
            case SmsDeviceStatus::DeviceBlocked: return L"The SIM is blocked (PUK required).";
            default: return L"The modem is not ready.";
            }
        }
    }

    void ModemService::Initialize()
    {
        try
        {
            m_sms = SmsDevice2::GetDefault();
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"SmsDevice2::GetDefault failed: " + DescribeError(e));
        }
        try
        {
            auto ids = MobileBroadbandAccount::AvailableNetworkAccountIds();
            if (ids.Size() > 0)
            {
                m_accountId = ids.GetAt(0);
            }
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"AvailableNetworkAccountIds failed: " + DescribeError(e));
        }

        // Both events fire on worker threads: hop to the UI thread before touching state.
        if (m_sms)
        {
            m_sms.DeviceStatusChanged([](SmsDevice2 const&, IInspectable const&)
            {
                Services().dispatcher.TryEnqueue([] { Services().modem->Refresh(); });
            });
        }
        NetworkInformation::NetworkStatusChanged([](IInspectable const&)
        {
            Services().dispatcher.TryEnqueue([] { Services().modem->Refresh(); });
        });

        // Signal strength has no change event; poll it.
        m_timer = Services().dispatcher.CreateTimer();
        m_timer.Interval(std::chrono::seconds(30));
        m_timer.Tick([](auto const&, auto const&) { Services().modem->Refresh(); });
        m_timer.Start();

        Refresh();
    }

    void ModemService::Refresh()
    {
        ModemStatus s;
        if (!m_sms)
        {
            s.problem = L"No cellular modem was found.";
        }
        else
        {
            try
            {
                const auto status = m_sms.DeviceStatus();
                s.ready = status == SmsDeviceStatus::Ready;
                if (!s.ready)
                {
                    s.problem = DescribeDeviceStatus(status);
                }
            }
            catch (hresult_error const& e)
            {
                s.problem = L"Cannot read the modem status: " + DescribeError(e);
            }
        }

        if (!m_accountId.empty())
        {
            try
            {
                auto account = MobileBroadbandAccount::CreateFromNetworkAccountId(m_accountId);
                if (auto network = account.CurrentNetwork())
                {
                    s.carrier = std::wstring(network.RegisteredProviderName());
                }
            }
            catch (hresult_error const& e)
            {
                core::Log::Write(L"Reading carrier failed: " + DescribeError(e));
            }
        }

        try
        {
            for (auto const& profile : NetworkInformation::GetConnectionProfiles())
            {
                if (!profile.IsWwanConnectionProfile())
                {
                    continue;
                }
                if (auto bars = profile.GetSignalBars())
                {
                    s.signalBars = bars.Value();
                    break;
                }
            }
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"Reading signal failed: " + DescribeError(e));
        }

        const bool changed = s.ready != m_status.ready || s.carrier != m_status.carrier ||
                             s.signalBars != m_status.signalBars || s.problem != m_status.problem;
        m_status = s;
        if (changed)
        {
            core::Log::Write(L"Modem: ready=" + std::to_wstring(s.ready) + L" carrier='" + s.carrier +
                             L"' bars=" + std::to_wstring(s.signalBars) + L" problem='" + s.problem + L"'");
            StatusChanged.Raise();
        }
    }

    ModemInfo ModemService::Info() const
    {
        ModemInfo info;
        info.carrier = m_status.carrier;
        try
        {
            if (m_sms) info.smsc = std::wstring(m_sms.SmscAddress());
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"Reading SMSC failed: " + DescribeError(e));
        }
        try
        {
            if (!m_accountId.empty())
            {
                auto device = MobileBroadbandAccount::CreateFromNetworkAccountId(m_accountId).CurrentDeviceInformation();
                info.model = std::wstring(device.Model());
                info.firmware = std::wstring(device.FirmwareInformation());
            }
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"Reading device info failed: " + DescribeError(e));
        }
        return info;
    }
}
```

- [ ] **Step 3: Register the service**

`Services/AppServices.h`: add `#include "Services/ModemService.h"` after the other includes, and insert before `// [members]`:

```cpp
        std::unique_ptr<ModemService> modem;
```

`Services/AppServices.cpp`: insert before `// [services]`:

```cpp
        s.modem = std::make_unique<ModemService>();
        s.modem->Initialize();
```

- [ ] **Step 4: Show modem state in the window**

`MainWindow.xaml.h`: add to the `private:` section:

```cpp
        void UpdateModemUi();
```

`MainWindow.xaml.cpp`: add `#include "Services/ModemService.h"` after the AppServices include; insert before `// [window-init]`:

```cpp
        s.modem->StatusChanged.Add([this] { UpdateModemUi(); });
        UpdateModemUi();
```

and add this method inside the namespace:

```cpp
    void MainWindow::UpdateModemUi()
    {
        auto const& st = wan::Services().modem->Status();
        std::wstring text = st.carrier.empty() ? L"Mobile" : st.carrier;
        if (st.signalBars >= 0)
        {
            text += L" · " + std::to_wstring(st.signalBars) + L"/5";
        }
        CarrierText().Text(text);

        // Segoe Fluent Icons SignalBars1..SignalBars5 = U+EC37..U+EC3B (0 bars reuses 1 at low opacity).
        static constexpr wchar_t kBars[] = { L'\uEC37', L'\uEC37', L'\uEC38', L'\uEC39', L'\uEC3A', L'\uEC3B' };
        SignalIcon().Glyph(hstring(std::wstring(1, kBars[std::clamp(st.signalBars, 0, 5)])));
        SignalIcon().Opacity(st.ready && st.signalBars > 0 ? 1.0 : 0.4);

        ModemBar().Title(L"Modem not ready");
        ModemBar().Message(st.problem);
        ModemBar().IsOpen(!st.ready);
    }
```

`WANUtility.vcxproj`: add `<ClInclude Include="Services\ModemService.h" />` and `<ClCompile Include="Services\ModemService.cpp" />`.

- [ ] **Step 5: Deploy and verify**

Run: `powershell -ExecutionPolicy Bypass -File scripts\deploy.ps1`, then:

```powershell
Get-Content "$env:LOCALAPPDATA\Packages\29d4b340-cdbf-4ef2-8a8c-cb0bc4b163e0_gpfrfcskskzsw\LocalState\logs\wanutility.log" -Encoding UTF8 | Select-String "Modem:"
```
Expected: a line like `Modem: ready=1 carrier='…' bars=N problem=''`. Record the actual carrier string — `RegisteredProviderName` was not observed in the spike (`ServiceProviderName` was empty). If it is empty, the footer shows "Mobile"; note this in CLAUDE.md as an observed fact.

Ask the user to confirm the pane footer shows a signal icon and "carrier · N/5", and that the icon looks like signal bars (if it is an unrelated glyph, look up "SignalBars" in the Segoe Fluent Icons reference and correct `kBars`).

- [ ] **Step 6: Airplane-mode check (ask the user first — it drops their connection briefly)**

With the user's OK: turn on airplane mode, wait up to 30 s, confirm the warning bar "Modem not ready — The cellular radio is off…" appears; turn it off, confirm the bar disappears. Paste the matching `Modem:` log lines into the task notes.

- [ ] **Step 7: Commit**

```powershell
git add -A
git commit -m "feat(modem): readiness, carrier and signal in the navigation pane`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Interactive USSD — session manager + UssdPage

**Files:**
- Create: `Services/UssdSessionManager.h`, `Services/UssdSessionManager.cpp`
- Replace: `UssdPage.xaml`, `UssdPage.xaml.h`, `UssdPage.xaml.cpp`
- Modify: `Services/AppServices.h`, `Services/AppServices.cpp`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `core::DecodeUtf16BE`, `core::IsUcs2Dcs` (Task 1), `core::ParseUssdMenu`, `core::UssdOption` (Task 3), `MessageDb::AddUssdHistory/RecentUssd/UssdCodes/AddUssdCode` (Task 4), `ModemService::NetworkAccountId/Status/StatusChanged` (Task 8), `core::Trim`, `core::FirstLine`, `FormatTime`, `NowMs`.
- Produces: `enum class wan::UssdState { Idle, Sending, AwaitingReply, Ended }`; `class wan::UssdSessionManager { fire_and_forget Dial(std::wstring code); fire_and_forget Reply(std::wstring text); void End(); UssdState State() const; std::wstring const& ReplyText() const; std::vector<core::UssdOption> const& Options() const; std::wstring StatusLine() const; Signal<> Changed; }`; `std::wstring wan::DecodeUssdPayload(UssdMessage const&)`; member `Services().ussd`.

- [ ] **Step 1: Create `Services/UssdSessionManager.h`**

```cpp
#pragma once
#include "Core/UssdMenuParser.h"
#include "Services/Signal.h"

#include <string>
#include <vector>

namespace wan
{
    enum class UssdState { Idle, Sending, AwaitingReply, Ended };

    // One USSD session at a time. Lives in AppServices, so leaving the page keeps the session.
    class UssdSessionManager
    {
    public:
        winrt::fire_and_forget Dial(std::wstring code);
        winrt::fire_and_forget Reply(std::wstring text);
        void End();

        UssdState State() const { return m_state; }
        std::wstring const& ReplyText() const { return m_text; }
        std::vector<core::UssdOption> const& Options() const { return m_options; }
        std::wstring StatusLine() const;

        Signal<> Changed;

    private:
        winrt::Windows::Foundation::IAsyncAction SendAsync(std::wstring text);
        void HandleReply(winrt::Windows::Networking::NetworkOperators::UssdReply const& reply);
        void Finish(std::wstring const& text);
        void SetState(UssdState state);

        winrt::Windows::Networking::NetworkOperators::UssdSession m_session{ nullptr };
        UssdState m_state = UssdState::Idle;
        std::wstring m_text;
        std::vector<core::UssdOption> m_options;
        std::vector<std::wstring> m_path; // codes/replies sent in this session
    };

    // UCS-2 replies are decoded from raw bytes because PayloadAsText() returns an empty string
    // for them (observed with dtac, DCS 0x48).
    std::wstring DecodeUssdPayload(winrt::Windows::Networking::NetworkOperators::UssdMessage const& message);
}
```

- [ ] **Step 2: Create `Services/UssdSessionManager.cpp`**

```cpp
#include "pch.h"
#include "Services/UssdSessionManager.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/TimeUtil.h"
#include "Core/Log.h"
#include "Core/Ucs2.h"

using namespace winrt;
using namespace winrt::Windows::Networking::NetworkOperators;

namespace wan
{
    std::wstring DecodeUssdPayload(UssdMessage const& message)
    {
        if (!message)
        {
            return {};
        }
        auto raw = message.GetPayload();
        const auto dcs = message.DataCodingScheme();
        if (core::IsUcs2Dcs(dcs))
        {
            return core::DecodeUtf16BE(std::span<const uint8_t>(raw.data(), raw.size()));
        }
        std::wstring text{ message.PayloadAsText() };
        if (!text.empty() || raw.size() == 0)
        {
            return text;
        }
        wchar_t buf[48];
        swprintf_s(buf, L"Unreadable reply (DCS 0x%02X)", dcs);
        return buf;
    }

    winrt::fire_and_forget UssdSessionManager::Dial(std::wstring code)
    {
        if (m_state == UssdState::Sending)
        {
            co_return;
        }
        if (m_session)
        {
            m_session.Close();
            m_session = nullptr;
        }
        m_path.clear();
        m_options.clear();
        m_text.clear();

        const auto accountId = Services().modem->NetworkAccountId();
        if (accountId.empty())
        {
            m_path.push_back(code);
            Finish(L"No cellular network account was found.");
            co_return;
        }
        try
        {
            m_session = UssdSession::CreateFromNetworkAccountId(accountId);
        }
        catch (hresult_error const& e)
        {
            m_path.push_back(code);
            Finish(L"Could not start USSD: " + DescribeError(e));
            co_return;
        }
        co_await SendAsync(code);
    }

    winrt::fire_and_forget UssdSessionManager::Reply(std::wstring text)
    {
        if (m_state != UssdState::AwaitingReply || !m_session)
        {
            co_return;
        }
        co_await SendAsync(text);
    }

    void UssdSessionManager::End()
    {
        if (m_state == UssdState::AwaitingReply)
        {
            Finish(m_text.empty() ? std::wstring(L"Session ended.") : m_text);
        }
    }

    Windows::Foundation::IAsyncAction UssdSessionManager::SendAsync(std::wstring text)
    {
        m_path.push_back(text);
        m_options.clear();
        SetState(UssdState::Sending);
        core::Log::Write(L"USSD send '" + text + L"'");
        try
        {
            auto reply = co_await m_session.SendMessageAndGetReplyAsync(UssdMessage{ text });
            HandleReply(reply);
        }
        catch (hresult_error const& e)
        {
            Finish(L"USSD failed: " + DescribeError(e));
        }
    }

    void UssdSessionManager::HandleReply(UssdReply const& reply)
    {
        const auto text = DecodeUssdPayload(reply.Message());
        const auto code = reply.ResultCode();
        core::Log::Write(L"USSD reply code=" + std::to_wstring(static_cast<int>(code)) + L" text='" + text + L"'");
        switch (code)
        {
        case UssdResultCode::ActionRequired:
            m_text = text;
            m_options = core::ParseUssdMenu(text);
            SetState(UssdState::AwaitingReply);
            return;
        case UssdResultCode::NoActionRequired:
            Finish(text.empty() ? std::wstring(L"(The network sent no text.)") : text);
            return;
        case UssdResultCode::Terminated:
            Finish(text.empty() ? std::wstring(L"Session ended by network.") : text + L"\n\nSession ended by network.");
            return;
        case UssdResultCode::OtherLocalClient:
            Finish(L"Another app is using USSD. Close it and try again.");
            return;
        case UssdResultCode::OperationNotSupported:
            Finish(L"Not supported by the network or modem.");
            return;
        case UssdResultCode::NetworkTimeout:
            Finish(L"Network timed out. Dial again.");
            return;
        }
        Finish(text);
    }

    void UssdSessionManager::Finish(std::wstring const& text)
    {
        m_text = text;
        m_options.clear();
        if (m_session)
        {
            m_session.Close();
            m_session = nullptr;
        }
        if (!m_path.empty() && Services().db)
        {
            try
            {
                Services().db->AddUssdHistory(m_path.front(), text, NowMs());
            }
            catch (core::DbError const& e)
            {
                ReportError(L"Saving USSD history failed: " + Widen(e.what()));
            }
        }
        SetState(UssdState::Ended);
    }

    void UssdSessionManager::SetState(UssdState state)
    {
        m_state = state;
        Changed.Raise();
    }

    std::wstring UssdSessionManager::StatusLine() const
    {
        std::wstring path;
        for (size_t i = 0; i < m_path.size(); ++i)
        {
            if (i > 0) path += L" → ";
            path += m_path[i];
        }
        switch (m_state)
        {
        case UssdState::Idle: return L"No active session";
        case UssdState::Sending: return L"Sending " + (m_path.empty() ? std::wstring() : m_path.back()) + L"…";
        case UssdState::AwaitingReply: return L"Session active · step " + std::to_wstring(m_path.size()) + L" · " + path;
        case UssdState::Ended: return L"Ended · " + path;
        }
        return {};
    }
}
```

- [ ] **Step 3: Register the service**

`Services/AppServices.h`: add `#include "Services/UssdSessionManager.h"`; before `// [members]`: `std::unique_ptr<UssdSessionManager> ussd;`.
`Services/AppServices.cpp`: before `// [services]`: `s.ussd = std::make_unique<UssdSessionManager>();`.

- [ ] **Step 4: Replace `UssdPage.xaml`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<Page
    x:Class="WANUtility.UssdPage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    Loaded="Page_Loaded"
    Unloaded="Page_Unloaded">

    <ScrollViewer>
        <StackPanel Padding="24,16,24,24" Spacing="12" MaxWidth="760" HorizontalAlignment="Left">
            <TextBlock Text="USSD" Style="{StaticResource TitleTextBlockStyle}" />

            <ScrollViewer HorizontalScrollMode="Enabled" HorizontalScrollBarVisibility="Auto"
                          VerticalScrollMode="Disabled" VerticalScrollBarVisibility="Disabled">
                <StackPanel x:Name="Chips" Orientation="Horizontal" Spacing="6" Padding="0,0,0,6" />
            </ScrollViewer>

            <Grid ColumnSpacing="8">
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="*" />
                    <ColumnDefinition Width="Auto" />
                    <ColumnDefinition Width="Auto" />
                </Grid.ColumnDefinitions>
                <TextBox x:Name="DialBox" PlaceholderText="Enter a code, e.g. *101#" TextChanged="DialBox_TextChanged" />
                <Button x:Name="DialButton" Grid.Column="1" Content="Dial" Style="{StaticResource AccentButtonStyle}" Click="DialButton_Click" />
                <Button x:Name="SaveCodeButton" Grid.Column="2" Content="Save code" Click="SaveCodeButton_Click" />
            </Grid>

            <Border Background="{ThemeResource CardBackgroundFillColorDefaultBrush}"
                    BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}"
                    BorderThickness="1" CornerRadius="8" Padding="16">
                <StackPanel Spacing="10">
                    <Grid ColumnSpacing="8">
                        <Grid.ColumnDefinitions>
                            <ColumnDefinition Width="*" />
                            <ColumnDefinition Width="Auto" />
                        </Grid.ColumnDefinitions>
                        <TextBlock x:Name="StatusLine" Style="{StaticResource CaptionTextBlockStyle}" TextWrapping="Wrap" />
                        <ProgressRing x:Name="Busy" Grid.Column="1" IsActive="False" Width="16" Height="16" />
                    </Grid>
                    <TextBlock x:Name="ReplyText" TextWrapping="Wrap" IsTextSelectionEnabled="True" />
                    <StackPanel x:Name="Options" Spacing="4" />
                    <Grid x:Name="ReplyRow" ColumnSpacing="8" Visibility="Collapsed">
                        <Grid.ColumnDefinitions>
                            <ColumnDefinition Width="*" />
                            <ColumnDefinition Width="Auto" />
                            <ColumnDefinition Width="Auto" />
                        </Grid.ColumnDefinitions>
                        <TextBox x:Name="ReplyBox" PlaceholderText="Or type a reply" />
                        <Button x:Name="SendReplyButton" Grid.Column="1" Content="Send" Click="SendReplyButton_Click" />
                        <Button x:Name="EndButton" Grid.Column="2" Content="End" Click="EndButton_Click" />
                    </Grid>
                </StackPanel>
            </Border>

            <TextBlock Text="Recent" Style="{StaticResource BodyStrongTextBlockStyle}" Margin="0,8,0,0" />
            <StackPanel x:Name="Recent" Spacing="4" />
        </StackPanel>
    </ScrollViewer>
</Page>
```

- [ ] **Step 5: Replace `UssdPage.xaml.h`**

```cpp
#pragma once

#include "UssdPage.g.h"

namespace winrt::WANUtility::implementation
{
    struct UssdPage : UssdPageT<UssdPage>
    {
        UssdPage() {}

        void Page_Loaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void Page_Unloaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void DialBox_TextChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void DialButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void SendReplyButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void EndButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        fire_and_forget SaveCodeButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        void Render();
        void RenderChips();
        void RenderRecent();

        int m_ussdToken = 0;
        int m_modemToken = 0;
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct UssdPage : UssdPageT<UssdPage, implementation::UssdPage>
    {
    };
}
```

- [ ] **Step 6: Replace `UssdPage.xaml.cpp`**

```cpp
#include "pch.h"
#include "UssdPage.xaml.h"
#if __has_include("UssdPage.g.cpp")
#include "UssdPage.g.cpp"
#endif

#include "Core/TextUtil.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/TimeUtil.h"
#include "Services/UssdSessionManager.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::WANUtility::implementation
{
    void UssdPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        m_ussdToken = s.ussd->Changed.Add([this]
        {
            Render();
            if (wan::Services().ussd->State() == wan::UssdState::Ended) RenderRecent();
        });
        m_modemToken = s.modem->StatusChanged.Add([this] { Render(); });
        RenderChips();
        RenderRecent();
        Render();
    }

    void UssdPage::Page_Unloaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        s.ussd->Changed.Remove(m_ussdToken);
        s.modem->StatusChanged.Remove(m_modemToken);
    }

    void UssdPage::Render()
    {
        auto& s = wan::Services();
        auto& u = *s.ussd;
        const bool ready = s.modem->Status().ready;
        const bool busy = u.State() == wan::UssdState::Sending;
        const bool awaiting = u.State() == wan::UssdState::AwaitingReply;

        StatusLine().Text(u.StatusLine());
        Busy().IsActive(busy);
        ReplyText().Text(u.State() == wan::UssdState::Idle ? std::wstring(L"Dial a code or tap a saved one.") : u.ReplyText());

        Options().Children().Clear();
        if (awaiting)
        {
            for (auto const& option : u.Options())
            {
                Button b;
                b.Content(box_value(hstring(option.key + L" · " + option.label)));
                b.HorizontalAlignment(HorizontalAlignment::Stretch);
                b.HorizontalContentAlignment(HorizontalAlignment::Left);
                b.IsEnabled(ready && !busy);
                auto key = option.key;
                b.Click([key](IInspectable const&, RoutedEventArgs const&) { wan::Services().ussd->Reply(key); });
                Options().Children().Append(b);
            }
        }
        ReplyRow().Visibility(awaiting ? Visibility::Visible : Visibility::Collapsed);
        SendReplyButton().IsEnabled(ready && !busy);

        const bool canDial = ready && !busy;
        DialButton().IsEnabled(canDial && !wan::core::Trim(DialBox().Text()).empty());
        for (auto const& child : Chips().Children())
        {
            child.as<Control>().IsEnabled(canDial);
        }
    }

    void UssdPage::RenderChips()
    {
        Chips().Children().Clear();
        auto& s = wan::Services();
        if (!s.db) return;
        try
        {
            for (auto const& c : s.db->UssdCodes())
            {
                Button chip;
                chip.Content(box_value(hstring(c.label + L"  " + c.code)));
                chip.CornerRadius(CornerRadius{ 14, 14, 14, 14 });
                auto code = c.code;
                chip.Click([this, code](IInspectable const&, RoutedEventArgs const&)
                {
                    DialBox().Text(code);
                    wan::Services().ussd->Dial(code);
                });
                Chips().Children().Append(chip);
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading saved codes failed: " + wan::Widen(e.what()));
        }
    }

    void UssdPage::RenderRecent()
    {
        Recent().Children().Clear();
        auto& s = wan::Services();
        if (!s.db) return;
        try
        {
            for (auto const& h : s.db->RecentUssd(5))
            {
                TextBlock t;
                t.Text(h.code + L" → " + wan::core::FirstLine(h.finalReply) + L" · " + wan::FormatTime(h.timestampMs));
                t.TextTrimming(TextTrimming::CharacterEllipsis);
                t.Opacity(0.8);
                Recent().Children().Append(t);
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading USSD history failed: " + wan::Widen(e.what()));
        }
    }

    void UssdPage::DialBox_TextChanged(IInspectable const&, TextChangedEventArgs const&)
    {
        Render();
    }

    void UssdPage::DialButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto code = wan::core::Trim(DialBox().Text());
        if (code.empty()) return;
        wan::Services().ussd->Dial(code);
    }

    void UssdPage::SendReplyButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto text = wan::core::Trim(ReplyBox().Text());
        if (text.empty()) return;
        ReplyBox().Text(L"");
        wan::Services().ussd->Reply(text);
    }

    void UssdPage::EndButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        wan::Services().ussd->End();
    }

    fire_and_forget UssdPage::SaveCodeButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto strong = get_strong();
        auto code = wan::core::Trim(DialBox().Text());
        if (code.empty() || !wan::Services().db) co_return;

        TextBox label;
        label.PlaceholderText(L"Label, e.g. Balance");
        ContentDialog dialog;
        dialog.XamlRoot(XamlRoot());
        dialog.Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Style>());
        dialog.Title(box_value(hstring(L"Save " + code)));
        dialog.Content(label);
        dialog.PrimaryButtonText(L"Save");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);
        if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) co_return;

        auto text = wan::core::Trim(label.Text());
        try
        {
            wan::Services().db->AddUssdCode(code, text.empty() ? code : text);
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Saving the code failed: " + wan::Widen(e.what()));
        }
        RenderChips();
        Render();
    }
}
```

`WANUtility.vcxproj`: add `<ClInclude Include="Services\UssdSessionManager.h" />`, `<ClCompile Include="Services\UssdSessionManager.cpp" />`.

- [ ] **Step 7: Deploy and verify `*101#` (free)**

Run `scripts\deploy.ps1`. Ask the user to open USSD and tap the "Balance *101#" chip (or do it with them watching).
Expected on screen: status "Ended · *101#", reply `ยอดคงเหลือ…บ. ใช้ได้ถึง…` (Thai, readable), a new line under Recent.
Expected in the log: `USSD reply code=0 text='ยอดคงเหลือ…'`.

- [ ] **Step 8: Verify a multi-step session with `*103#` — NAVIGATE ONLY, NEVER CONFIRM A PURCHASE**

Ask the user before starting. Dial `*103#`.
Record verbatim in the task notes: the first reply text, the result code from the log, and whether option buttons appeared. If the menu has a pure navigation option (e.g. "next page"), it may be tapped once; anything that could subscribe/buy must not be. Then press **End** and confirm status "Ended · *103# …".
If dtac's menu format produces no buttons although it is clearly a menu, capture the exact text, add it as a new failing test in `Tests/UssdMenuParserTests.cpp`, adjust `ParseSegment`, re-run `scripts\test.ps1`, and redeploy.

- [ ] **Step 9: Disabled-state check**

With an empty dial box, Dial is disabled. While "Sending…" is shown, Dial, chips and option buttons are disabled. Confirm with the user.

- [ ] **Step 10: Commit**

```powershell
git add -A
git commit -m "feat(ussd): interactive sessions with parsed menu options and saved codes`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Receive — InboxImporter + MessagesPage threads and conversation

**Files:**
- Create: `Services/InboxImporter.h`, `Services/InboxImporter.cpp`
- Replace: `MessagesPage.xaml`, `MessagesPage.xaml.h`, `MessagesPage.xaml.cpp`
- Modify: `Services/AppServices.h`, `Services/AppServices.cpp`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `core::ScanBatch`, `core::StoreEntry` (Task 6); `MessageDb::HasWindowsStoreId/InsertIncoming/MessageById/GetSetting/SetSetting/Threads/MessagesInThread/MarkThreadRead` (Task 4); `core::IsPhoneNumberKey` (Task 2); `ToUnixMs`, `FormatTime`, `ReportError`, `Services().MessagesChanged/OpenThreadRequested/pendingOpenThread/threadInView/windowFocused`; `ModemService::Status/StatusChanged`.
- Produces: `class wan::InboxImporter { fire_and_forget Start(); fire_and_forget Import(); Signal<core::Message> NewIncoming; }`; member `Services().importer`. `MessagesPage` private methods `RefreshThreads()`, `RefreshConversation()`, `SelectThread(std::wstring const&)`, `UpdateSendEnabled()`, `MakeBubble(core::Message const&)`, members `m_threadKey`, `m_replyAddress`, `m_newMode` — Task 11 extends these.

- [ ] **Step 1: Create `Services/InboxImporter.h`**

```cpp
#pragma once
#include "Core/MessageDb.h"
#include "Services/Signal.h"

namespace wan
{
    // Copies incoming SMS from the Windows chat store into MessageDb.
    // Triggers: store MessageChanged (no details in this SDK), a 60 s timer, and Start().
    class InboxImporter
    {
    public:
        winrt::fire_and_forget Start();
        winrt::fire_and_forget Import();

        // Raised for each newly imported message, except during the very first import.
        Signal<core::Message> NewIncoming;

    private:
        winrt::Windows::Foundation::IAsyncAction ImportAsync();

        winrt::Windows::ApplicationModel::Chat::ChatMessageStore m_store{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
        bool m_running = false;
        bool m_again = false;
    };
}
```

- [ ] **Step 2: Create `Services/InboxImporter.cpp`**

```cpp
#include "pch.h"
#include "Services/InboxImporter.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/TimeUtil.h"
#include "Core/InboxScan.h"
#include "Core/Log.h"

#include <unordered_map>

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Chat;

namespace wan
{
    winrt::fire_and_forget InboxImporter::Start()
    {
        try
        {
            m_store = co_await ChatMessageManager::RequestStoreAsync();
        }
        catch (hresult_error const& e)
        {
            ReportError(L"Cannot read the Windows message store: " + DescribeError(e));
            co_return;
        }
        if (!m_store)
        {
            ReportError(L"Windows returned no message store.");
            co_return;
        }

        // Fires on a worker thread and also without new messages (observed during USSD): just re-import.
        m_store.MessageChanged([](ChatMessageStore const&, ChatMessageChangedEventArgs const&)
        {
            Services().dispatcher.TryEnqueue([] { Services().importer->Import(); });
        });
        // Safety net: MessageChanged was never observed for a genuinely new SMS in the spike.
        m_timer = Services().dispatcher.CreateTimer();
        m_timer.Interval(std::chrono::seconds(60));
        m_timer.Tick([](auto const&, auto const&) { Services().importer->Import(); });
        m_timer.Start();

        co_await ImportAsync();
    }

    winrt::fire_and_forget InboxImporter::Import()
    {
        co_await ImportAsync();
    }

    Windows::Foundation::IAsyncAction InboxImporter::ImportAsync()
    {
        if (!m_store || !Services().db)
        {
            co_return;
        }
        if (m_running)
        {
            m_again = true; // coalesce bursts of triggers into one more pass
            co_return;
        }
        m_running = true;
        do
        {
            m_again = false;
            try
            {
                auto& db = *Services().db;
                const bool firstImport = !db.GetSetting(L"first_import_done").has_value();
                auto isKnown = [&db](std::wstring const& id) { return db.HasWindowsStoreId(id); };

                std::vector<core::StoreEntry> fresh;
                std::unordered_map<std::wstring, ChatMessage> byId;
                auto reader = m_store.GetMessageReader();
                while (true)
                {
                    auto batch = co_await reader.ReadBatchAsync(); // newest first (observed)
                    if (batch.Size() == 0)
                    {
                        break;
                    }
                    std::vector<core::StoreEntry> entries;
                    for (auto const& m : batch)
                    {
                        std::wstring id{ m.Id() };
                        entries.push_back({ id, m.IsIncoming() });
                        byId.emplace(id, m);
                    }
                    if (core::ScanBatch(entries, isKnown, fresh))
                    {
                        break;
                    }
                }

                int inserted = 0;
                for (auto it = fresh.rbegin(); it != fresh.rend(); ++it) // oldest first
                {
                    auto const& m = byId.at(it->id);
                    const auto rowId = db.InsertIncoming(std::wstring(m.From()), std::wstring(m.Body()), ToUnixMs(m.LocalTimestamp()), it->id);
                    if (rowId == 0)
                    {
                        continue;
                    }
                    ++inserted;
                    if (!firstImport)
                    {
                        if (auto saved = db.MessageById(rowId))
                        {
                            NewIncoming.Raise(*saved);
                        }
                    }
                }
                if (firstImport)
                {
                    db.SetSetting(L"first_import_done", L"1");
                }
                if (inserted > 0)
                {
                    core::Log::Write(L"Imported " + std::to_wstring(inserted) + L" message(s)" + (firstImport ? L" (first import, no toasts)" : L""));
                    Services().MessagesChanged.Raise(L"");
                }
            }
            catch (hresult_error const& e)
            {
                ReportError(L"Importing messages failed: " + DescribeError(e));
            }
            catch (core::DbError const& e)
            {
                ReportError(L"Saving imported messages failed: " + Widen(e.what()));
            }
        } while (m_again);
        m_running = false;
    }
}
```

- [ ] **Step 3: Register the service**

`Services/AppServices.h`: add `#include "Services/InboxImporter.h"`; before `// [members]`: `std::unique_ptr<InboxImporter> importer;`.
`Services/AppServices.cpp`: before `// [services]`:

```cpp
        s.importer = std::make_unique<InboxImporter>();
        s.importer->Start(); // suspends at its first co_await; NewIncoming subscribers added later in OnLaunched still see every toast-worthy message
```

`WANUtility.vcxproj`: add `<ClInclude Include="Services\InboxImporter.h" />`, `<ClCompile Include="Services\InboxImporter.cpp" />`.

- [ ] **Step 4: Replace `MessagesPage.xaml`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<Page
    x:Class="WANUtility.MessagesPage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    Loaded="Page_Loaded"
    Unloaded="Page_Unloaded">

    <Page.Resources>
        <Style x:Key="InBubble" TargetType="Border">
            <Setter Property="Background" Value="{ThemeResource CardBackgroundFillColorDefaultBrush}" />
            <Setter Property="BorderBrush" Value="{ThemeResource CardStrokeColorDefaultBrush}" />
            <Setter Property="BorderThickness" Value="1" />
            <Setter Property="CornerRadius" Value="8" />
            <Setter Property="Padding" Value="10,7,10,7" />
            <Setter Property="MaxWidth" Value="480" />
            <Setter Property="HorizontalAlignment" Value="Left" />
        </Style>
        <Style x:Key="OutBubble" TargetType="Border">
            <Setter Property="Background" Value="{ThemeResource AccentFillColorDefaultBrush}" />
            <Setter Property="CornerRadius" Value="8" />
            <Setter Property="Padding" Value="10,7,10,7" />
            <Setter Property="MaxWidth" Value="480" />
            <Setter Property="HorizontalAlignment" Value="Right" />
        </Style>
        <Style x:Key="FailedBubble" TargetType="Border">
            <Setter Property="Background" Value="{ThemeResource CardBackgroundFillColorDefaultBrush}" />
            <Setter Property="BorderBrush" Value="{ThemeResource SystemFillColorCriticalBrush}" />
            <Setter Property="BorderThickness" Value="1" />
            <Setter Property="CornerRadius" Value="8" />
            <Setter Property="Padding" Value="10,7,10,7" />
            <Setter Property="MaxWidth" Value="480" />
            <Setter Property="HorizontalAlignment" Value="Right" />
        </Style>
        <Style x:Key="BubbleText" TargetType="TextBlock">
            <Setter Property="TextWrapping" Value="Wrap" />
            <Setter Property="IsTextSelectionEnabled" Value="True" />
        </Style>
        <Style x:Key="OnAccentText" TargetType="TextBlock" BasedOn="{StaticResource BubbleText}">
            <Setter Property="Foreground" Value="{ThemeResource TextOnAccentFillColorPrimaryBrush}" />
        </Style>
        <Style x:Key="MetaText" TargetType="TextBlock" BasedOn="{StaticResource CaptionTextBlockStyle}">
            <Setter Property="Opacity" Value="0.75" />
        </Style>
        <Style x:Key="OnAccentMetaText" TargetType="TextBlock" BasedOn="{StaticResource MetaText}">
            <Setter Property="Foreground" Value="{ThemeResource TextOnAccentFillColorPrimaryBrush}" />
        </Style>
        <Style x:Key="ErrorText" TargetType="TextBlock" BasedOn="{StaticResource CaptionTextBlockStyle}">
            <Setter Property="Foreground" Value="{ThemeResource SystemFillColorCriticalBrush}" />
            <Setter Property="TextWrapping" Value="Wrap" />
        </Style>
        <Style x:Key="RowName" TargetType="TextBlock" BasedOn="{StaticResource BodyStrongTextBlockStyle}">
            <Setter Property="TextTrimming" Value="CharacterEllipsis" />
        </Style>
        <Style x:Key="RowPreview" TargetType="TextBlock" BasedOn="{StaticResource CaptionTextBlockStyle}">
            <Setter Property="TextTrimming" Value="CharacterEllipsis" />
            <Setter Property="MaxLines" Value="1" />
            <Setter Property="Opacity" Value="0.8" />
        </Style>
    </Page.Resources>

    <Grid>
        <Grid.ColumnDefinitions>
            <ColumnDefinition Width="300" />
            <ColumnDefinition Width="*" />
        </Grid.ColumnDefinitions>

        <Grid Padding="12" RowSpacing="8" BorderThickness="0,0,1,0" BorderBrush="{ThemeResource DividerStrokeColorDefaultBrush}">
            <Grid.RowDefinitions>
                <RowDefinition Height="Auto" />
                <RowDefinition Height="*" />
            </Grid.RowDefinitions>
            <Grid ColumnSpacing="8">
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="*" />
                    <ColumnDefinition Width="Auto" />
                </Grid.ColumnDefinitions>
                <TextBox x:Name="SearchBox" PlaceholderText="Search messages" TextChanged="SearchBox_TextChanged" />
                <Button x:Name="NewButton" Grid.Column="1" ToolTipService.ToolTip="New message">
                    <FontIcon Glyph="&#xE710;" FontSize="14" />
                </Button>
            </Grid>
            <ListView x:Name="ThreadList" Grid.Row="1" SelectionMode="Single" SelectionChanged="ThreadList_SelectionChanged" />
        </Grid>

        <Grid Grid.Column="1" Padding="16" RowSpacing="8">
            <Grid.RowDefinitions>
                <RowDefinition Height="Auto" />
                <RowDefinition Height="*" />
                <RowDefinition Height="Auto" />
            </Grid.RowDefinitions>
            <TextBlock x:Name="ThreadTitle" Style="{StaticResource SubtitleTextBlockStyle}" Text="Select a conversation" />
            <TextBox x:Name="ToBox" PlaceholderText="To: phone number" Visibility="Collapsed" />
            <ScrollViewer x:Name="BubbleScroller" Grid.Row="1">
                <StackPanel x:Name="Bubbles" Spacing="6" Padding="0,0,12,0" />
            </ScrollViewer>
            <Grid Grid.Row="2" ColumnSpacing="8">
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="*" />
                    <ColumnDefinition Width="Auto" />
                </Grid.ColumnDefinitions>
                <TextBox x:Name="ComposeBox" PlaceholderText="Type a message" AcceptsReturn="True" TextWrapping="Wrap" MaxHeight="120" />
                <Button x:Name="SendButton" Grid.Column="1" Content="Send" Style="{StaticResource AccentButtonStyle}" IsEnabled="False" VerticalAlignment="Bottom" />
            </Grid>
        </Grid>
    </Grid>
</Page>
```

- [ ] **Step 5: Replace `MessagesPage.xaml.h`**

```cpp
#pragma once

#include "MessagesPage.g.h"
#include "Core/MessageDb.h"

namespace winrt::WANUtility::implementation
{
    struct MessagesPage : MessagesPageT<MessagesPage>
    {
        MessagesPage() {}

        void Page_Loaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void Page_Unloaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void SearchBox_TextChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void ThreadList_SelectionChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);

    private:
        void RefreshThreads();
        void RefreshConversation();
        void SelectThread(std::wstring const& threadKey);
        void UpdateSendEnabled();
        Microsoft::UI::Xaml::Controls::ListViewItem MakeThreadRow(wan::core::Thread const& thread);
        Microsoft::UI::Xaml::UIElement MakeBubble(wan::core::Message const& message);
        Microsoft::UI::Xaml::Style LocalStyle(wchar_t const* key);

        std::wstring m_threadKey;     // selected thread, empty when none
        std::wstring m_replyAddress;  // address to send replies to (newest message's address)
        bool m_newMode = false;       // composing to a new recipient
        bool m_refreshing = false;    // suppress SelectionChanged while rebuilding the list
        int m_messagesToken = 0;
        int m_openToken = 0;
        int m_modemToken = 0;
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct MessagesPage : MessagesPageT<MessagesPage, implementation::MessagesPage>
    {
    };
}
```

- [ ] **Step 6: Replace `MessagesPage.xaml.cpp`**

```cpp
#include "pch.h"
#include "MessagesPage.xaml.h"
#if __has_include("MessagesPage.g.cpp")
#include "MessagesPage.g.cpp"
#endif

#include "Core/PhoneNumber.h"
#include "Core/TextUtil.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/TimeUtil.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using wan::core::Direction;
using wan::core::MessageStatus;

namespace winrt::WANUtility::implementation
{
    void MessagesPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        m_messagesToken = s.MessagesChanged.Add([this](std::wstring const&)
        {
            RefreshThreads();
            RefreshConversation();
        });
        m_openToken = s.OpenThreadRequested.Add([this](std::wstring const& key)
        {
            wan::Services().pendingOpenThread.clear();
            SelectThread(key);
        });
        m_modemToken = s.modem->StatusChanged.Add([this] { UpdateSendEnabled(); });

        RefreshThreads();
        if (!s.pendingOpenThread.empty())
        {
            auto key = s.pendingOpenThread;
            s.pendingOpenThread.clear();
            SelectThread(key);
        }
        else
        {
            RefreshConversation();
        }
    }

    void MessagesPage::Page_Unloaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        s.MessagesChanged.Remove(m_messagesToken);
        s.OpenThreadRequested.Remove(m_openToken);
        s.modem->StatusChanged.Remove(m_modemToken);
        s.threadInView.clear();
    }

    Style MessagesPage::LocalStyle(wchar_t const* key)
    {
        return Resources().Lookup(box_value(key)).as<Style>();
    }

    void MessagesPage::RefreshThreads()
    {
        auto& s = wan::Services();
        if (!s.db) return;
        std::vector<wan::core::Thread> threads;
        try
        {
            threads = s.db->Threads(wan::core::Trim(SearchBox().Text()));
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading conversations failed: " + wan::Widen(e.what()));
            return;
        }
        m_refreshing = true;
        ThreadList().Items().Clear();
        for (auto const& t : threads)
        {
            auto row = MakeThreadRow(t);
            ThreadList().Items().Append(row);
            if (t.threadKey == m_threadKey)
            {
                ThreadList().SelectedItem(row);
            }
        }
        m_refreshing = false;
    }

    ListViewItem MessagesPage::MakeThreadRow(wan::core::Thread const& t)
    {
        Grid top;
        top.ColumnSpacing(8);
        ColumnDefinition nameCol;
        nameCol.Width(GridLength{ 1, GridUnitType::Star });
        ColumnDefinition timeCol;
        timeCol.Width(GridLength{ 0, GridUnitType::Auto }); // a default ColumnDefinition is "*", not Auto
        top.ColumnDefinitions().Append(nameCol);
        top.ColumnDefinitions().Append(timeCol);
        TextBlock name;
        name.Style(LocalStyle(L"RowName"));
        name.Text(t.displayName);
        TextBlock time;
        time.Style(LocalStyle(L"RowPreview"));
        time.Text(wan::FormatTime(t.lastMessageAtMs));
        Grid::SetColumn(time, 1);
        top.Children().Append(name);
        top.Children().Append(time);

        Grid bottom;
        bottom.ColumnSpacing(8);
        ColumnDefinition previewCol;
        previewCol.Width(GridLength{ 1, GridUnitType::Star });
        ColumnDefinition badgeCol;
        badgeCol.Width(GridLength{ 0, GridUnitType::Auto });
        bottom.ColumnDefinitions().Append(previewCol);
        bottom.ColumnDefinitions().Append(badgeCol);
        TextBlock preview;
        preview.Style(LocalStyle(L"RowPreview"));
        preview.Text(wan::core::FirstLine(t.lastBody));
        bottom.Children().Append(preview);
        if (t.unreadCount > 0)
        {
            InfoBadge badge;
            badge.Value(t.unreadCount);
            Grid::SetColumn(badge, 1);
            bottom.Children().Append(badge);
        }

        StackPanel panel;
        panel.Padding(Thickness{ 0, 6, 0, 6 });
        panel.Spacing(2);
        panel.Children().Append(top);
        panel.Children().Append(bottom);

        ListViewItem item;
        item.Tag(box_value(hstring(t.threadKey)));
        item.Content(panel);
        return item;
    }

    void MessagesPage::SelectThread(std::wstring const& threadKey)
    {
        m_threadKey = threadKey;
        m_newMode = false;
        RefreshThreads();
        RefreshConversation();
    }

    void MessagesPage::ThreadList_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        if (m_refreshing) return;
        auto item = ThreadList().SelectedItem().try_as<ListViewItem>();
        if (!item) return;
        m_threadKey = unbox_value<hstring>(item.Tag());
        m_newMode = false;
        RefreshConversation();
    }

    void MessagesPage::SearchBox_TextChanged(IInspectable const&, TextChangedEventArgs const&)
    {
        RefreshThreads();
    }

    void MessagesPage::RefreshConversation()
    {
        auto& s = wan::Services();
        Bubbles().Children().Clear();
        m_replyAddress.clear();

        ToBox().Visibility(m_newMode ? Visibility::Visible : Visibility::Collapsed);
        ThreadTitle().Visibility(m_newMode ? Visibility::Collapsed : Visibility::Visible);
        s.threadInView = m_newMode ? std::wstring() : m_threadKey;

        if (m_newMode || m_threadKey.empty() || !s.db)
        {
            if (!m_newMode) ThreadTitle().Text(L"Select a conversation");
            UpdateSendEnabled();
            return;
        }

        std::vector<wan::core::Message> messages;
        try
        {
            messages = s.db->MessagesInThread(m_threadKey);
            if (s.windowFocused && s.db->MarkThreadRead(m_threadKey) > 0)
            {
                RefreshThreads(); // clear the unread badge
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading the conversation failed: " + wan::Widen(e.what()));
            return;
        }

        if (!messages.empty())
        {
            m_replyAddress = messages.back().address;
        }
        ThreadTitle().Text(m_replyAddress.empty() ? m_threadKey : m_replyAddress);
        for (auto const& m : messages)
        {
            Bubbles().Children().Append(MakeBubble(m));
        }
        BubbleScroller().UpdateLayout();
        BubbleScroller().ChangeView(nullptr, BubbleScroller().ScrollableHeight(), nullptr);
        UpdateSendEnabled();
    }

    UIElement MessagesPage::MakeBubble(wan::core::Message const& m)
    {
        const bool out = m.direction == Direction::Out;
        const bool failed = m.status == MessageStatus::Failed;
        const bool onAccent = out && !failed;

        StackPanel content;
        content.Spacing(2);

        TextBlock body;
        body.Style(LocalStyle(onAccent ? L"OnAccentText" : L"BubbleText"));
        body.Text(m.body);
        content.Children().Append(body);

        std::wstring meta = wan::FormatTime(m.timestampMs);
        if (m.status == MessageStatus::Sending) meta += L" · Sending…";
        else if (failed) meta += L" · Not sent";
        TextBlock metaText;
        metaText.Style(LocalStyle(onAccent ? L"OnAccentMetaText" : L"MetaText"));
        metaText.Text(meta);
        content.Children().Append(metaText);

        if (failed)
        {
            TextBlock error;
            error.Style(LocalStyle(L"ErrorText"));
            error.Text(m.errorDetail);
            content.Children().Append(error);
            // [retry-button] (Task 11)
        }

        Border bubble;
        bubble.Style(LocalStyle(failed ? L"FailedBubble" : (out ? L"OutBubble" : L"InBubble")));
        bubble.Child(content);
        return bubble;
    }

    void MessagesPage::UpdateSendEnabled()
    {
        // Task 11 implements sending; until then the Send button stays disabled.
        SendButton().IsEnabled(false);
    }
}
```

- [ ] **Step 7: Deploy and verify history import**

Run `scripts\deploy.ps1`. Then:

```powershell
Get-Content "$env:LOCALAPPDATA\Packages\29d4b340-cdbf-4ef2-8a8c-cb0bc4b163e0_gpfrfcskskzsw\LocalState\logs\wanutility.log" -Encoding UTF8 | Select-String "Imported"
```
Expected: one line `Imported N message(s) (first import, no toasts)` with N ≥ 20 (the spike saw at least 20 dtac messages), and no toast. Ask the user to confirm the Messages page lists threads `dtac`, `dtacOnline`, `dtacreward` with Thai previews and times, and that opening `dtac` shows readable bubbles, newest at the bottom, and clears its unread badge.

Record what `From()` looked like for any numeric sender, if present (spec risk 3: "+66…" vs "0…").

- [ ] **Step 8: Verify live receive (free)**

Leave the app open on the Messages page. On USSD, dial `*101*1#` (status SMS). Within ~2 minutes a new message must appear in the `dtac` thread without restarting.
Then check the log for the trigger: an `Imported 1 message(s)` line. Note in CLAUDE.md whether it came within seconds of arrival (MessageChanged works) or up to 60 s later (only the timer caught it). This closes spec risk 1 with an observation.

- [ ] **Step 9: Commit**

```powershell
git add -A
git commit -m "feat(messages): import SMS from the Windows store and show conversation threads`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: Send — SmsSender + compose, new message, retry

**Files:**
- Create: `Services/SmsSender.h`, `Services/SmsSender.cpp`
- Modify: `MessagesPage.xaml`, `MessagesPage.xaml.h`, `MessagesPage.xaml.cpp`, `Services/AppServices.h`, `Services/AppServices.cpp`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `MessageDb::InsertOutgoing/MarkSending/MarkSent/MarkFailed/MessageById` (Task 4), `ModemService::SmsDevice/Status` (Task 8), `core::ThreadKeyFor/IsPhoneNumberKey` (Task 2), `MessagesPage` members from Task 10.
- Produces: `class wan::SmsSender { fire_and_forget Send(std::wstring address, std::wstring body); fire_and_forget Retry(int64_t id); }`; `std::wstring wan::DescribeSendFailure(SmsSendMessageResult const&)`; member `Services().sms`. Marker `// [after-sent]` in `SmsSender::TransmitAsync` (Task 12 inserts there).

- [ ] **Step 1: Create `Services/SmsSender.h`**

```cpp
#pragma once
#include <string>

namespace wan
{
    // Send flow: save as "sending" -> transmit -> "sent"/"failed" -> (Task 12) copy to Windows store.
    class SmsSender
    {
    public:
        winrt::fire_and_forget Send(std::wstring address, std::wstring body);
        winrt::fire_and_forget Retry(int64_t messageId);

    private:
        winrt::Windows::Foundation::IAsyncAction TransmitAsync(int64_t messageId);
    };

    std::wstring DescribeSendFailure(winrt::Windows::Devices::Sms::SmsSendMessageResult const& result);
}
```

- [ ] **Step 2: Create `Services/SmsSender.cpp`**

```cpp
#include "pch.h"
#include "Services/SmsSender.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/TimeUtil.h"
#include "Core/Log.h"
#include "Core/PhoneNumber.h"

using namespace winrt;
using namespace winrt::Windows::Devices::Sms;

namespace wan
{
    std::wstring DescribeSendFailure(SmsSendMessageResult const& r)
    {
        return L"Modem error " + std::to_wstring(static_cast<int>(r.ModemErrorCode())) +
               L", network cause " + std::to_wstring(r.NetworkCauseCode()) +
               L", transport cause " + std::to_wstring(r.TransportFailureCause()) +
               (r.IsErrorTransient() ? L" (temporary — try again)" : L"");
    }

    winrt::fire_and_forget SmsSender::Send(std::wstring address, std::wstring body)
    {
        auto& s = Services();
        if (!s.db) co_return;
        int64_t id = 0;
        try
        {
            id = s.db->InsertOutgoing(address, body, NowMs());
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Saving the message failed: " + Widen(e.what()));
            co_return;
        }
        s.MessagesChanged.Raise(core::ThreadKeyFor(address));
        co_await TransmitAsync(id);
    }

    winrt::fire_and_forget SmsSender::Retry(int64_t messageId)
    {
        auto& s = Services();
        if (!s.db) co_return;
        std::wstring threadKey;
        try
        {
            s.db->MarkSending(messageId);
            if (auto m = s.db->MessageById(messageId)) threadKey = m->threadKey;
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Retrying failed: " + Widen(e.what()));
            co_return;
        }
        s.MessagesChanged.Raise(threadKey);
        co_await TransmitAsync(messageId);
    }

    Windows::Foundation::IAsyncAction SmsSender::TransmitAsync(int64_t messageId)
    {
        auto& s = Services();
        std::optional<core::Message> message;
        try
        {
            message = s.db->MessageById(messageId);
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Reading the message failed: " + Widen(e.what()));
            co_return;
        }
        if (!message) co_return;

        std::wstring failure;
        auto device = s.modem->SmsDevice();
        if (!device)
        {
            failure = L"No cellular modem was found.";
        }
        else if (!s.modem->Status().ready)
        {
            failure = s.modem->Status().problem;
        }
        else
        {
            try
            {
                SmsTextMessage2 text;
                text.To(message->address);
                text.Body(message->body);
                auto result = co_await device.SendMessageAndGetResultAsync(text);
                if (!result.IsSuccessful())
                {
                    failure = DescribeSendFailure(result);
                }
            }
            catch (hresult_error const& e)
            {
                failure = DescribeError(e);
            }
        }

        try
        {
            if (failure.empty()) s.db->MarkSent(messageId);
            else s.db->MarkFailed(messageId, failure);
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Updating the message failed: " + Widen(e.what()));
        }
        core::Log::Write(L"SMS " + std::to_wstring(messageId) + L" to " + message->address + L" (" +
                         std::to_wstring(message->body.size()) + L" chars): " + (failure.empty() ? std::wstring(L"sent") : L"FAILED " + failure));
        s.MessagesChanged.Raise(message->threadKey);

        if (failure.empty())
        {
            // [after-sent]
        }
    }
}
```

- [ ] **Step 3: Register the service**

`Services/AppServices.h`: add `#include "Services/SmsSender.h"`; before `// [members]`: `std::unique_ptr<SmsSender> sms;`.
`Services/AppServices.cpp`: before `// [services]`: `s.sms = std::make_unique<SmsSender>();`.
`WANUtility.vcxproj`: add `<ClInclude Include="Services\SmsSender.h" />`, `<ClCompile Include="Services\SmsSender.cpp" />`.

- [ ] **Step 4: Wire the compose UI in `MessagesPage.xaml`**

Replace the `NewButton` opening tag with:
```xml
                <Button x:Name="NewButton" Grid.Column="1" ToolTipService.ToolTip="New message" Click="NewButton_Click">
```
Replace the `ToBox` line with:
```xml
            <TextBox x:Name="ToBox" PlaceholderText="To: phone number" Visibility="Collapsed" TextChanged="ComposeInput_TextChanged" />
```
Replace the `ComposeBox` line with:
```xml
                <TextBox x:Name="ComposeBox" PlaceholderText="Type a message" AcceptsReturn="True" TextWrapping="Wrap" MaxHeight="120" TextChanged="ComposeInput_TextChanged" />
```
Replace the `SendButton` line with:
```xml
                <Button x:Name="SendButton" Grid.Column="1" Content="Send" Style="{StaticResource AccentButtonStyle}" IsEnabled="False" VerticalAlignment="Bottom" Click="SendButton_Click" />
```

- [ ] **Step 5: Add handlers to `MessagesPage.xaml.h`** (public section, after `ThreadList_SelectionChanged`)

```cpp
        void NewButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void ComposeInput_TextChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void SendButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
```

- [ ] **Step 6: Implement them in `MessagesPage.xaml.cpp`**

Add `#include "Services/SmsSender.h"` to the includes. Replace the whole `UpdateSendEnabled` method with:

```cpp
    void MessagesPage::UpdateSendEnabled()
    {
        auto& s = wan::Services();
        const bool ready = s.db && s.modem->Status().ready;
        const bool hasRecipient = m_newMode
            ? wan::core::IsPhoneNumberKey(wan::core::ThreadKeyFor(ToBox().Text()))
            : (!m_replyAddress.empty() && wan::core::IsPhoneNumberKey(m_threadKey)); // "dtac" cannot receive SMS
        const bool hasBody = !wan::core::Trim(ComposeBox().Text()).empty();
        SendButton().IsEnabled(ready && hasRecipient && hasBody);
    }

    void MessagesPage::NewButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_newMode = true;
        m_threadKey.clear();
        m_refreshing = true;
        ThreadList().SelectedItem(nullptr);
        m_refreshing = false;
        ToBox().Text(L"");
        RefreshConversation();
        ToBox().Focus(FocusState::Programmatic);
    }

    void MessagesPage::ComposeInput_TextChanged(IInspectable const&, TextChangedEventArgs const&)
    {
        UpdateSendEnabled();
    }

    void MessagesPage::SendButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        const auto body = wan::core::Trim(ComposeBox().Text());
        const auto address = m_newMode ? wan::core::Trim(ToBox().Text()) : m_replyAddress;
        if (body.empty() || address.empty()) return;

        ComposeBox().Text(L"");
        m_newMode = false;
        m_threadKey = wan::core::ThreadKeyFor(address);
        // Send() inserts the "sending" row before its first suspension, so the refresh below shows it.
        wan::Services().sms->Send(address, body);
        RefreshThreads();
        RefreshConversation();
    }
```

In `MakeBubble`, replace the line `// [retry-button] (Task 11)` with:

```cpp
            HyperlinkButton retry;
            retry.Content(box_value(L"Retry"));
            retry.Padding(Thickness{ 0, 0, 0, 0 });
            const int64_t id = m.id;
            retry.Click([id](IInspectable const&, RoutedEventArgs const&) { wan::Services().sms->Retry(id); });
            content.Children().Append(retry);
```

- [ ] **Step 7: Deploy; disabled-state check (free)**

Run `scripts\deploy.ps1`. With the user, confirm:
- `dtac` thread selected → Send stays disabled even with text (cannot reply to an alphanumeric sender).
- New message with `To:` empty or only spaces → disabled; `To: 088-960-5613` + body → enabled.
- Body only spaces → disabled.

- [ ] **Step 8: Send one real SMS — ASK THE USER FIRST (costs ~1 SMS)**

With explicit OK: New message → To `0889605613` → body `WANUtility v1 test` → Send.
Expected: bubble appears immediately as "Sending…", turns into a sent accent bubble; the thread key is `+66889605613` (same thread as any reply from that number); log line `SMS <id> to 0889605613 (18 chars): sent`. Ask the user to confirm the phone received it.

- [ ] **Step 9: Long Thai SMS — ASK THE USER FIRST (costs 2–3 SMS segments)**

Review Focus item 5. With explicit OK, send to `0889605613` a Thai body longer than 70 characters, e.g. `ทดสอบข้อความภาษาไทยยาวเกินเจ็ดสิบตัวอักษรเพื่อดูว่าข้อความถูกแบ่งส่งและรวมกลับครบถ้วนหรือไม่ 1234567890`.
Expected: either "sent" and the phone shows the whole text as one message, or a failed bubble with error codes. Record which. If it is "sent" but the phone shows truncated text, stop and report — that is a silent-truncation bug that needs a decision (e.g. block > 70 chars with a visible message).

- [ ] **Step 10: Failure path (free)**

With the user's OK, turn on airplane mode, try to send (Send should already be disabled by the modem bar — confirm). This verifies that sending cannot start while the modem is not ready. Turn airplane mode off.

- [ ] **Step 11: Commit**

```powershell
git add -A
git commit -m "feat(messages): compose, send, new message and retry`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 12: WindowsStoreSync — copy sent messages into the Windows message store

**Files:**
- Create: `Services/WindowsStoreSync.h`, `Services/WindowsStoreSync.cpp`
- Modify: `Services/SmsSender.cpp`, `Services/AppServices.h`, `Services/AppServices.cpp`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `MessageDb::PendingSync/MarkSynced/DisableSync/GetSetting` (Task 4), `FromUnixMs`.
- Produces: `class wan::WindowsStoreSync { fire_and_forget Start(); fire_and_forget SyncPending(); bool Enabled() const; std::wstring DisabledReason() const; Signal<> StateChanged; }`; member `Services().storeSync`. Used by SettingsPage (Task 15).

- [ ] **Step 1: Create `Services/WindowsStoreSync.h`**

```cpp
#pragma once
#include "Services/Signal.h"

#include <string>

namespace wan
{
    // Copies sent SMS into the Windows chat store. Transient failures stay "pending" and are retried
    // at start and every 5 minutes; an access-denied failure disables sync permanently (never retried).
    class WindowsStoreSync
    {
    public:
        winrt::fire_and_forget Start();
        winrt::fire_and_forget SyncPending();

        bool Enabled() const;
        std::wstring DisabledReason() const;

        Signal<> StateChanged; // raised after sync state or pending count may have changed

    private:
        winrt::Windows::Foundation::IAsyncAction SyncAsync();

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
        bool m_running = false;
    };
}
```

- [ ] **Step 2: Create `Services/WindowsStoreSync.cpp`**

```cpp
#include "pch.h"
#include "Services/WindowsStoreSync.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/TimeUtil.h"
#include "Core/Log.h"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Chat;

namespace wan
{
    namespace
    {
        // E_ACCESSDENIED, and NTSTATUS STATUS_ACCESS_DENIED as returned by SmsMessageRegistration in the spike.
        bool IsPermanentDenial(hresult code)
        {
            return code == E_ACCESSDENIED || static_cast<uint32_t>(code.value) == 0xD0000022u;
        }
    }

    bool WindowsStoreSync::Enabled() const
    {
        auto& s = Services();
        return s.db && s.db->GetSetting(L"store_sync_enabled").value_or(L"1") != L"0";
    }

    std::wstring WindowsStoreSync::DisabledReason() const
    {
        auto& s = Services();
        return s.db ? s.db->GetSetting(L"store_sync_disabled_reason").value_or(L"") : std::wstring();
    }

    winrt::fire_and_forget WindowsStoreSync::Start()
    {
        m_timer = Services().dispatcher.CreateTimer();
        m_timer.Interval(std::chrono::minutes(5));
        m_timer.Tick([](auto const&, auto const&) { Services().storeSync->SyncPending(); });
        m_timer.Start();
        co_await SyncAsync();
    }

    winrt::fire_and_forget WindowsStoreSync::SyncPending()
    {
        co_await SyncAsync();
    }

    Windows::Foundation::IAsyncAction WindowsStoreSync::SyncAsync()
    {
        auto& s = Services();
        if (m_running || !Enabled())
        {
            co_return;
        }
        m_running = true;
        try
        {
            auto pending = s.db->PendingSync();
            if (!pending.empty())
            {
                hstring transportId;
                for (auto const& transport : co_await ChatMessageManager::GetTransportsAsync())
                {
                    if (transport.TransportKind() == ChatMessageTransportKind::Text)
                    {
                        transportId = transport.TransportId();
                        break;
                    }
                }
                if (transportId.empty())
                {
                    s.db->DisableSync(L"Windows has no SMS transport in its message store.");
                    core::Log::Write(L"Store sync disabled: no SMS transport");
                }
                else
                {
                    auto store = co_await ChatMessageManager::RequestStoreAsync();
                    for (auto const& m : pending)
                    {
                        ChatMessage cm;
                        cm.TransportId(transportId);
                        cm.Body(m.body);
                        cm.Recipients().Append(m.address);
                        cm.IsIncoming(false);
                        cm.IsRead(true);
                        cm.MessageOperatorKind(ChatMessageOperatorKind::Sms);
                        cm.Status(ChatMessageStatus::Sent);
                        cm.LocalTimestamp(FromUnixMs(m.timestampMs));
                        cm.NetworkTimestamp(FromUnixMs(m.timestampMs));
                        co_await store.SaveMessageAsync(cm);
                        const std::wstring storeId{ cm.Id() };
                        s.db->MarkSynced(m.id, storeId);
                        core::Log::Write(L"Synced message " + std::to_wstring(m.id) + L" to Windows store as '" + storeId + L"'");
                    }
                }
            }
        }
        catch (hresult_error const& e)
        {
            if (IsPermanentDenial(e.code()))
            {
                try
                {
                    s.db->DisableSync(L"Windows denied access to its message store (" + DescribeError(e) + L").");
                }
                catch (core::DbError const& dbError)
                {
                    ReportError(L"Saving sync state failed: " + Widen(dbError.what()));
                }
                core::Log::Write(L"Store sync disabled: " + DescribeError(e));
            }
            else
            {
                core::Log::Write(L"Store sync will retry later: " + DescribeError(e));
            }
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Store sync failed: " + Widen(e.what()));
        }
        m_running = false;
        StateChanged.Raise();
    }
}
```

- [ ] **Step 3: Register and trigger after each send**

`Services/AppServices.h`: add `#include "Services/WindowsStoreSync.h"`; before `// [members]`: `std::unique_ptr<WindowsStoreSync> storeSync;`.
`Services/AppServices.cpp`: before `// [services]`:

```cpp
        s.storeSync = std::make_unique<WindowsStoreSync>();
        s.storeSync->Start();
```

`Services/SmsSender.cpp`: add `#include "Services/WindowsStoreSync.h"`; replace `// [after-sent]` with:

```cpp
            s.storeSync->SyncPending();
```

`WANUtility.vcxproj`: add `<ClInclude Include="Services\WindowsStoreSync.h" />`, `<ClCompile Include="Services\WindowsStoreSync.cpp" />`.

- [ ] **Step 4: Deploy and observe the sync outcome**

Run `scripts\deploy.ps1`. The SMS sent in Task 11 is `pending`; `Start()` syncs it.

```powershell
Get-Content "$env:LOCALAPPDATA\Packages\29d4b340-cdbf-4ef2-8a8c-cb0bc4b163e0_gpfrfcskskzsw\LocalState\logs\wanutility.log" -Encoding UTF8 | Select-String "Synced message|Store sync"
```
Expected: exactly one of
- `Synced message <id> to Windows store as '<id>'` — writing works (record whether the store id is non-empty; if empty, `MarkSynced` stores NULL, which is supported), or
- `Store sync disabled: …` — permanently denied; the app keeps working from its own DB, or
- `Store sync will retry later: …` — transient; record the code.

Record the observed outcome in CLAUDE.md (closes spec risk 2). Also confirm the outgoing message did NOT reappear as a duplicate in the thread after the next import (the importer skips outgoing entries).

- [ ] **Step 5: Commit**

```powershell
git add -A
git commit -m "feat(sync): copy sent SMS into the Windows message store with safe fallback`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 13: Tray icon + close-to-tray + Exit

**Files:**
- Create: `scripts/make-tray-icon.py`, `Assets/Tray.ico` (generated), `Services/TrayIcon.h`, `Services/TrayIcon.cpp`
- Modify: `App.xaml.cpp`, `MainWindow.xaml.h`, `Services/AppServices.h`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `App::ShowMainWindow()`, `App::ExitApp()` (Task 7), markers `// [app-init]`, `// [exit]`.
- Produces: `class wan::TrayIcon { void Create(std::wstring const& iconPath, std::wstring const& tooltip, std::function<void()> onOpen, std::function<void()> onExit); void Remove(); }`; member `Services().tray`.

- [ ] **Step 1: Create `scripts/make-tray-icon.py`**

```python
"""Wraps a PNG into a single-image .ico (PNG-compressed ICO entries are supported since Windows Vista).

Usage: python scripts/make-tray-icon.py <input.png> <output.ico>
"""
import pathlib
import struct
import sys

src, dst = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
png = src.read_bytes()
if png[:8] != b"\x89PNG\r\n\x1a\n":
    sys.exit(f"{src} is not a PNG")
width, height = struct.unpack(">II", png[16:24])
if width > 256 or height > 256:
    sys.exit("icon images must be at most 256x256")
header = struct.pack("<HHH", 0, 1, 1)  # reserved, type 1 = icon, 1 image
entry = struct.pack("<BBBBHHII", width % 256, height % 256, 0, 0, 1, 32, len(png), 6 + 16)
dst.write_bytes(header + entry + png)
print(f"{dst}: {width}x{height}, {len(png)} bytes of PNG")
```

- [ ] **Step 2: Generate the icon**

Run: `python scripts/make-tray-icon.py Assets/Square44x44Logo.targetsize-24_altform-unplated.png Assets/Tray.ico`
Expected: `Assets\Tray.ico: 24x24, … bytes of PNG`.

Add to `WANUtility.vcxproj` (new `<ItemGroup>` or the one with the `Image` items):

```xml
    <None Include="Assets\Tray.ico">
      <DeploymentContent>true</DeploymentContent>
    </None>
```

- [ ] **Step 3: Create `Services/TrayIcon.h`**

```cpp
#pragma once
#include <functional>
#include <string>

namespace wan
{
    // Notification-area icon. Left click -> onOpen; right click -> menu (Open / Exit).
    // Lives on the UI thread: its hidden window's messages are pumped by the XAML message loop.
    class TrayIcon
    {
    public:
        void Create(std::wstring const& iconPath, std::wstring const& tooltip, std::function<void()> onOpen, std::function<void()> onExit);
        void Remove();

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
        void Add();
        void ShowMenu(int x, int y);

        HWND m_hwnd{};
        HICON m_icon{};
        UINT m_taskbarCreated{};
        std::wstring m_tooltip;
        std::function<void()> m_onOpen;
        std::function<void()> m_onExit;
    };
}
```

- [ ] **Step 4: Create `Services/TrayIcon.cpp`**

```cpp
#include "pch.h"
#include "Services/TrayIcon.h"
#include "Core/Log.h"

namespace wan
{
    namespace
    {
        constexpr UINT WM_TRAY = WM_APP + 1;
        constexpr UINT kOpen = 1001;
        constexpr UINT kExit = 1002;
        constexpr wchar_t kClassName[] = L"WANUtilityTrayWindow";
    }

    void TrayIcon::Create(std::wstring const& iconPath, std::wstring const& tooltip, std::function<void()> onOpen, std::function<void()> onExit)
    {
        m_tooltip = tooltip;
        m_onOpen = std::move(onOpen);
        m_onExit = std::move(onExit);

        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = &TrayIcon::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassExW(&wc);

        // A hidden top-level window rather than HWND_MESSAGE: TrackPopupMenu needs a window that can
        // become foreground, otherwise the menu does not close when the user clicks elsewhere.
        m_hwnd = CreateWindowExW(0, kClassName, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, this);
        m_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

        m_icon = static_cast<HICON>(LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE));
        if (!m_icon)
        {
            core::Log::Write(L"Tray icon could not be loaded from " + iconPath + L"; using the default icon");
            m_icon = LoadIconW(nullptr, IDI_APPLICATION);
        }
        Add();
    }

    void TrayIcon::Add()
    {
        NOTIFYICONDATAW nid{ sizeof(nid) };
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        nid.uCallbackMessage = WM_TRAY;
        nid.hIcon = m_icon;
        wcsncpy_s(nid.szTip, m_tooltip.c_str(), _TRUNCATE);
        if (!Shell_NotifyIconW(NIM_ADD, &nid))
        {
            core::Log::Write(L"Shell_NotifyIcon(NIM_ADD) failed");
        }
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }

    void TrayIcon::Remove()
    {
        if (!m_hwnd)
        {
            return;
        }
        NOTIFYICONDATAW nid{ sizeof(nid) };
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    void TrayIcon::ShowMenu(int x, int y)
    {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, kOpen, L"Open WANUtility");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kExit, L"Exit");
        SetForegroundWindow(m_hwnd);
        const UINT command = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, x, y, 0, m_hwnd, nullptr));
        PostMessageW(m_hwnd, WM_NULL, 0, 0);
        DestroyMenu(menu);
        if (command == kOpen) m_onOpen();
        else if (command == kExit) m_onExit();
    }

    LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto self = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
        {
            if (message == WM_TRAY)
            {
                switch (LOWORD(lParam)) // NOTIFYICON_VERSION_4: event in LOWORD(lParam), anchor in wParam
                {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                    self->m_onOpen();
                    return 0;
                case WM_CONTEXTMENU:
                    self->ShowMenu(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
                    return 0;
                }
            }
            else if (message == self->m_taskbarCreated)
            {
                self->Add(); // Explorer restarted: the icon must be added again
                return 0;
            }
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}
```

- [ ] **Step 5: Wire it into the app**

`Services/AppServices.h`: add `#include "Services/TrayIcon.h"`; before `// [members]`: `std::unique_ptr<TrayIcon> tray;`.

`App.xaml.cpp`: insert before `// [app-init]`:

```cpp
        auto& services = wan::Services();
        services.tray = std::make_unique<wan::TrayIcon>();
        services.tray->Create(std::wstring(Windows::ApplicationModel::Package::Current().InstalledLocation().Path()) + L"\\Assets\\Tray.ico",
                              L"WANUtility",
                              [this] { ShowMainWindow(); },
                              [this] { ExitApp(); });
```

and before `// [exit]`:

```cpp
        if (wan::Services().tray) wan::Services().tray->Remove();
```

`MainWindow.xaml.h`: change `bool m_closeToTray = false; // Task 13 turns this on` to `bool m_closeToTray = true;`.

`WANUtility.vcxproj`: add `<ClInclude Include="Services\TrayIcon.h" />`, `<ClCompile Include="Services\TrayIcon.cpp" />`.

- [ ] **Step 6: Deploy and verify**

Run `scripts\deploy.ps1`, then:

```powershell
Test-Path x64\Debug\WANUtility\Assets\Tray.ico
Get-Content "$env:LOCALAPPDATA\Packages\29d4b340-cdbf-4ef2-8a8c-cb0bc4b163e0_gpfrfcskskzsw\LocalState\logs\wanutility.log" -Encoding UTF8 -Tail 20 | Select-String "Tray|NIM_ADD"
```
Expected: `True`; no "could not be loaded" or "NIM_ADD failed" lines.

With the user watching:
1. Click ✕ → window disappears; `(Get-Process WANUtility).Count` → `1`; the tray icon is visible (it may sit in the overflow ^ area).
2. Left-click the tray icon → window returns in front.
3. Close again; launch from Start → window returns (single-instance redirect); still 1 process.
4. Right-click tray icon → menu "Open WANUtility / Exit"; click elsewhere → menu closes.
5. Right-click → Exit → `Get-Process WANUtility -ErrorAction SilentlyContinue` returns nothing; log ends with `Exit requested`.

- [ ] **Step 7: Commit**

```powershell
git add -A
git commit -m "feat(tray): close to tray with Open/Exit menu`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 14: Toast notifications for new SMS

**Files:**
- Create: `Services/Notifier.h`, `Services/Notifier.cpp`
- Modify: `Package.appxmanifest`, `App.xaml.cpp`, `Services/AppServices.h`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `InboxImporter::NewIncoming` (Task 10), `App::ShowThread` (Task 7), `Services().windowFocused/threadInView`, markers `// [app-init]`, `// [activation]`, `// [exit]`.
- Produces: `class wan::Notifier { void Initialize(std::function<void(std::wstring const&)> openThread); void ShowIncoming(core::Message const&); void Shutdown(); static std::wstring ThreadFromArguments(IMap<hstring, hstring> const&); }`; member `Services().notifier`.

- [ ] **Step 1: Declare toast activation in `Package.appxmanifest`**

In the `<Package …>` start tag, add the two namespaces and extend `IgnorableNamespaces`:

```xml
  xmlns:com="http://schemas.microsoft.com/appx/manifest/com/windows10"
  xmlns:desktop="http://schemas.microsoft.com/appx/manifest/desktop/windows10"
  IgnorableNamespaces="uap rescap systemai com desktop">
```
(replacing the existing `IgnorableNamespaces="uap rescap systemai">` line).

Inside `<Application …>`, directly after `</uap:VisualElements>`, add:

```xml
      <Extensions>
        <!-- Toast clicks activate this COM class; Windows App SDK routes it to AppNotificationManager. -->
        <desktop:Extension Category="windows.toastNotificationActivation">
          <desktop:ToastNotificationActivation ToastActivatorCLSID="2FC8F360-452A-445C-B838-93152DE181DC" />
        </desktop:Extension>
        <com:Extension Category="windows.comServer">
          <com:ComServer>
            <com:ExeServer Executable="WANUtility.exe" DisplayName="WANUtility" Arguments="----AppNotificationActivated:">
              <com:Class Id="2FC8F360-452A-445C-B838-93152DE181DC" />
            </com:ExeServer>
          </com:ComServer>
        </com:Extension>
      </Extensions>
```

- [ ] **Step 2: Create `Services/Notifier.h`**

```cpp
#pragma once
#include "Core/MessageDb.h"

#include <functional>
#include <string>

namespace wan
{
    class Notifier
    {
    public:
        // openThread runs on the UI thread with the clicked toast's thread key.
        void Initialize(std::function<void(std::wstring const&)> openThread);
        void ShowIncoming(core::Message const& message);
        void Shutdown();

        static std::wstring ThreadFromArguments(winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> const& arguments);

    private:
        bool m_registered = false;
    };
}
```

- [ ] **Step 3: Create `Services/Notifier.cpp`**

```cpp
#include "pch.h"
#include "Services/Notifier.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Core/Log.h"

using namespace winrt;
using namespace winrt::Microsoft::Windows::AppNotifications;
using namespace winrt::Microsoft::Windows::AppNotifications::Builder;

namespace wan
{
    std::wstring Notifier::ThreadFromArguments(Windows::Foundation::Collections::IMap<hstring, hstring> const& arguments)
    {
        return arguments && arguments.HasKey(L"thread") ? std::wstring(arguments.Lookup(L"thread")) : std::wstring();
    }

    void Notifier::Initialize(std::function<void(std::wstring const&)> openThread)
    {
        auto manager = AppNotificationManager::Default();
        // Subscribe before Register(). Raised on a worker thread when a toast is clicked while we run.
        manager.NotificationInvoked([openThread](AppNotificationManager const&, AppNotificationActivatedEventArgs const& args)
        {
            auto key = ThreadFromArguments(args.Arguments());
            Services().dispatcher.TryEnqueue([openThread, key] { openThread(key); });
        });
        try
        {
            manager.Register();
            m_registered = true;
        }
        catch (hresult_error const& e)
        {
            ReportError(L"Notifications are unavailable: " + DescribeError(e));
        }
    }

    void Notifier::ShowIncoming(core::Message const& message)
    {
        auto& s = Services();
        if (!m_registered)
        {
            return;
        }
        if (s.windowFocused && s.threadInView == message.threadKey)
        {
            return; // the user is already looking at this conversation
        }
        const std::wstring body = message.body.size() > 200 ? message.body.substr(0, 200) + L"…" : message.body;
        try
        {
            auto toast = AppNotificationBuilder()
                .AddArgument(L"thread", message.threadKey)
                .AddText(message.address)
                .AddText(body)
                .BuildNotification();
            AppNotificationManager::Default().Show(toast);
            core::Log::Write(L"Toast shown for thread " + message.threadKey);
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"Showing a toast failed: " + DescribeError(e));
        }
    }

    void Notifier::Shutdown()
    {
        if (m_registered)
        {
            AppNotificationManager::Default().Unregister();
            m_registered = false;
        }
    }
}
```

- [ ] **Step 4: Wire it into the app**

`Services/AppServices.h`: add `#include "Services/Notifier.h"`; before `// [members]`: `std::unique_ptr<Notifier> notifier;`.

`App.xaml.cpp`: add `#include "Services/Notifier.h"` and `#include "Services/InboxImporter.h"`. Insert before `// [app-init]` (after the tray block):

```cpp
        services.notifier = std::make_unique<wan::Notifier>();
        services.notifier->Initialize([this](std::wstring const& threadKey) { ShowThread(threadKey); });
        services.importer->NewIncoming.Add([](wan::core::Message const& message) { wan::Services().notifier->ShowIncoming(message); });
```

Insert before `// [activation]`:

```cpp
        if (args.Kind() == ExtendedActivationKind::AppNotification)
        {
            // App was launched (or redirected) by clicking a toast after it had exited.
            auto toast = args.Data().as<winrt::Microsoft::Windows::AppNotifications::AppNotificationActivatedEventArgs>();
            ShowThread(wan::Notifier::ThreadFromArguments(toast.Arguments()));
            return;
        }
```

Insert before `// [exit]`:

```cpp
        if (wan::Services().notifier) wan::Services().notifier->Shutdown();
```

`WANUtility.vcxproj`: add `<ClInclude Include="Services\Notifier.h" />`, `<ClCompile Include="Services\Notifier.cpp" />`.

- [ ] **Step 5: Deploy (manifest changed → re-register) and confirm data survived**

Run `scripts\deploy.ps1`.
Expected output includes `Backed up LocalState`, `Registered package`, `Restored LocalState`. Ask the user to confirm the Messages page still shows the same threads (DB preserved).

- [ ] **Step 6: Verify a toast while the window is hidden (free)**

Dial `*101*1#` on the USSD page, then close the window to the tray. Within ~2 minutes a toast "dtac — คุณมีโปรโมชั่น…" must appear. Click it → the window opens on the `dtac` thread. Log shows `Toast shown for thread dtac`.

- [ ] **Step 7: Verify suppression and cold-start activation (free)**

1. With the window open and focused on the `dtac` thread, dial `*101*1#` and stay on Messages: the message appears, **no** toast.
2. Dial `*101*1#`, then tray → Exit before the SMS arrives. Wait for the SMS. (The app is not running, so no toast is expected — record that.) Launch the app: the message is imported without a toast (it's not the first import, so the code *would* toast; record whether one appears. Either way the message must be in the thread).
3. From step 6, if a toast is still in Action Center after Exit, click it → the app launches on the `dtac` thread (log: `Activation kind 5001`).

- [ ] **Step 8: Commit**

```powershell
git add -A
git commit -m "feat(notify): toast for new SMS, click opens the thread`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 15: Start at sign-in + Settings page

**Files:**
- Create: `Services/StartupSetting.h`, `Services/StartupSetting.cpp`
- Replace: `SettingsPage.xaml`, `SettingsPage.xaml.h`, `SettingsPage.xaml.cpp`
- Modify: `Package.appxmanifest`, `App.xaml.cpp`, `WANUtility.vcxproj`

**Interfaces:**
- Consumes: `WindowsStoreSync::Enabled/DisabledReason/StateChanged` (Task 12), `ModemService::Info/StatusChanged` (Task 8), `MessageDb::UssdCodes/RenameUssdCode/RemoveUssdCode/PendingSync` (Task 4), marker `// [activation]`.
- Produces: `class wan::StartupSetting { static IAsyncOperation<StartupTaskState> GetStateAsync(); static IAsyncOperation<StartupTaskState> SetEnabledAsync(bool); }`.

- [ ] **Step 1: Declare the startup task in `Package.appxmanifest`**

Add `xmlns:uap5="http://schemas.microsoft.com/appx/manifest/uap/windows10/5"` to the `<Package …>` start tag and `uap5` to `IgnorableNamespaces` (→ `"uap rescap systemai com desktop uap5"`). Inside the existing `<Extensions>` of `<Application>`, add:

```xml
        <uap5:Extension Category="windows.startupTask" Executable="WANUtility.exe" EntryPoint="Windows.FullTrustApplication">
          <uap5:StartupTask TaskId="WANUtilityStartup" Enabled="false" DisplayName="WANUtility" />
        </uap5:Extension>
```

- [ ] **Step 2: Create `Services/StartupSetting.h` and `.cpp`**

```cpp
#pragma once

namespace wan
{
    // Wraps the packaged StartupTask "WANUtilityStartup" declared in Package.appxmanifest.
    class StartupSetting
    {
    public:
        static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::ApplicationModel::StartupTaskState> GetStateAsync();
        // Enabling may be refused (DisabledByUser / DisabledByPolicy); the returned state is the truth.
        static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::ApplicationModel::StartupTaskState> SetEnabledAsync(bool enabled);
    };
}
```

```cpp
#include "pch.h"
#include "Services/StartupSetting.h"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel;

namespace wan
{
    namespace
    {
        constexpr wchar_t kTaskId[] = L"WANUtilityStartup";
    }

    Windows::Foundation::IAsyncOperation<StartupTaskState> StartupSetting::GetStateAsync()
    {
        auto task = co_await StartupTask::GetAsync(kTaskId);
        co_return task.State();
    }

    Windows::Foundation::IAsyncOperation<StartupTaskState> StartupSetting::SetEnabledAsync(bool enabled)
    {
        auto task = co_await StartupTask::GetAsync(kTaskId);
        if (enabled)
        {
            co_return co_await task.RequestEnableAsync();
        }
        task.Disable();
        co_return task.State();
    }
}
```

- [ ] **Step 3: Start hidden when launched at sign-in**

`App.xaml.cpp`, insert before `// [activation]` (after the toast branch):

```cpp
        if (args.Kind() == ExtendedActivationKind::StartupTask && !fromRedirect)
        {
            wan::core::Log::Write(L"Started at sign-in: staying in the tray");
            return;
        }
```

- [ ] **Step 4: Replace `SettingsPage.xaml`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<Page
    x:Class="WANUtility.SettingsPage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    Loaded="Page_Loaded"
    Unloaded="Page_Unloaded">

    <Page.Resources>
        <Style x:Key="Card" TargetType="Border">
            <Setter Property="Background" Value="{ThemeResource CardBackgroundFillColorDefaultBrush}" />
            <Setter Property="BorderBrush" Value="{ThemeResource CardStrokeColorDefaultBrush}" />
            <Setter Property="BorderThickness" Value="1" />
            <Setter Property="CornerRadius" Value="8" />
            <Setter Property="Padding" Value="16" />
        </Style>
        <Style x:Key="Section" TargetType="TextBlock" BasedOn="{StaticResource BodyStrongTextBlockStyle}">
            <Setter Property="Margin" Value="0,16,0,4" />
        </Style>
    </Page.Resources>

    <ScrollViewer>
        <StackPanel Padding="24,16,24,24" Spacing="4" MaxWidth="760" HorizontalAlignment="Left">
            <TextBlock Text="Settings" Style="{StaticResource TitleTextBlockStyle}" />

            <TextBlock Text="General" Style="{StaticResource Section}" />
            <Border Style="{StaticResource Card}">
                <Grid ColumnSpacing="12">
                    <Grid.ColumnDefinitions>
                        <ColumnDefinition Width="*" />
                        <ColumnDefinition Width="Auto" />
                    </Grid.ColumnDefinitions>
                    <StackPanel Spacing="2" VerticalAlignment="Center">
                        <TextBlock Text="Start at sign-in" />
                        <TextBlock x:Name="StartupNote" Style="{StaticResource CaptionTextBlockStyle}" TextWrapping="Wrap" />
                    </StackPanel>
                    <ToggleSwitch x:Name="StartupToggle" Grid.Column="1" OnContent="On" OffContent="Off" Toggled="StartupToggle_Toggled" />
                </Grid>
            </Border>

            <TextBlock Text="Windows message store" Style="{StaticResource Section}" />
            <Border Style="{StaticResource Card}">
                <TextBlock x:Name="SyncStatus" TextWrapping="Wrap" />
            </Border>

            <TextBlock Text="Saved USSD codes" Style="{StaticResource Section}" />
            <Border Style="{StaticResource Card}">
                <StackPanel x:Name="CodesList" Spacing="8" />
            </Border>

            <TextBlock Text="Modem" Style="{StaticResource Section}" />
            <Border Style="{StaticResource Card}">
                <StackPanel Spacing="4">
                    <TextBlock x:Name="ModemModel" IsTextSelectionEnabled="True" />
                    <TextBlock x:Name="ModemFirmware" IsTextSelectionEnabled="True" />
                    <TextBlock x:Name="ModemCarrier" IsTextSelectionEnabled="True" />
                    <TextBlock x:Name="ModemSmsc" IsTextSelectionEnabled="True" />
                </StackPanel>
            </Border>
        </StackPanel>
    </ScrollViewer>
</Page>
```

- [ ] **Step 5: Replace `SettingsPage.xaml.h`**

```cpp
#pragma once

#include "SettingsPage.g.h"

namespace winrt::WANUtility::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage() {}

        void Page_Loaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void Page_Unloaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        fire_and_forget StartupToggle_Toggled(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        fire_and_forget LoadStartup();
        void ApplyStartupState(Windows::ApplicationModel::StartupTaskState state);
        void RenderSync();
        void RenderCodes();
        void RenderModem();

        bool m_suppressToggle = false;
        int m_syncToken = 0;
        int m_modemToken = 0;
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}
```

- [ ] **Step 6: Replace `SettingsPage.xaml.cpp`**

```cpp
#include "pch.h"
#include "SettingsPage.xaml.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif

#include "Core/Log.h"
#include "Core/TextUtil.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/StartupSetting.h"
#include "Services/WindowsStoreSync.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using winrt::Windows::ApplicationModel::StartupTaskState;

namespace winrt::WANUtility::implementation
{
    void SettingsPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        m_syncToken = s.storeSync->StateChanged.Add([this] { RenderSync(); });
        m_modemToken = s.modem->StatusChanged.Add([this] { RenderModem(); });
        LoadStartup();
        RenderSync();
        RenderCodes();
        RenderModem();
    }

    void SettingsPage::Page_Unloaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        s.storeSync->StateChanged.Remove(m_syncToken);
        s.modem->StatusChanged.Remove(m_modemToken);
    }

    fire_and_forget SettingsPage::LoadStartup()
    {
        auto strong = get_strong();
        try
        {
            ApplyStartupState(co_await wan::StartupSetting::GetStateAsync());
        }
        catch (hresult_error const& e)
        {
            StartupToggle().IsEnabled(false);
            StartupNote().Text(L"Unavailable: " + wan::DescribeError(e));
        }
    }

    fire_and_forget SettingsPage::StartupToggle_Toggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_suppressToggle) co_return;
        auto strong = get_strong();
        try
        {
            ApplyStartupState(co_await wan::StartupSetting::SetEnabledAsync(StartupToggle().IsOn()));
        }
        catch (hresult_error const& e)
        {
            wan::ReportError(L"Changing start at sign-in failed: " + wan::DescribeError(e));
            LoadStartup();
        }
    }

    void SettingsPage::ApplyStartupState(StartupTaskState state)
    {
        wan::core::Log::Write(L"Startup task state " + std::to_wstring(static_cast<int>(state)));
        m_suppressToggle = true; // setting IsOn raises Toggled synchronously
        const bool on = state == StartupTaskState::Enabled || state == StartupTaskState::EnabledByPolicy;
        const bool locked = state == StartupTaskState::DisabledByUser || state == StartupTaskState::DisabledByPolicy ||
                            state == StartupTaskState::EnabledByPolicy;
        StartupToggle().IsOn(on);
        StartupToggle().IsEnabled(!locked);
        switch (state)
        {
        case StartupTaskState::DisabledByUser:
            StartupNote().Text(L"Turned off in Windows Settings › Apps › Startup. Turn it on there first.");
            break;
        case StartupTaskState::DisabledByPolicy:
            StartupNote().Text(L"Turned off by your organization's policy.");
            break;
        case StartupTaskState::EnabledByPolicy:
            StartupNote().Text(L"Turned on by your organization's policy.");
            break;
        default:
            StartupNote().Text(L"Starts in the system tray when you sign in to Windows.");
            break;
        }
        m_suppressToggle = false;
    }

    void SettingsPage::RenderSync()
    {
        auto& s = wan::Services();
        if (!s.db)
        {
            SyncStatus().Text(L"Unavailable: the message database could not be opened.");
            return;
        }
        try
        {
            const auto pending = s.db->PendingSync().size();
            if (s.storeSync->Enabled())
            {
                SyncStatus().Text(pending == 0
                    ? std::wstring(L"On. Sent messages are also copied to the Windows message store.")
                    : L"On. " + std::to_wstring(pending) + L" sent message(s) waiting to be copied; retrying every 5 minutes.");
            }
            else
            {
                SyncStatus().Text(L"Off. " + s.storeSync->DisabledReason() + L" Sent messages are still kept in WANUtility.");
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Reading sync status failed: " + wan::Widen(e.what()));
        }
    }

    void SettingsPage::RenderCodes()
    {
        CodesList().Children().Clear();
        auto& s = wan::Services();
        if (!s.db) return;
        try
        {
            auto codes = s.db->UssdCodes();
            if (codes.empty())
            {
                TextBlock none;
                none.Text(L"No saved codes. Save one from the USSD page.");
                CodesList().Children().Append(none);
            }
            for (auto const& c : codes)
            {
                Grid row;
                row.ColumnSpacing(8);
                for (auto width : { GridLength{ 110, GridUnitType::Pixel }, GridLength{ 1, GridUnitType::Star },
                                    GridLength{ 0, GridUnitType::Auto }, GridLength{ 0, GridUnitType::Auto } })
                {
                    ColumnDefinition column;
                    column.Width(width);
                    row.ColumnDefinitions().Append(column);
                }

                TextBlock code;
                code.Text(c.code);
                code.VerticalAlignment(VerticalAlignment::Center);
                TextBox label;
                label.Text(c.label);
                Grid::SetColumn(label, 1);
                Button save;
                save.Content(box_value(L"Save"));
                Grid::SetColumn(save, 2);
                Button remove;
                remove.Content(box_value(L"Remove"));
                Grid::SetColumn(remove, 3);

                const int64_t id = c.id;
                save.Click([this, id, label](IInspectable const&, RoutedEventArgs const&)
                {
                    const auto text = wan::core::Trim(label.Text());
                    if (text.empty()) return;
                    try { wan::Services().db->RenameUssdCode(id, text); }
                    catch (wan::core::DbError const& e) { wan::ReportError(L"Renaming the code failed: " + wan::Widen(e.what())); }
                    RenderCodes();
                });
                remove.Click([this, id](IInspectable const&, RoutedEventArgs const&)
                {
                    try { wan::Services().db->RemoveUssdCode(id); }
                    catch (wan::core::DbError const& e) { wan::ReportError(L"Removing the code failed: " + wan::Widen(e.what())); }
                    RenderCodes();
                });

                row.Children().Append(code);
                row.Children().Append(label);
                row.Children().Append(save);
                row.Children().Append(remove);
                CodesList().Children().Append(row);
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading saved codes failed: " + wan::Widen(e.what()));
        }
    }

    void SettingsPage::RenderModem()
    {
        const auto info = wan::Services().modem->Info();
        auto orUnknown = [](std::wstring const& v) { return v.empty() ? std::wstring(L"Unknown") : v; };
        ModemModel().Text(L"Model: " + orUnknown(info.model));
        ModemFirmware().Text(L"Firmware: " + orUnknown(info.firmware));
        ModemCarrier().Text(L"Carrier: " + orUnknown(info.carrier));
        ModemSmsc().Text(L"SMS center: " + orUnknown(info.smsc));
    }
}
```

`WANUtility.vcxproj`: add `<ClInclude Include="Services\StartupSetting.h" />`, `<ClCompile Include="Services\StartupSetting.cpp" />`.

- [ ] **Step 7: Deploy (manifest changed) and verify Settings**

Run `scripts\deploy.ps1` (expect backup/restore lines). Open Settings with the user and confirm:
- Modem card: `Model: Quectel EM120R-GL` (or what Windows reports — record it), firmware `EM120RGLAPR02A07M4G`, SMS center `+66942500500`.
- Sync card matches the Task 12 outcome.
- Saved codes: rename "Status SMS" → "Package SMS", Save; USSD page chip shows the new label; Remove a code you added in Task 9 (not the seeded ones) and confirm it disappears from the USSD chips.

- [ ] **Step 8: Verify start at sign-in (needs the user to sign out and in)**

1. Toggle **On** → log `Startup task state 2`; Windows Settings › Apps › Startup lists WANUtility as On.
2. Ask the user to sign out and back in. Expected: `(Get-Process WANUtility).Count` = 1, no window shown, tray icon present, log `Started at sign-in: staying in the tray`.
3. In Windows Settings › Apps › Startup turn WANUtility **off**, reopen the app's Settings: toggle shows Off, is disabled, note "Turned off in Windows Settings › Apps › Startup…" (state 1). Turn it back on there and the toggle unlocks.
4. Toggle **Off** in the app → log `Startup task state 0`.

- [ ] **Step 9: Commit**

```powershell
git add -A
git commit -m "feat(settings): start at sign-in, sync status, saved codes, modem info`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 16: Final verification + documentation

**Files:**
- Create: `docs/superpowers/verification/2026-10-12-v1-verification.md`
- Modify: `CLAUDE.md`

- [ ] **Step 1: Full unit-test run**

Run: `powershell -ExecutionPolicy Bypass -File scripts\test.ps1`
Expected: `Total tests: 45. Passed: 45.` (plus any tests added during Task 9 Step 8). Paste the summary into the verification file.

- [ ] **Step 2: Modem loss during a USSD session (Review Focus 4) — ask the user first**

Dial `*103#`; while the menu is shown (AwaitingReply), turn on airplane mode, then tap an option or press Send.
Expected: within the network timeout the card shows "USSD failed: 0x…" or "Network timed out…", status "Ended · …", buttons re-enable after the modem returns; the app does not freeze or crash. Turn airplane mode off. Record the exact text.

- [ ] **Step 3: Run the spec §10.2 checklist and record observed results**

Create `docs/superpowers/verification/2026-10-12-v1-verification.md` with one row per check — each row's result comes from this run or from the task step that already observed it (cite the task):

```markdown
# WANUtility v1 — verification (2026-10-12)

| Check | How | Observed result | Pass? |
|---|---|---|---|
| Unit tests | scripts\test.ps1 | Passed: N/N | |
| *101# decoded Thai balance | USSD chip | (paste reply text) | |
| *101*1# → SMS → import → toast → dtac thread | Task 10 Step 8, Task 14 Step 6 | (import delay; toast yes/no) | |
| *103# multi-step, buttons, End (no purchase) | Task 9 Step 8 | (menu text, buttons parsed?) | |
| Send SMS to 0889605613 | Task 11 Step 8 | (user confirmed receipt?) | |
| Long Thai SMS (> 70 chars) | Task 11 Step 9 | (whole / failed / truncated) | |
| Windows store sync outcome | Task 12 Step 4 | (synced / disabled / retry + code) | |
| ✕ hides to tray, reopen, Exit, single instance | Task 13 Step 6 | | |
| Toast suppression + cold-start toast click | Task 14 Step 7 | | |
| Start at sign-in (sign out/in) | Task 15 Step 8 | | |
| Airplane mode: warning bar, Send/Dial disabled, recovery | Task 8 Step 6, Task 11 Step 10 | | |
| Modem loss during USSD session | Task 16 Step 2 | | |
```

Anything not run gets "Deferred — <reason>" in the result column, never left blank.

- [ ] **Step 4: Update `CLAUDE.md`**

- Replace the "Current state" bullet with: `v1 implemented per docs/superpowers/plans/2026-10-12-wanutility-v1.md; verification in docs/superpowers/verification/2026-10-12-v1-verification.md.`
- Under "Spike results", add the newly observed facts: `RegisteredProviderName` value, whether `MessageChanged` fires for a new SMS, Windows-store write outcome, numeric sender format, `*103#` menu format, long-SMS behaviour.
- Under "Gotchas", add: `Deploy with scripts\deploy.ps1 (re-registers on manifest change, preserves LocalState). Tests: scripts\test.ps1.`

- [ ] **Step 5: Commit**

```powershell
git add -A
git commit -m "docs: v1 verification results and updated project notes`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Push — only with the user's go-ahead**

Pushing publishes the code. Ask first; on approval (and once the user has added the SSH key from `~/.ssh/id_ed25519.pub` to GitHub):

```powershell
ssh -T git@github.com   # expect: "Hi awillfox! You've successfully authenticated…"
git push -u origin main
```
