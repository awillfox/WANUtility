# WANUtility v1 — verification (2026-10-12)

Branch `feat/v1`. Every result below was observed in this run: log lines, DB rows, UI Automation reads, or
window-only captures (`PrintWindow`). Anything not run says so.

| Check | How | Observed result | Pass? |
|---|---|---|---|
| Unit tests | `scripts\test.ps1` | `Total tests: 48  Passed: 48` (45 planned + 3 from the final-review fixes) | Yes |
| `*101#` decoded Thai balance | USSD chip (Task 9) | UI "Ended · *101#", reply `ยอดคงเหลือ47.79บ.ใช้ได้ถึง12/10/70 / ซื้อโปร โทร*103#`; log `USSD reply code=0`; Recent line added | Yes |
| `*101*1#` → SMS → import → toast → dtac thread | Tasks 10, 14 | Import: SMS timestamp 18:05:28.941, `Imported 1 message(s)` 18:05:28.991 (MessageChanged, not the 60 s timer). Hidden window: `Toast shown for thread dtac` 12 s after dial. **Clicking the toast: pending user** | Partly |
| `*103#` multi-step, buttons, End (no purchase) | Task 9 Step 8 | `USSD reply code=1`, text `1 เน็ต2GB 35บ./ว` / `2 เน็ต2GB 24บ/ว` / `3 เติม50บ.ฟรีเน็ต1GB` → 3 option buttons; End pressed without tapping an option → "Ended · *103#", buttons gone | Yes |
| Send SMS to 0889605613 | Task 11 Step 8 (user-approved) | `SMS 105 to 0889605613 (18 chars): sent`; thread `+66889605613`. **Receipt on phone: pending user** | Pending |
| Long Thai SMS (> 70 chars) | Task 11 Step 9 (user-approved) | 103 chars, `SMS 106 … (103 chars): sent`, DB body intact. **Arrived whole on phone: pending user** | Pending |
| Windows store sync outcome | Task 12 Step 4 | `Synced message 105 to Windows store as '22,6,6b'`, `106 … '22,6,6c'`; no duplicate after re-import (106 rows) | Yes |
| ✕ hides to tray, reopen, Exit, single instance | Task 13 Step 6 | ✕ → hidden, 1 process; tray NIN_SELECT (posted) → shown; Start launch while hidden → shown, 1 process (3/3 after the Task 14 fix). **Real tray left/right click + menu Exit: pending user** | Partly |
| Toast suppression + cold-start toast click | Task 14 Step 7 | Not observed: the attempt ran while another app had focus (toast correct then). **Pending user** | Pending |
| Start at sign-in | Task 15 Step 8 | Toggle On → `Startup task state 2` (+ registry State=2); Off → state 0 (left Off). **Sign out/in: pending user** | Partly |
| Airplane mode: warning bar, Send/Dial disabled, recovery | Task 8 Step 6, Task 11 Step 10 | Deferred — user chose to skip radio-off tests | Deferred |
| Modem loss during USSD session | Task 16 Step 2 | Deferred — user chose to skip radio-off tests | Deferred |
| Settings: modem info, codes | Task 15 Step 7 | Model `Quectel EM120R-GL`, firmware `EM120RGLAPR02A07M4G`, SMSC `+66942500500`; rename/add/remove codes reflected in USSD chips | Yes |
| Re-register keeps data | Tasks 14, 15 | `Backed up / Restored LocalState`; messages 102 = 102, 104 = 104 | Yes |
| Mica / Windows 11 look | Tasks 7, 15 | Captures show custom title bar, left NavigationView, tinted Mica background on Settings. **User eyeball: pending** | Pending |
