# Secure IOT ESP32

## Rust + ESP32 Development Environment (Windows Guide)

This guide explains how to install and configure a Rust development environment for ESP32 on Windows, build a firmware, flash it, and open the serial monitor — entirely from the terminal.

---

### 1. Install Rust

```powershell
winget install --id Rustlang.Rustup -e
rustup update
rustc --version
```

---

### 2. Install Visual Studio Build Tools (MSVC Compiler)

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools -e
```

In the installer, enable:

- Desktop development with C++  
- MSVC v143 build tools  
- Windows 10/11 SDK  
- CMake tools (optional but recommended)

---

### 3. Open a Proper Build Environment (MSVC x64)

Every time you start a new PowerShell session for ESP32 development:

```powershell
powershell
& "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
```

---

### 4. Install ESP Rust Environment

```powershell
cargo install espup
espup install
```

Then load ESP-IDF environment:

```powershell
. $HOME\export-esp.ps1
```

**WARNING**: This command must be run again at each new PowerShell session

### 5. Install ldproxy
```powershell
cargo install ldproxy
```

---

### 6. Fix Windows Long Path Issues

Rust + ESP-IDF generates long paths → short build directory required:

```powershell
mkdir C:\t
setx CARGO_TARGET_DIR "C:\t"
```

**WARNING**: Restart PowerShell after running this  
Then reload:
```powershell
& "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
. $HOME\export-esp.ps1
```

---

### 7. Install ESP Tools

```powershell
cargo install cargo-generate
cargo install espflash cargo-espflash
```

---

### 8. Build & Flash Firmware to ESP32

Connect board → locate COM port (ex: `COM11`)  
Then:

```powershell
cargo espflash flash --release --port COM11 --monitor
```

---

### 9. Reminder for Each New Session

Before building or flashing:

```powershell
& "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
. $HOME\export-esp.ps1
```
