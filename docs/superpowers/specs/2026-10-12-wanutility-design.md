# WANUtility — Design Spec

Date: 2026-10-12
Status: draft for user review

## 1. Purpose

A Windows 11 utility for the laptop's built-in cellular modem that lets the user:

- send SMS and read received SMS as **conversation threads**,
- run **interactive USSD sessions** (multi-step carrier menus),
- keep running in the **system tray** with **toast notifications** for new SMS, optionally
  **auto-starting at sign-in**.

It must look and behave like a built-in Windows 11 app (WinUI 3, Fluent controls, Mica,
light/dark theme).

Single user, personal machine, sideloaded package (not Store-distributed).

### Out of scope (v1)

- **RCS**: Windows has no RCS API; it needs an IMS/SIP client with carrier/Jibe access. Separate
  spike if ever pursued.
- MMS, group messages, contacts integration, multiple modems/SIMs, Store distribution.
- Changing modem firmware, USB composition, or drivers.

## 2. Environment (observed)

| Item | Value |
|---|---|
| OS | Windows 11 Pro 10.0.26300 |
| Modem | Quectel EM120R-GL, firmware `EM120RGLAPR02A07M4G`, PCIe, exposed as MBIM |
| AT/COM port | **None exposed** — AT commands are not an option |
| Carrier | dtac (Thailand), SMSC `+66942500500` |
| Toolchain | VS 18 Community MSBuild, Windows App SDK 2.5.1, C++/WinRT, packaged (MSIX) |
| SQLite | built-in `winsqlite3` (SDK 10.0.26100 header + lib; `System32\winsqlite3.dll`) |

## 3. Feasibility basis (spike, 2026-10-12, all observed)

| Capability | API | Result |
|---|---|---|
| Send SMS | `SmsDevice2` | Works; local number format accepted; delivery confirmed by user |
| USSD | `UssdSession` | Works; dtac replies UCS-2 (DCS `0x48`); `PayloadAsText()` returns empty → decode `GetPayload()` as UTF-16 BE ourselves |
| Read received SMS | `ChatMessageManager::RequestStoreAsync` (rescap `chatSystem`) | Works; incoming SMS appear in the system chat store, newest first |
| Live receive signal | `ChatMessageStore.MessageChanged` | Fires but carries no details; also fired without a new message; not yet observed for a genuinely new SMS |
| `SmsMessageRegistration` | — | Access denied (`0xD0000022`) — not used |
| Legacy `SmsDevice` | — | Removed from SDK 10.0.28000 metadata — not used |

Required restricted capabilities: `cellularMessaging`, `cellularDeviceControl`,
`cellularDeviceIdentity`, `chatSystem`.

## 4. UI

### 4.1 Shell

- Left **NavigationView**: **Messages**, **USSD**; **Settings** pinned at the bottom; carrier name and
  signal strength shown in the pane footer.
- Mica backdrop, extended title bar, follows system light/dark theme.
- Errors and modem-state warnings appear as **InfoBar** at the top of the current page (no modal dialogs).

### 4.2 Messages page

- Left: thread list (search box, one row per thread: display name, last-message preview, time,
  unread badge) and a "new message" button.
- Right: selected conversation — incoming bubbles left, outgoing right (accent color), timestamps,
  compose box + Send.
- Outgoing bubble states: sending (progress), sent, failed (red marker + modem/network error codes +
  **Retry**).

### 4.3 USSD page — current-step card

- **Saved-code chips** across the top (one-click dial) plus "+ Save code". Seeded with
  `*101#` Balance, `*103#` Packages, `*101*1#` Status SMS. Chips can be renamed/removed.
- A free-text dial box for any code.
- **Current-step card**: session status line (e.g. "Session active · step 2 · *103# → 1"), the
  current reply text, numbered options rendered as **clickable buttons** when they parse, a typed
  reply box (always available), and **End**.
- "Recent" line(s) beneath: last completed sessions with their final reply.

### 4.4 Settings page

- Start at sign-in (toggle; reflects `StartupTask` state, including "disabled by user in Windows"
  which locks the toggle off).
- Windows message store sync: status (on / off-with-reason) and a count of unsynced messages.
- Manage saved USSD codes.
- Modem info (model, firmware, carrier, SMSC) read-only.

## 5. Components

| Unit | Responsibility | Depends on |
|---|---|---|
| `ModemService` | Carrier, signal, readiness; raises status-changed | `MobileBroadbandModem`, `SmsDevice2` |
| `SmsSender` | Send one SMS; return success or error codes | `SmsDevice2` |
| `UssdSessionManager` | Start / reply / end sessions; decode payload; map result codes | `UssdSession` |
| `UssdMenuParser` | Pure: reply text → numbered options (or none) | — |
| `Ucs2` | Pure: raw bytes → text (UTF-16 BE) | — |
| `PhoneNumber` | Pure: normalize addresses into thread keys | — |
| `MessageDb` | SQLite persistence: messages, threads, USSD codes/history, settings | `winsqlite3` |
| `InboxImporter` | Copy new incoming messages from the Windows chat store into `MessageDb` | `ChatMessageStore`, `MessageDb` |
| `WindowsStoreSync` | Write sent messages to the Windows chat store; retry transient; disable on access denied | `ChatMessageStore`, `MessageDb` |
| `TrayIcon` | Tray icon + menu (Open / Exit) on a hidden message-only window | Win32 `Shell_NotifyIcon` |
| `Notifier` | Toasts for new incoming SMS; click opens thread | `AppNotificationManager` |
| `StartupSetting` | Auto-start toggle | `StartupTask` |
| `MessagesPage`, `UssdPage`, `SettingsPage` | UI only; talk to services | services above |

Pages never call Windows modem APIs directly.

## 6. Data

### 6.1 Storage decision

**Our SQLite database is the primary copy of everything**; the UI reads only from it.

- Incoming SMS are **copied in** from the Windows chat store.
- Sent SMS are **saved on send**, and **also written to the Windows chat store**.
- If writing to the Windows store fails transiently, the message stays in our DB marked `pending`
  and is retried. If it fails with **access denied**, Windows-store sync is **disabled** (reason
  shown in Settings) and pending messages become `disabled` — a permanent denial is never retried
  in a loop.

DB file: `<package LocalState>\wanutility.db`.

### 6.2 Tables

- `messages`: `id` (PK), `thread_key`, `direction` (`in`/`out`), `body`, `timestamp` (UTC),
  `status` (`sending`/`sent`/`failed`/`received`), `error_detail` (nullable),
  `windows_store_id` (nullable, unique when set), `sync_state` (`n/a`/`pending`/`synced`/`disabled`),
  `is_read`.
- `threads`: `thread_key` (PK), `display_name`, `last_message_at`, `unread_count`
  (maintained alongside `messages`).
- `ussd_codes`: `id`, `code`, `label`, `sort_order`.
- `ussd_history`: `id`, `code`, `final_reply`, `timestamp`.
- `settings`: `key`, `value` (e.g. `store_sync_enabled`, `store_sync_disabled_reason`,
  `first_import_done`).

### 6.3 Thread keys

- Numeric addresses are normalized: Thai local `0XXXXXXXXX` → `+66XXXXXXXXX`; already-international
  numbers kept; spaces/dashes stripped.
- Alphanumeric senders (`dtac`, `dtacOnline`) are used verbatim.
- **Unverified assumption:** the Windows store reports numeric incoming senders in `+66` form. The
  spike only saw alphanumeric senders. Verify during build; normalization handles either form.

### 6.4 Flows

**Receive.** Triggers: `MessageChanged` event, 60 s periodic re-read, app start →
`InboxImporter` reads batches newest-first from the Windows store, **continuing batch after batch
until it reaches a message whose `windows_store_id` already exists (or the store ends)** — so a burst
larger than one batch (20 observed) is never skipped → inserts the new ones as unread → thread list updates → `Notifier`
toasts each new message. On the **first ever import** (`first_import_done` unset), existing messages
are imported **without toasts**.

**Send.** Insert `out` message as `sending` → `SmsSender` → `sent` or `failed` (with error codes) →
if `sent`, `sync_state = pending` and `WindowsStoreSync` attempts the write → `synced` (store id
recorded), stays `pending` (transient error), or sync disabled (access denied).

**Sync retry.** Pending messages retried at app start and every 5 minutes while sync is enabled.

## 7. USSD sessions

State machine: `Idle → Sending → AwaitingReply ⇄ Sending → Ended`. One session at a time.

| `UssdResultCode` | Behavior |
|---|---|
| `ActionRequired` | Show text, option buttons (if parsed), reply box, End |
| `NoActionRequired` | Show final text; session ends; append to `ussd_history` |
| `Terminated` | "Session ended by network" |
| `NetworkTimeout` | "Network timed out — dial again" |
| `OtherLocalClient` | "Another app is using USSD" |
| `OperationNotSupported` | "Not supported by network or modem" |

**Payload decoding:** if DCS indicates UCS-2 (e.g. `0x48`) → decode raw bytes as UTF-16 BE.
Otherwise `PayloadAsText()`. If still empty → show "Unreadable reply (DCS 0x..)". The non-UCS-2 path
is unobserved on dtac.

**Menu parsing:** lines beginning with a number followed by `.`, `)`, `-`, or whitespace become
options. Clicking an option sends that number exactly as if typed. No parse → no buttons; typed
reply only.

The session is owned by `UssdSessionManager`, so leaving the USSD page does not end it. **End**
closes it explicitly.

**Unverified:** real multi-step menus on dtac, and dtac's session timeout. First tested during build.

## 8. Tray, lifetime, startup, notifications

- **Close (✕) hides the window**; the app keeps running with a tray icon. Left-click → show window;
  right-click → **Open / Exit**. **Exit** is the only way to quit.
- **Single instance** via Windows App SDK `AppInstance`: a second launch activates the existing
  instance and shows its window.
- **Auto-start**: `windows.startupTask` extension in the package manifest, toggled via `StartupTask`.
  When activated by the startup task, the app starts **hidden in the tray**.
- **Toasts**: one per new incoming SMS (sender + text); click opens that thread. Suppressed when the
  window is visible, focused, and already showing that thread.

## 9. Error handling and logging

- Modem missing / no SIM / airplane mode / radio off → warning InfoBar; Send and Dial disabled until
  recovered; InfoBar clears on recovery.
- SMS send failure → failed bubble with codes + Retry.
- DB open/write failure → error InfoBar stating what failed; messages are never silently dropped.
- Log: UTF-8 text file in LocalState, size-capped with rotation. (Spike lesson: `wofstream` in the C
  locale silently drops Thai text.)

## 10. Testing and verification

### 10.1 Automated (`WANUtility.Tests`, GoogleTest via NuGet; modem-free code only)

- `UssdMenuParser`: realistic dtac menu texts; no-options case.
- `Ucs2`: the spike's captured raw bytes decode to `ยอดคงเหลือ47.79บ.ใช้ได้ถึง12/10/70\nซื้อโปร โทร*103#`.
- `PhoneNumber`: `0889605613` and `+66889605613` → same key; `dtac` unchanged.
- `MessageDb` (temp file): import de-duplication, thread grouping/unread counts, sync-state
  transitions, failed → retry.

### 10.2 Manual, on the real machine (results recorded with observed output)

| Check | Cost / risk |
|---|---|
| `*101#` shows decoded Thai balance | free |
| `*101*1#` → status SMS → imported → toast → in dtac thread | free (carrier status request) |
| `*103#` multi-step: buttons parse, replies work | **may purchase a package** — navigate only, then End/cancel; never confirm a purchase |
| Send one SMS to 0889605613 → thread → Windows store (or sync disables cleanly) | ~1 SMS fee; **ask user before each send** |
| ✕ hides to tray; tray reopens; Exit quits; second launch reuses instance | none |
| Auto-start on → sign out/in → app in tray | requires user sign-out/in |
| Airplane mode on → warning, Send disabled; off → recovers | briefly drops connectivity |

**Done** = automated tests pass (output shown) and every manual check has run with its observed
result recorded, or is explicitly deferred with a reason.

## 11. Open risks

1. `MessageChanged` not yet observed firing for a genuinely new SMS → mitigated by 60 s re-read.
2. Writing to the Windows chat store is unverified → designed to degrade to DB-only.
3. Incoming numeric sender format unverified → normalization covers both forms.
4. dtac multi-step USSD behavior and timeout unverified.
5. Tray, toasts and startup task are standard APIs but not yet exercised in this project.
