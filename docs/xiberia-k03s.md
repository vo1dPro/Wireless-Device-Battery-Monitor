# XIBERIA K03S Wireless 7.1

## Exact device

- Receiver: `36F9:C001`, interface 0, product `XIBERIA K03S Wireless 7.1`.
- Battery collection: `FF90:0001`, input/output lengths 64 bytes.
- Other C001 products (S21/K05S) and F016 K03S variants are deliberately excluded.
- Windows accepts the output report even though HidP_InitializeReportForID does
  not list output ID 55 for this device. Input ID 55 is present.

## Evidence (2026-11-22)

The fixed, zero-padded 64-byte query `55 65 01 00 ...` returned
`55 65 2D 02 00 ...` (64 bytes). Decimal 45 matched the user's earcup display;
the user confirmed that the headset was not charging.

After the user connected charging power, the same query returned
`55 65 34 00 ...` (52/state 00). State 00 is therefore recognized as external
power when the level is greater than 1; 0/1 are rejected as ambiguous echoes.
The charging percentage itself has not been validated against the display.

After unplugging, the full application reported K03S 56%, charging=false,
alongside MCHOSE A5 V2 Ultra at 87%. A subsequent taskbar inspection showed
both headset and mouse indicators (55% and 87%). The process was responsive.

Validation completed: new driver tests, full Windows CMake/MinGW build,
full-application `--json` during charging and after unplugging, and taskbar
visual inspection. On upstream base `1dc6bc5`, all 74 Windows test executables
were run: 73 passed, and `composition_hit_test` failed with "fully transparent
corners/center hit the field". The unchanged original test reproduces this
failure in the same environment. After the main test target stopped there,
the remaining recipes were executed separately and passed. Full charge,
headset power-off, reconnect and long-term stability have not been tested.

After restricting parsing to the hardware-observed states 00/02, the K03S
tests and full Windows build were rerun successfully. A fresh `--json` read
reported the K03S at 51% and the mouse at 87%. Unchanged upstream tests were
not repeated after this driver-only refinement.

The T7-style query `55 65 00 00 ...` returned an unchanged echo. This is **not**
a 0% battery reading and is rejected. Passive listening produced no frames in
the two-second observation window per vendor collection.

Protocol sources:
- Existing BatteryHub `src/drivers/mchose_v9pro.cpp` (BLK read-battery command).
- https://github.com/wushuai0802/myfinder-device-battery/issues/528
  (same product/receiver reportedly read by the BLK provider; no source supplied).
- https://github.com/sdparklab/Voltide/blob/main/src/Providers/XiberiaT7Provider.cs
  (comparison only: different T7 selector and reply layout, not used by this driver).

## Behavior and limits

- State 02: expose a percentage only for an exact 64-byte response and value 0–100.
- State 00 with level >1: hardware-checked charging detection.
- All other states, including 03/04/1A, are rejected until verified on this
  specific K03S receiver; no behavior is inferred from other headset models.
- Echoes, unknown states, malformed responses and timeouts never publish a fresh
  percentage. The app's existing last-known/stale handling remains in charge.
- No firmware, equalizer, lighting, pairing or configuration writes.
- One fixed query per poll, 1 second total response budget, at most 32 frames.

## Build and verify (PowerShell, repository directory)

```powershell
cmd /c build.bat 0.0.0 dev
g++ -std=c++17 -O2 -Wall -Wextra -static tests/xiberia_driver_test.cpp src/drivers/xiberia.cpp -o build/xiberia_driver_test.exe
.\build\xiberia_driver_test.exe
.\BatteryHub.exe --json
```

The build uses the existing MinGW-w64 toolchain, no new dependencies.
Only the Windows polling chain registers this driver; Linux integration and
hardware behavior are not verified by this change.
