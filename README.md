# TouchFreeze

TouchFreeze is an advanced Windows utility that automatically disables your laptop's touchpad while typing, preventing accidental palm touches, cursor jumps, and unwanted clicks that disrupt your workflow.

Enhanced with **Windows Precision Touchpad (PTP)** support, a multi-state **Palm Rejection State Machine**, and a **Real-time Diagnostic Monitor**, TouchFreeze delivers seamless typing without compromising intentional touchpad gestures.

## Key Features

- **Advanced Palm Rejection State Machine**:
  - Distinguishes between accidental palm rests and intentional cursor movements using a 6-state machine (`Idle`, `Typing`, `TouchDetected`, `Blocked`, `Released`, `Cooldown`).
  - **Single-Finger Motion Override**: Allows small, precise single-finger cursor adjustments even right after typing, while blocking large palm contacts.
- **Windows Precision Touchpad (PTP) & Raw Input Integration**:
  - Leverages Raw Input API (`WM_INPUT`) to inspect physical touch metrics (contact count, PTP confidence bit, normalized position ratio, and delta movement).
- **Real-Time Diagnostic & Input Monitor**:
  - Built-in GUI tool (`Input Monitor...`) accessible from the system tray for real-time visualization of touchpad touch points, state machine transitions, Raw Input values, and event streams.
- **Touchpad Right Drag Zone**:
  - Configurable physical zones (`Right 1/3`, `Right 1/2`, `Top-Right`, `Bottom-Right`, `Anywhere`, `Disabled`) to easily trigger right-drag operations via touchpad.
- **Customizable Blocking Duration**:
  - Selectable typing timeouts (`Fast 300ms`, `Normal 500ms`, `Slow 700ms`) with registry persistence.
- **Low Resource Overhead**:
  - Minimal CPU and memory usage, designed to run silently in the Windows system tray with auto-start support.

## System Requirements

- **OS**: Windows 10 / Windows 11 (64-bit or 32-bit)
- **Hardware**: Laptop with standard Touchpad or Windows Precision Touchpad (PTP)
- **Build Requirements**: Visual Studio 2022 / C++ Desktop Development Workload

## Installation

1. Download the latest `TouchFreeze.msi` or `TouchFreeze.zip` from the [Releases Page](https://github.com/kuwa72/touchfreeze/releases).
2. Run `TouchFreeze.msi` and follow the setup wizard, or extract `TouchFreeze.zip`.
3. TouchFreeze will launch and reside in your Windows System Tray (Taskbar Notification Area).

## System Tray Options & Usage

Right-click the **TouchFreeze** icon in the system tray to configure:

- **Auto Start**: Enable/disable automatic startup with Windows.
- **Block Time**: Set typing rejection timeout (`Fast 300ms` / `Normal 500ms` / `Slow 700ms`).
- **Palm Rejection**:
  - `Allow 1-Finger Motion`: Toggle single-finger precise motion override during typing.
- **Right Drag Zone**: Select the physical touchpad area for right-drag gesture recognition.
- **Input Monitor...**: Launch the real-time input diagnostic window to inspect touch features and state transitions.
- **About TouchFreeze**: View version and copyright information.

## Diagnostic Monitor

To verify how TouchFreeze recognizes your touchpad and palm inputs:

1. Right-click the system tray icon and select **Input Monitor...**.
2. Type on your keyboard or touch the touchpad to see real-time updates:
   - Current State Machine status (`Blocked`, `Typing`, `TouchDetected`, etc.)
   - Touchpad position ratio (`X %`, `Y %`) and delta movement distance (`DeltaDist`)
   - Hardware Contact Count and Confidence flag
   - Visual touchpad canvas and live event log stream

## Building from Source

```cmd
git clone https://github.com/kuwa72/touchfreeze.git
cd touchfreeze
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" TouchFreeze.sln /p:Configuration=Release /p:Platform=Win32
```

The compiled binaries will be output to `Executable\Bin\`.

## License

This project is open-source under the terms of the included [License.txt](License.txt) file.

