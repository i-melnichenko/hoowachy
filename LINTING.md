# Code Quality Tools for Hoowachy

This document describes the code quality tools and linting setup for the Hoowachy project.

## Quick Start

```bash
# Show all available commands
make help

# Quick check your code (recommended for development)
make quick-check

# Format your code
make format

# Full comprehensive check
make check

# Before committing
make pre-commit
```

## Available Commands

| Command            | Description                                 |
| ------------------ | ------------------------------------------- |
| `make help`        | Show all available commands                 |
| `make quick-check` | Fast code check with cppcheck (recommended) |
| `make check`       | Full code analysis with all tools           |
| `make format`      | Auto-format all C++ source files            |
| `make pio-check`   | Run PlatformIO's built-in code check        |
| `make build`       | Build the project                           |
| `make upload`      | Upload firmware to device                   |
| `make monitor`     | Open serial monitor                         |
| `make reports`     | Show summary of analysis reports            |
| `make clean`       | Clean build files and reports               |
| `make pre-commit`  | Format code and run quick check             |
| `make dev-check`   | Alias for `quick-check`                     |
| `make ci`          | Full CI pipeline (check + build)            |

## Installing Tools

For full functionality, install the required tools:

```bash
# Install all tools (macOS)
make install-tools

# Or install manually
brew install cppcheck clang-format
```

## Development Workflow

### Daily Development

```bash
# Before starting work
make quick-check

# During development (check specific issues)
make dev-check

# Before committing
make pre-commit
```

### Code Review Preparation

```bash
# Full analysis
make check

# Review reports
make reports

# Ensure code is formatted
make format
```

## Tools Configuration

### cppcheck

- Configuration: `Makefile` (quick-check target)
- Full config: `cppcheck.xml`
- Checks: warnings, style, performance, portability
- Suppresses common false positives from system headers

### clang-tidy

- Configuration: `.clang-tidy`
- Follows camelCase naming convention
- Disabled overly strict checks for embedded development

### clang-format

- Configuration: `.clang-format`
- Based on Google style with 4-space indentation
- 120 character line limit

### PlatformIO Check

- Configuration: `platformio.ini`
- Uses built-in cppcheck integration
- Focused on high-severity issues

## Reports

All analysis reports are saved to the `reports/` directory:

- `quick_check.txt` - Results from quick cppcheck run
- `cppcheck.xml` - Full cppcheck analysis (XML format)
- `clang_tidy.txt` - clang-tidy analysis results
- `format_check.txt` - Code formatting issues
- `pio_check.json` - PlatformIO check results

## Integration

### Git Hooks

You can set up a pre-commit hook:

```bash
# Create .git/hooks/pre-commit
#!/bin/bash
make pre-commit
```

### IDE Integration

- **VS Code**: Install C/C++ and PlatformIO extensions
- **CLion**: Built-in support for clang-tidy and clang-format
- **Vim/Neovim**: Use ALE or coc.nvim with clangd

## Troubleshooting

### Tool not found errors

```bash
# Install missing tools
make install-tools

# Check if tools are available
which cppcheck clang-format clang-tidy
```

### PlatformIO check issues

```bash
# Clean and rebuild
make clean
pio run --environment esp32s3-n16r8

# Check with verbose output
pio check --environment esp32s3-n16r8 --verbose
```

### Configuration issues

- Ensure all configuration files are present: `.clang-tidy`, `.clang-format`, `cppcheck.xml`
- Check `platformio.ini` for proper check configuration
