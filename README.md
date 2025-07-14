# Hoowachy

ESP32-based modular display system with WiFi connectivity and configurable modules.

## Features

- Modular architecture with automatic module registration
- WiFi connectivity management
- Configuration via INI files
- Multiple display modules (Clock, Weather, Mining stats, etc.)
- Easy module development framework

## Setup

### 1. Hardware Requirements

- ESP32 development board
- Display (configuration depends on your setup)
- SD card (for configuration storage)

### 2. SD Card Configuration

**Important**: You need to place the configuration file `hoowachy_config.ini` in the root directory of your SD card.

1. Format your SD card to FAT32
2. Copy the `hoowachy_config.ini` file to the root directory of the SD card
3. Insert the SD card into your ESP32 device

### 3. Configuration File

The `hoowachy_config.ini` file contains all module configurations. Example structure:

```ini
[clock]
enable=true
timezone=Europe/Kiev

[accuweather]
enable=true
api_key=your_api_key_here
```

### 4. Building and Flashing

This project uses PlatformIO for building and flashing:

```bash
# Build the project
pio run

# Flash to device
pio run --target upload

# Monitor serial output
pio device monitor
```

## Module Development

For detailed instructions on creating new modules, see [docs/module_creation.md](docs/module_creation.md).

## Project Structure

- `src/` - Main source code
  - `modules/` - All system modules
  - `main.cpp` - Main application entry point
- `docs/` - Documentation
- `hoowachy_config.ini` - Configuration file template

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

MIT License
