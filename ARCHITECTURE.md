# TouchFreeze Architecture & Implementation Details

This document outlines the internal design, input handling pipeline, and components of TouchFreeze.

## High-Level Architecture

```
[Input Sources]
 ├── Low-Level Keyboard Hook (WH_KEYBOARD_LL) ──> Tracks last keypress timestamp & virtual keycode
 └── Raw Input API (WM_INPUT / HID Digitizer) ──> Extracts touch points, contact count, delta distance & PTP confidence
                                 │
                                 ▼
                    [Palm Rejection State Machine]
                    States: IDLE, TYPING, TOUCH_DETECTED, BLOCKED, RELEASED, COOLDOWN
                                 │
                                 ▼ Evaluates mouse suppression decision
                                 │
[Suppression Layer]              │
 └── Low-Level Mouse Hook (WH_MOUSE_LL) ───────> Blocks mouse events (WM_LBUTTONDOWN/UP, WM_MOUSEMOVE)
                                                 when State == BLOCKED

[Virtual Input & Gesture Layer]
 └── Right Drag Gesture Module ────────────────> Generates SendInput(MOUSEEVENTF_RIGHTDOWN/UP)
                                                 Bypasses HOOK via LLMHF_INJECTED flag
```

## Core Components

1. **`HookDll` (Shared Hook Library)**:
   - Sets global `WH_KEYBOARD_LL` and `WH_MOUSE_LL` hooks via `SetWindowsHookEx`.
   - Intercepts low-level mouse messages when `TFHookSetOverrideBlocked(TRUE)` is requested by the State Machine.
   - Filters out injected events (`LLMHF_INJECTED` and `TF_GESTURE_EXTRA_INFO`) to prevent infinite input loops.

2. **`TouchGesture` Module**:
   - Registers for HID Digitizer Raw Input (`RIDEV_INPUTSINK` with Usage Page `0x0D` and Usage `0x05`/`0x04`).
   - Parses Raw Input preparsed data to extract min/max boundaries, normalized ratio coordinates, contact count, and delta movement vectors (`DeltaDist`).
   - Runs the Palm Rejection State Machine:
     - **Typing Period**: Blocks multi-touch, low-confidence, or fast gestures while allowing subtle 1-finger cursor movements if enabled.
     - **Cooldown Period**: Gradually relaxes suppression sensitivity.

3. **`MonitorDlg` (Diagnostic UI)**:
   - Provides a real-time monitor dialog (`IDD_MONITOR`) in the main executable.
   - Renders a physical touchpad visualizer and real-time state machine event stream.
   - Configured with `HWND_TOPMOST` z-order and focus management for non-intrusive debugging over background windows.

## Build Information

- **Solution**: `TouchFreeze.sln`
- **Toolset**: Visual Studio 2022 (v143 Toolset), MSBuild
- **Configurations**:
  - `Release|Win32`: Primary 32-bit production binary and Wix MSI installer.
  - `Debug|x64`: 64-bit diagnostic build.
