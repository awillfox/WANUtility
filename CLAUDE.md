# WANUtility

Windows 11 utility for the machine's built-in cellular (WAN) modem: run USSD codes, send SMS
(RCS: see Scope). UI must look native to Windows 11.

## Stack

- WinUI 3, C++/WinRT, Windows App SDK 2.5.1, packaged (MSIX), sideloaded via Developer Mode.
- Project: `WANUtility.vcxproj` / `WANUtility.slnx` at repo root.
- MSBuild: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`

## Hardware facts (observed 2026-10-12, not assumed)

- Modem: Quectel EM120R-GL, firmware `EM120RGLAPR02A07M4G`, carrier dtac (Thailand).
- Attached over PCIe (`Quectel PCI Express`, VEN_1EAC), exposed to Windows as MBIM
  ("Generic Mobile Broadband Adapter"), plus a QMI channel (`Quectel QMUX Interconnect`).
- **No COM / AT port is exposed.** `AT+CUSD` / `AT+CMGS` over serial is NOT available as-is.
  Do not change modem USB composition/driver to get one (risky, possibly not reversible on
  ThinkPad whitelisted hardware) without explicit user sign-off.

## Scope decisions

- RCS: no Windows API; needs an IMS/SIP client + carrier/Jibe access. Out of scope for v1
  unless a separate spike proves otherwise.
- SMS/USSD route: Windows mobile broadband APIs over MBIM
  (`Windows.Devices.Sms.SmsDevice2`, `Windows.Networking.NetworkOperators.UssdSession`),
  requiring restricted capabilities `cellularMessaging`, `cellularDeviceControl`,
  `cellularDeviceIdentity`, `chatSystem`. Feasibility under sideloading: **verified** (see Spike results).
- Git remote: `git@github.com:awillfox/WANUtility.git` (branch `main`). SSH key `~/.ssh/id_ed25519`
  (generated 2026-10-12, no passphrase) — user adds the public key to GitHub.
- v1 USSD: **interactive sessions** (multi-step menus, reply until network ends session), chosen by user.
  Multi-step on dtac not yet verified (spike only ran one-shot codes).
- v1 background behavior: **close to system tray**, keep running, show Windows toast on new SMS;
  **setting to auto-start at sign-in** (user-toggleable). Chosen by user.
  Planned mechanisms (unverified until built): tray via Win32 `Shell_NotifyIcon`, toasts via
  WinAppSDK `AppNotificationManager`, auto-start via packaged `windows.startupTask` extension.
  New-SMS detection relies on `ChatMessageStore.MessageChanged` (+ periodic store re-read as safety net,
  since the event wasn't yet observed for a genuinely new SMS).
- v1 layout: **left NavigationView** (Messages, USSD; Settings pinned bottom; carrier + signal in pane),
  Messages page = thread list + conversation pane. Chosen by user from mockups.
- v1 USSD page: **current-step card** — shows only the current menu, numbered options parsed into
  clickable buttons (best-effort parse; typed reply always available as fallback), session status line,
  saved-code chips, recent history line. Chosen by user from mockups.
- v1 storage (chosen by user): **own SQLite DB is the primary copy of everything** (incoming copied in
  from the Windows chat store; sent saved on send); UI reads only from our DB. Sent messages are
  **also written to the Windows chat store**; on transient failure mark "unsynced" and retry later;
  on access denied, disable Windows-store sync (shown in Settings) — never retry-loop a permanent denial.
  SQLite via built-in `winsqlite3` (header `winsqlite/winsqlite3.h`, `winsqlite3.lib`, present in SDK 10.0.26100).
- v1 receiving UI: **conversation threads** (sent + received grouped per number), chosen by user.

## Process

- Architectural project: brainstorm -> written spec -> plan -> implement. No product code before
  spec approval.
- Spec: `docs/superpowers/specs/2026-10-12-wanutility-design.md` (all design decisions live there).
- Plan: `docs/superpowers/plans/2026-10-12-wanutility-v1.md` (16 tasks). Deviations from spec, approved
  with the plan: CppUnitTest + vstest instead of GoogleTest (no nuget.exe here); threads computed by query.
- Current state: spec approved, plan written, awaiting plan review + execution choice. Git identity not yet
  set (commits blocked until user gives GitHub email). Spike code in
  `MainWindow.xaml*` (marked `SPIKE`) is throwaway and gets replaced by the real app.
- Test values approved by user: USSD `*101#`; test SMS recipient `0889605613`.
- Real SMS costs money: never send automatically more than once. The spike guards this with
  `sms_attempted.marker` in the package LocalState folder.

## Spike results (2026-10-12, sideloaded dev package, all observed)

| Capability | API | Result |
|---|---|---|
| Send SMS | `SmsDevice2::GetDefault()` + `SendMessageAndGetResultAsync` | Works. Local-format number `0889605613` accepted; delivery confirmed by user. SMSC `+66942500500`. |
| USSD | `UssdSession::CreateFromNetworkAccountId` + `SendMessageAndGetReplyAsync` | Works. dtac replies with DCS `0x48` (UCS-2). **`PayloadAsText()` returns empty for UCS-2** — must decode `GetPayload()` bytes as UTF-16 BE ourselves. |
| Receive SMS (history) | `ChatMessageManager::RequestStoreAsync()` + `GetMessageReader().ReadBatchAsync()` | Works with rescap `chatSystem`. Incoming SMS land in the system chat store; newest-first. |
| Receive SMS (live) | `ChatMessageStore.MessageChanged` | Event fires, but carries no details in this SDK and also fired with no new message (during a USSD send). Treat it as a "re-read" signal and diff by message id. Not yet observed firing for a genuinely new SMS — verify in the real app. |
| Receive SMS (registration) | `SmsMessageRegistration::Register` | **Denied**: `0xD0000022` (STATUS_ACCESS_DENIED). Don't use. |
| Legacy `SmsDevice` / message store | — | **Removed** from SDK 10.0.28000 metadata. Not available. |

dtac codes: `*101#` = balance (free); `*101*1#` = request package-status SMS (arrives ~1–2 min later).

## Gotchas

- Use the VS **Community** MSBuild (`C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`).
  The Build Tools install lacks working VC targets ("OutputPath not set").
- Dev re-registration of the same package version is blocked (`0x80073CFB`) and VS may have
  registered from `x64\Debug\WANUtility\AppX\`. If the manifest changes: back up LocalState,
  `Remove-AppxPackage`, then `Add-AppxPackage -Register x64\Debug\WANUtility\AppxManifest.xml`.
  Removing the package **deletes LocalState**. If only code changed, rebuilding is enough.
- `std::wofstream` in the C locale silently drops everything after the first Thai char. Write UTF-8.
- `UssdMessage` has no default ctor: `UssdMessage{ text }`.
