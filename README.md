# VeraCrypt SDK

A Software Development Kit (SDK) for integrating VeraCrypt encryption capabilities into your applications. This SDK provides a C-style DLL interface for creating VeraCrypt file containers and formatting non-system partitions programmatically.

## Overview

The VeraCrypt SDK currently provides the following functionality:
- **Format SDK**: Create encrypted file containers and format partitions with VeraCrypt encryption

*Note: Additional SDK modules (such as Mount SDK) are planned for future releases.*

## Features

- Create encrypted file containers with customizable parameters
- Format non-system partitions with VeraCrypt encryption
- Support for multiple encryption algorithms (AES, Serpent, Twofish, and combinations)
- Support for multiple hash algorithms (SHA-512, SHA-256, RIPEMD-160, Whirlpool, BLAKE2s-256)
- Support for various filesystems (NTFS, FAT, ExFAT, ReFS)
- Password and keyfile authentication
- Progress callback support
- Thread-safe operations
- Cross-architecture support (x64 and ARM64)

## Repository Structure

```
VeraCrypt-SDK/
├── bin/                          # Pre-compiled DLL files
│   ├── x64/
│   │   └── VeraCryptFormat.dll   # x64 DLL
│   └── arm64/
│       └── VeraCryptFormat.dll   # ARM64 DLL
├── lib/                          # Import libraries
│   ├── x64/
│   │   └── VeraCryptFormat.lib   # x64 import library
│   └── arm64/
│       └── VeraCryptFormat.lib   # ARM64 import library
├── include/
│   └── VeraCryptFormatSDK.h      # C header file with API definitions
├── VeraCryptFormatTool.cpp       # Sample implementation
├── CMakeLists.txt                # CMake build configuration
├── LICENSE                       # Apache 2.0 License
└── README.md                     # This file
```

## Requirements

### System Requirements
- Windows 10/11 (x64 or ARM64)
- VeraCrypt driver installed
- Administrator privileges (required for device/partition formatting)

### Development Requirements
- Visual Studio 2019 or later
- CMake 3.16 or later
- Windows SDK

## Quick Start

### Using CMake (Recommended)

1. Clone the repository:
```bash
git clone https://github.com/veracrypt/VeraCrypt-SDK.git
cd VeraCrypt-SDK
```

2. Build for x64 architecture:
```bash
mkdir build-x64
cd build-x64
cmake -A x64 ..
cmake --build . --config Release
```

3. Build for ARM64 architecture:
```bash
mkdir build-arm64
cd build-arm64
cmake -A ARM64 ..
cmake --build . --config Release
```

4. Run the sample tool (from either build directory):
```bash
# Create a 100MB encrypted file container
.\Release\VeraCryptFormatTool.exe "C:\MyData.hc" -p "MySecret123" -s 104857600

# Format a partition (requires Administrator privileges)
.\Release\VeraCryptFormatTool.exe \\Device\\Harddisk1\\Partition1 -p "EncryptMyPartition"
```

### Manual Compilation (Visual Studio)

1. Open a "Developer Command Prompt for VS"
2. Navigate to the SDK directory
3. Compile using:
```cmd
cl VeraCryptFormatTool.cpp /I include /link lib\x64\VeraCryptFormat.lib /out:VeraCryptFormatTool.exe
```

### Packaging SDK Distributions

To create distributable packages for both architectures:

1. Package x64 distribution:
```bash
cd build-x64
cpack -C Release
```
This creates `VeraCryptSDK-1.26.24-Windows-x64.zip`

2. Package ARM64 distribution:
```bash
cd build-arm64
cpack -C Release
```
This creates `VeraCryptSDK-1.26.24-Windows-ARM64.zip`

The generated packages include:
- Sample tool executable
- Required DLL files
- Development headers and import libraries
- Documentation

## API Usage

### Basic Example

```cpp
#include "VeraCryptFormatSDK.h"

int main() {
    // Initialize the SDK
    int result = VeraCryptFormat_Initialize();
    if (result != VCF_SUCCESS) {
        return result;
    }

    // Setup format options
    VeraCryptFormatOptions options = { 0 };
    options.path = L"C:\\MyContainer.hc";
    options.isDevice = FALSE;
    options.password = "MySecretPassword";
    options.size = 104857600; // 100MB
    options.encryptionAlgorithm = L"AES";
    options.hashAlgorithm = L"SHA-512";
    options.filesystem = L"NTFS";
    options.quickFormat = FALSE;

    // Create the encrypted volume
    result = VeraCryptFormat(&options);
    
    // Cleanup
    VeraCryptFormat_Shutdown();
    
    return result;
}
```

### Key Functions

- `VeraCryptFormat_Initialize()`: Initialize the SDK (call once per process)
- `VeraCryptFormat()`: Create an encrypted volume
- `VeraCryptFormat_Shutdown()`: Cleanup resources

### Supported Algorithms

**Encryption Algorithms:**
- AES
- Serpent
- Twofish
- AES-Twofish-Serpent
- Serpent-AES
- Serpent-Twofish-AES
- Twofish-Serpent

**Hash Algorithms:**
- SHA-512 (default)
- SHA-256
- RIPEMD-160
- Whirlpool
- BLAKE2s-256

**Filesystems:**
- NTFS (default)
- FAT
- ExFAT
- ReFS
- None (no filesystem)

## Sample Tool Usage

The included `VeraCryptFormatTool.exe` demonstrates SDK usage:

### Create File Containers

```bash
# Basic 100MB container with password
VeraCryptFormatTool.exe "C:\MyData.hc" -p "MySecret123" -s 104857600

# Dynamic container with custom encryption (password will be prompted)
VeraCryptFormatTool.exe "D:\secure.vhd" -s 268435456 -e "Serpent-Twofish-AES" -h "SHA-512" --dynamic

# Container with keyfile authentication
VeraCryptFormatTool.exe "C:\KeyfileVolume.hc" -k "C:\MyKeyfile.dat" -s 52428800
```

### Format Devices/Partitions (Requires Administrator)

```bash
# Format entire external disk
VeraCryptFormatTool.exe \Device\Harddisk1\Partition0 -p "EncryptMyDisk" --fs ExFAT

# Format partition with quick format
VeraCryptFormatTool.exe \Device\Harddisk1\Partition1 -p "EncryptMyPartition" --fs NTFS --quick
```

### Command Line Options

- `-p, --password [<pass>]`: Volume password (if option specified without <pass>, password will be prompted interactively)
- `-k, --keyfile <path>`: Keyfile path (can be used multiple times)
- `-s, --size <bytes>`: Container size (supports KB, MB, GB, TB suffixes)
- `-e, --encryption <alg>`: Encryption algorithm
- `--hash <alg>`: Hash algorithm
- `--fs <filesystem>`: Target filesystem
- `--pim <value>`: Personal Iterations Multiplier
- `--quick`: Quick format (less secure)
- `--dynamic`: Dynamic/sparse container
- `--fast`: Fast creation without random pool wait

## Error Handling

The SDK returns standardized error codes:

- `VCF_SUCCESS` (0): Operation successful
- `VCF_ERROR_INVALID_PARAMETER`: Invalid parameters provided
- `VCF_ERROR_PASSWORD_OR_KEYFILE_REQUIRED`: Authentication required
- `VCF_ERROR_ACCESS_DENIED`: Insufficient permissions
- `VCF_ERROR_NO_DRIVER`: VeraCrypt driver not available
- And more... (see `VeraCryptFormatSDK.h` for complete list)

## Security Considerations

- Always use strong passwords or keyfiles
- Consider using PIM (Personal Iterations Multiplier) for additional security
- Avoid quick format for maximum security (overwrites existing data)
- Run with Administrator privileges only when formatting devices
- Ensure VeraCrypt driver is properly installed and running

## Threading and Safety

- The SDK is thread-safe
- `VeraCryptFormat_Initialize()` must be called once per process
- Multiple format operations are serialized internally
- Progress callbacks are called from the formatting thread

## Troubleshooting

### Common Issues

1. **"VeraCrypt driver not available"**
   - Install VeraCrypt application first
   - Ensure the driver service is running

2. **"Access denied"**
   - Run as Administrator for device formatting
   - Check file/folder permissions for containers

3. **"Invalid parameter"**
   - Verify all required parameters are provided
   - Check that size is multiple of 512 bytes
   - Ensure encryption/hash algorithms are valid

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Related Projects

- [VeraCrypt](https://github.com/veracrypt/VeraCrypt) - The main VeraCrypt project

## Support

For support and questions:
- Create an issue on this repository
- Visit the [VeraCrypt forums](https://sourceforge.net/p/veracrypt/discussion/)
- Check the [VeraCrypt documentation](https://www.veracrypt.jp/en/Documentation.html)

---

**Note**: This SDK is provided as-is. Always test thoroughly in your specific environment before production use.
