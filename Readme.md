# WANUtility

A Windows 11 app for the computer's built-in cellular (WAN) modem. It sends and receives SMS, runs
USSD codes, and shows your GOMO account balance. The interface is native WinUI 3.

## Features

- **Messages**: SMS conversations grouped by number. You can send SMS, and incoming SMS are imported from the
  Windows chat store. A Windows notification appears for each new message.
- **USSD**: runs codes such as `*101#`, including menus with several steps. Numbered options appear as buttons,
  and you can always type a reply. Codes can be saved and are shown as chips.
- **GOMO**: shows data remaining, calls remaining and package details for a GOMO (AIS) SIM. The carrier
  identifies you from the cellular connection, so no OTP is needed. This page is enabled only when Windows uses
  the cellular connection for internet and the carrier is AIS.
- **Background**: closing the window keeps the app in the system tray. In Settings you can choose to start the
  app when you sign in.

RCS is not supported. Windows has no RCS API.

## Requirements

- Windows 11 with a cellular modem that Windows sees as Mobile Broadband (MBIM).
- Developer Mode, because the app is sideloaded as an MSIX package. The app uses the restricted capabilities
  `cellularMessaging`, `cellularDeviceControl`, `cellularDeviceIdentity` and `chatSystem`.
- Visual Studio Community 2026 with the C++ desktop and WinUI workloads. Windows App SDK 2.5.1 and
  Windows SDK 10.0.26100 or later are also required.

## Build, test and run

From PowerShell in the repository folder:

```powershell
scripts\test.ps1     # build and run the unit tests (CppUnitTest + vstest)
scripts\deploy.ps1   # build, register the dev package if the manifest changed, and launch
```

`deploy.ps1` keeps the app's LocalState (message database and logs). Removing the package deletes LocalState,
so the script backs it up and restores it if it has to re-register the package.

Use the MSBuild from Visual Studio Community:
`C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`.
The Build Tools install lacks working VC targets.

## Project layout

| Path | Contents |
|---|---|
| `Core/` | Platform-independent logic: SQLite message DB, USSD menu parser, UCS-2 decoding, GOMO response parser |
| `Services/` | Modem, SMS, USSD, inbox import, tray, notifications, GOMO client and refresh |
| `*.xaml`, `*.idl` | Main window and the Messages, USSD, GOMO and Settings pages |
| `Tests/` | Unit tests |
| `docs/superpowers/` | Design spec, implementation plans and verification records |

## Data and privacy

Messages are stored in a local SQLite database in the app's LocalState folder. The GOMO session token is kept
in memory for a single refresh. It is only sent over HTTPS and is never logged or saved.
