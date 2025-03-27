# TouchFreeze

TouchFreeze is a Windows utility that automatically disables your laptop's touchpad while typing, preventing accidental cursor movements and unwanted clicks that can disrupt your workflow.

## Features

- Automatically disables touchpad input while typing
- Runs silently in the system tray
- Minimal resource usage
- Auto-start with Windows option
- Configurable blocking duration
- Simple and intuitive interface

## System Requirements

- Windows OS
- Laptop with touchpad/trackpad
- Administrator rights for installation
- Visual Studio Community (for building from source)

## Installation

1. Download the latest release from the releases page
2. Run the installer
3. Follow the installation wizard
4. TouchFreeze will automatically start and appear in your system tray

## Usage

- TouchFreeze runs automatically in the background
- A system tray icon indicates its status:
  - Normal icon: TouchFreeze is active
  - Blocked icon: Touchpad is temporarily disabled while typing
- Right-click the system tray icon for options:
  - Enable/Disable auto-start
  - Access settings
  - View about information
  - Exit the application

## Building from Source

1. Install Visual Studio Community
2. Open `TouchFreeze.sln` in Visual Studio
3. Build the solution
4. The installer will be generated in the `Setup` directory

## License

This project is licensed under the terms of the included License.txt file.

## Contributing

1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a new Pull Request
