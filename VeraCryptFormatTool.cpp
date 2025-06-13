/**
 * @file VeraCryptFormatTool.cpp
 * @brief A command-line tool to demonstrate the usage of VeraCryptFormat.dll.
 *
 * This program serves as a reference implementation for integrators of the
 * VeraCrypt Format SDK. It shows how to create VeraCrypt file containers
 * and format device partitions using the provided DLL.
 *
 * It covers:
 * - Parsing command-line arguments.
 * - Checking for Administrator privileges for device formatting.
 * - Initializing and shutting down the SDK.
 * - Setting up the VeraCryptFormatOptions struct.
 * - Implementing a progress callback.
 * - Handling and displaying errors.
 * - Securely handling passwords in memory.
 *
 * -----------------------------------------------------------------------------
 * HOW TO COMPILE (using Microsoft Visual C++ Compiler):
 * -----------------------------------------------------------------------------
 * 1. Save this code as `VeraCryptFormatTool.cpp`.
 * 2. Save the provided header as `VeraCryptFormat.h`.
 * 3. Place `VeraCryptFormat.dll` and `VeraCryptFormat.lib` in the same directory.
 * 4. Open a "Developer Command Prompt for VS".
 * 5. Navigate to the directory containing the files.
 * 6. Run the following command:
 *
 *    cl VeraCryptFormatTool.cpp /link VeraCryptFormat.lib /out:VeraCryptFormatTool.exe
 *
 * -----------------------------------------------------------------------------
 * EXAMPLE USAGE:
 * -----------------------------------------------------------------------------
 *
 * 1. Create a 100MB dynamic file container with AES and a password:
 *    VeraCryptFormatTool.exe "C:\MyData.hc" -p "MySecret123" -s 104857600 --dynamic
 *
 * 2. Create a 256MB file container with Serpent-Twofish-AES, SHA-512, and a PIM:
 *    VeraCryptFormatTool.exe "D:\secure.vhd" -s 268435456 -e "Serpent-Twofish-AES" -h "SHA-512" --pim 2048
 *
 * 3. Create a container using a keyfile:
 *    VeraCryptFormatTool.exe "C:\KeyfileVolume.hc" -k "C:\MyKeyfile.dat"
 *
 * 4. Format an entire external disk with ExFAT.
 *    (REQUIRES RUNNING AS ADMINISTRATOR)
 *    VeraCryptFormatTool.exe \Device\Harddisk1\Partition0 -p "EncryptMyDisk" --fs ExFAT
 *
 * 5. Format a partition with NTFS and quick format.
 *    (REQUIRES RUNNING AS ADMINISTRATOR)
 *    VeraCryptFormatTool.exe \Device\Harddisk1\Partition1 -p "EncryptMyPartition" --fs NTFS --quick
 *
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>
#include <shlwapi.h>
#include "VeraCryptFormatSDK.h"

#pragma comment(lib, "shlwapi.lib")

 // Maximum number of keyfiles supported by this tool
#define MAX_KEYFILES 16

// --- Helper Functions ---
BOOL IsElevated();
void PrintUsage();
const char* GetErrorMessage(int errorCode);
BOOL CALLBACK ProgressCallback(int percentComplete, void* userData);
BOOL GetPasswordFromConsole(char* utf8Password, int bufSize, int* pBytesWritten);
BOOL ParseSizeWithSuffix(const wchar_t* sizeStr, uint64_t* pSize);
BOOL GetAbsolutePath(const wchar_t* relativePath, wchar_t** absolutePath);
BOOL IsVolumeDeviceHosted (const wchar_t *lpszDiskFile);

// --- Main Application Logic ---
int wmain(int argc, wchar_t* argv[])
{
    VeraCryptFormatOptions options = { 0 };
    const wchar_t* keyfiles[MAX_KEYFILES + 1] = { 0 }; // +1 for NULL terminator
    int keyfileCount = 0;
    wchar_t* passwordArg = NULL;
    char* utf8Password = NULL;
    int utf8PasswordSize = 0;
    char consolePasswordBuffer[VC_MAX_PASSWORD * 4 + 1]; // safe size
    int consolePasswordLen = 0;
    wchar_t* absolutePath = NULL; // For storing absolute path
    BOOL passwordRequested = FALSE; // NEW: Track if -p was specified (with or without value)
    int exitCode = EXIT_FAILURE;
    int i;
    int result;

    fwprintf(stderr, L"VeraCrypt Format SDK Command-Line Tool\n");
    fwprintf(stderr, L"---------------------------------------\n\n");

    if (argc < 2 || _wcsicmp(argv[1], L"-h") == 0 || _wcsicmp(argv[1], L"--help") == 0) {
        PrintUsage();
        return (argc < 2) ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    // The first argument MUST be the path.
    if (argv[1][0] == L'-') {
        fwprintf(stderr, L"Error: The first argument must be the volume path. Options must come after the path.\n\n");
        PrintUsage();
        goto cleanup;
    }
    options.path = argv[1];

    // --- 1. Parse Command Line Arguments ---
    options.encryptionAlgorithm = L"AES";
    options.hashAlgorithm = L"SHA-512";
    options.filesystem = L"NTFS";

    for (i = 2; i < argc; ++i) { // Start parsing from the first option
        if (_wcsicmp(argv[i], L"-h") == 0 || _wcsicmp(argv[i], L"--help") == 0) {
            PrintUsage();
            exitCode = EXIT_SUCCESS;
            goto cleanup;
        }
        else if (_wcsicmp(argv[i], L"-p") == 0 || _wcsicmp(argv[i], L"--password") == 0) {
            passwordRequested = TRUE; // NEW: Mark that password was requested
            // Check if next argument exists and is not another option
            if (i + 1 < argc && argv[i + 1][0] != L'-') {
                passwordArg = argv[++i]; // Use provided password
            }
            // If no argument follows or next argument is an option, password will be prompted
        }
        else if ((_wcsicmp(argv[i], L"-k") == 0 || _wcsicmp(argv[i], L"--keyfile") == 0) && i + 1 < argc) {
            if (keyfileCount < MAX_KEYFILES) {
                const wchar_t* keyfilePath = argv[++i];
                if (!PathFileExistsW(keyfilePath)) {
                    fwprintf(stderr, L"Error: Keyfile not found at path: %s\n", keyfilePath);
                    goto cleanup;
                }
                keyfiles[keyfileCount++] = keyfilePath;
            }
            else {
                fwprintf(stderr, L"Error: Maximum number of keyfiles (%d) exceeded.\n", MAX_KEYFILES);
                goto cleanup;
            }
        }
        else if ((_wcsicmp(argv[i], L"-s") == 0 || _wcsicmp(argv[i], L"--size") == 0) && i + 1 < argc) {
            if (!ParseSizeWithSuffix(argv[++i], &options.size)) {
                fwprintf(stderr, L"Error: Invalid size parameter. Use a number optionally followed by KB, MB, GB, or TB.\n");
                goto cleanup;
            }
        }
        else if ((_wcsicmp(argv[i], L"-e") == 0 || _wcsicmp(argv[i], L"--encryption") == 0) && i + 1 < argc) {
            options.encryptionAlgorithm = argv[++i];
        }
        else if ((_wcsicmp(argv[i], L"--hash") == 0) && i + 1 < argc) {
            options.hashAlgorithm = argv[++i];
        }
        else if ((_wcsicmp(argv[i], L"--fs") == 0) && i + 1 < argc) {
            options.filesystem = argv[++i];
        }
        else if ((_wcsicmp(argv[i], L"--pim") == 0) && i + 1 < argc) {
            wchar_t* endPtr;
            long pim_val = wcstol(argv[++i], &endPtr, 10);
            if (*endPtr != L'\0' || pim_val < 0 || pim_val > INT_MAX) {
                fwprintf(stderr, L"Error: Invalid or out-of-range PIM value.\n");
                goto cleanup;
            }
            options.pim = (int)pim_val;
        }
        else if (_wcsicmp(argv[i], L"--quick") == 0) {
            options.quickFormat = TRUE;
        }
        else if (_wcsicmp(argv[i], L"--dynamic") == 0) {
            options.dynamicFormat = TRUE;
        }
        else if (_wcsicmp(argv[i], L"--fast") == 0) {
            options.fastCreateFile = TRUE;
        }
        else if (argv[i][0] != L'-' && options.path == NULL) {
            options.path = argv[i];
        }
        else {
            fwprintf(stderr, L"Error: Unknown or invalid argument: %s\n", argv[i]);
            PrintUsage();
            goto cleanup;
        }
    }

    // Automatically detect if the path is for a device
    options.isDevice = IsVolumeDeviceHosted(options.path);
    
    // --- 2. Validate Parsed Arguments ---
    if (!options.isDevice && options.size == 0) {
        fwprintf(stderr, L"Error: File container creation requires a non-zero size specified with -s.\n");
        goto cleanup;
    }

    // Note: The API should handle this, but validating early is good practice.
    if (!options.isDevice && (options.size % 512 != 0)) {
        fwprintf(stderr, L"Error: File container size must be a multiple of 512 bytes.\n");
        goto cleanup;
    }

    // Convert relative path to absolute path for file containers
    if (!options.isDevice) {
        if (!GetAbsolutePath(options.path, &absolutePath)) {
            fwprintf(stderr, L"Error: Failed to get absolute path for '%s'.\n", options.path);
            goto cleanup;
        }
        options.path = absolutePath;
    }

    if (options.dynamicFormat && !options.isDevice) {
        fwprintf(stderr, L"Info: --dynamic implies --quick format.\n");
        options.quickFormat = TRUE;
    }

    if (options.isDevice && !IsElevated()) {
        fwprintf(stderr, L"Error: Administrator privileges are required to format a device or partition.\n");
        fwprintf(stderr, L"Please run this tool from an elevated (Administrator) command prompt.\n");
        goto cleanup;
    }

    if (options.isDevice && options.size > 0) {
        fwprintf(stderr, L"Warning: Size parameter (-s) is ignored for device formatting.\n");
        options.size = 0;
    }

    // --- 3. Prepare Options for the DLL call ---
    
    // NEW: Modified password handling logic
    if (passwordRequested) {
        if (passwordArg) {
            // Password was provided via command line
            utf8PasswordSize = WideCharToMultiByte(CP_UTF8, 0, passwordArg, -1, NULL, 0, NULL, NULL);
            if (utf8PasswordSize == 0) {
                fwprintf(stderr, L"Error: Could not determine buffer size for password conversion. Win32 Error: %lu\n", GetLastError());
                goto cleanup;
            }
            utf8Password = (char*)malloc(utf8PasswordSize);
            if (!utf8Password) {
                fwprintf(stderr, L"Error: Failed to allocate memory for UTF-8 password.\n");
                goto cleanup;
            }
            utf8PasswordSize = WideCharToMultiByte(CP_UTF8, 0, passwordArg, -1, utf8Password, utf8PasswordSize, NULL, NULL);
            if (utf8PasswordSize == 0) {
                fwprintf(stderr, L"Error: Could not convert password to UTF-8. Win32 Error: %lu\n", GetLastError());
                goto cleanup;
            }
            if (utf8PasswordSize > (VC_MAX_PASSWORD + 1)) {
                fwprintf(stderr, L"Error: Password exceeds maximum length of %d UTF-8 characters.\n", VC_MAX_PASSWORD);
                goto cleanup;
            }
            options.password = utf8Password;
        }
        else {
            // Password was requested but not provided - prompt for it
            fwprintf(stderr, L"Password requested via -p. Please enter it now.\n");
            if (!GetPasswordFromConsole(consolePasswordBuffer, sizeof(consolePasswordBuffer), &consolePasswordLen)) {
                fwprintf(stderr, L"\nError: Failed to read password from console.\n");
                goto cleanup;
            }
            else if (consolePasswordLen > VC_MAX_PASSWORD) {
                fwprintf(stderr, L"Error: Password exceeds maximum length of %d UTF-8 characters.\n", VC_MAX_PASSWORD);
                goto cleanup;
            }
            options.password = consolePasswordBuffer;
        }
    }
    else if (keyfileCount == 0) {
        // No password requested and no keyfiles - prompt for password (backward compatibility)
        fwprintf(stderr, L"No password or keyfiles provided. Please enter a password.\n");
        if (!GetPasswordFromConsole(consolePasswordBuffer, sizeof(consolePasswordBuffer), &consolePasswordLen)) {
            fwprintf(stderr, L"\nError: Failed to read password from console.\n");
            goto cleanup;
        }
        else if (consolePasswordLen > VC_MAX_PASSWORD) {
            fwprintf(stderr, L"Error: Password exceeds maximum length of %d UTF-8 characters.\n", VC_MAX_PASSWORD);
            goto cleanup;
        }
        options.password = consolePasswordBuffer;
    }
    // NEW: If passwordRequested is FALSE and keyfileCount > 0, only keyfiles will be used

    // Validate that we have at least one authentication method
    if (!options.password && keyfileCount == 0) {
        fwprintf(stderr, L"Error: At least one authentication method (password or keyfile) must be provided.\n");
        goto cleanup;
    }

    if (keyfileCount > 0) {
        options.keyfiles = keyfiles;
    }

    options.progressCallback = ProgressCallback;
    options.progressUserData = (void*)L"Format Operation";

    // --- 4. Initialize the VeraCrypt Format SDK ---
    fwprintf(stderr, L"Initializing VeraCrypt Format SDK...\n");
    result = VeraCryptFormat_Initialize();
    if (result != VCF_SUCCESS) {
        fwprintf(stderr, L"Error: Failed to initialize SDK. Code: %d (%hs)\n", result, GetErrorMessage(result));
        goto cleanup;
    }
    fwprintf(stderr, L"SDK Initialized Successfully.\n");

    // --- 5. Call the Formatting Function ---
    fwprintf(stderr, L"\nStarting VeraCrypt volume creation...\n");
    fwprintf(stderr, L"  Path: %s\n", options.path);
    fwprintf(stderr, L"  Type: %s\n", options.isDevice ? L"Device/Partition" : L"File Container");
    fwprintf(stderr, L"  Encryption: %s\n", options.encryptionAlgorithm);
    fwprintf(stderr, L"  Hash: %s\n", options.hashAlgorithm);
    fwprintf(stderr, L"  Filesystem: %s\n", options.filesystem);
    if (!options.isDevice) {
        fwprintf(stderr, L"  Size: %llu bytes\n", options.size);
    }
    if (options.pim > 0) {
        fwprintf(stderr, L"  PIM: %d\n", options.pim);
    }
    fwprintf(stderr, L"\n");

    result = VeraCryptFormat(&options);

    fwprintf(stderr, L"\n");

    // --- 6. Process Results ---
    if (result == VCF_SUCCESS) {
        // CHANGE: Final success message goes to stdout
        wprintf(L"SUCCESS: VeraCrypt volume created successfully at '%s'.\n", options.path);
        exitCode = EXIT_SUCCESS;
    }
    else {
        fwprintf(stderr, L"\n\nERROR: VeraCrypt volume creation failed. Code: %d (%hs)\n", result, GetErrorMessage(result));
        exitCode = EXIT_FAILURE;
    }

cleanup:
    // --- 7. Shutdown and Cleanup ---
    fwprintf(stderr, L"\nShutting down VeraCrypt Format SDK...\n");
    VeraCryptFormat_Shutdown();

    if (utf8Password) {
        SecureZeroMemory(utf8Password, utf8PasswordSize);
        free(utf8Password);
        utf8Password = NULL;
    }

    if (absolutePath) {
        free(absolutePath);
        absolutePath = NULL;
    }

    SecureZeroMemory(consolePasswordBuffer, sizeof(consolePasswordBuffer));

    return exitCode;
}

BOOL IsElevated() {
    BOOL fIsElevated = FALSE;
    HANDLE hToken = NULL;
    TOKEN_ELEVATION Elevation;
    DWORD cbSize;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        fwprintf(stderr, L"[AdminCheck] OpenProcessToken failed. Error: %lu\n", GetLastError());
        return FALSE;
    }

    if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
        fIsElevated = Elevation.TokenIsElevated;
    }
    else {
        fwprintf(stderr, L"[AdminCheck] GetTokenInformation failed. Error: %lu\n", GetLastError());
    }

    if (hToken) {
        CloseHandle(hToken);
    }
    return fIsElevated;
}

void PrintUsage() {
    // CHANGE: Usage instructions go to stderr
    fwprintf(stderr, L"Usage: VeraCryptFormatTool.exe <path> [options]\n\n");
    fwprintf(stderr, L"  <path>             Full path for file container or device path (e.g., \\Device\\Harddisk1\\Partition0).\n");
    fwprintf(stderr, L"Required Security Options (at least one):\n");
    fwprintf(stderr, L"  -p, --password <pass>  Password for the volume.\n");
    fwprintf(stderr, L"  -k, --keyfile <path>   Path to a keyfile. Can be used multiple times.\n\n");
    fwprintf(stderr, L"File Container Options:\n");
    fwprintf(stderr, L"  -s, --size <bytes>     Size of the file container in bytes. Must be a multiple of 512.\n");
    fwprintf(stderr, L"  --dynamic              Create a dynamically-expanding (sparse) file container.\n");
    fwprintf(stderr, L"  --fast                 Quick creation without waiting for random pool to be filled.\n\n");
    fwprintf(stderr, L"General Formatting Options:\n");
    fwprintf(stderr, L"  --pim <value>          Personal Iterations Multiplier (PIM). Use 0 for default.\n");
    fwprintf(stderr, L"  -e, --encryption <alg> Encryption algorithm (Default: AES).\n");
    fwprintf(stderr, L"                         (e.g., Serpent, Twofish, AES-Twofish-Serpent)\n");
    fwprintf(stderr, L"  --hash <alg>           Hash algorithm (Default: SHA-512).\n");
    fwprintf(stderr, L"                         (e.g., RIPEMD-160, Whirlpool, SHA-256)\n");
    fwprintf(stderr, L"  --fs <filesystem>      Filesystem to format with (Default: NTFS).\n");
    fwprintf(stderr, L"                         (e.g., FAT, ExFAT, ReFS, None)\n");
    fwprintf(stderr, L"  --quick                Perform a quick format (less secure).\n\n");
    fwprintf(stderr, L"Other:\n");
    fwprintf(stderr, L"  -h, --help             Display this help message.\n");
}

/**
 * @brief A callback function to receive and display progress updates.
 * This function can return FALSE to abort the operation.
 * @param percentComplete The percentage complete (0-100).
 * @param userData User-defined data passed from the options struct.
 * @return TRUE to continue, FALSE to abort.
 */
BOOL CALLBACK ProgressCallback(int percentComplete, void* userData)
{
    static int lastPos = -1;
    static int lastPercent = -1;
    wchar_t* operationName = (wchar_t*)userData;
    int barWidth = 50;
    int pos = (barWidth * percentComplete) / 100;

    // Only redraw if something changed
    if (pos == lastPos && percentComplete > 0 && percentComplete < 100) return TRUE;
    if (percentComplete == lastPercent) return TRUE;
    lastPercent = percentComplete;

    // CHANGE: Progress bar prints to stderr
    fwprintf(stderr, L"\r%s: [", operationName);
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) fwprintf(stderr, L"#");
        else if (i == pos) fwprintf(stderr, L"#");
        else fwprintf(stderr, L" ");
    }
    fwprintf(stderr, L"] %3d%% ", percentComplete);
    fflush(stderr);

    // Return TRUE to continue the format operation.
    // An integrator could add logic here to return FALSE to abort.
    return TRUE;
}

static BOOL ReadAndCleanPassword(HANDLE hStdin, const wchar_t* prompt, wchar_t* passwordBuf, size_t bufCharCount)
{
    DWORD charsRead = 0;
    fwprintf(stderr, prompt);
    fflush(stderr);

    if (!ReadConsoleW(hStdin, passwordBuf, (DWORD)bufCharCount - 1, &charsRead, NULL)) {
        fwprintf(stderr, L"\nError reading from console. Win32 Error: %lu\n", GetLastError());
        return FALSE;
    }
    passwordBuf[charsRead] = L'\0'; // Ensure null-termination

    // Clean up trailing newline characters
    if (charsRead >= 2 && passwordBuf[charsRead - 2] == L'\r' && passwordBuf[charsRead - 1] == L'\n') {
        passwordBuf[charsRead - 2] = L'\0';
    } else if (charsRead >= 1 && (passwordBuf[charsRead - 1] == L'\r' || passwordBuf[charsRead - 1] == L'\n')) {
        passwordBuf[charsRead - 1] = L'\0';
    }
    
    fwprintf(stderr, L"\n");
    return TRUE;
}

/**
 * @brief Securely reads a password from the console without echoing characters.
 *
 * This function handles disabling console echo, reading the user's input as
 * wide characters, converting it to UTF-8, and placing it into a caller-provided
 * buffer.
 *
 * It is critical that the temporary wide-character buffer used internally is
 * wiped from memory before the function returns.
 *
 * @param[out] utf8PasswordBuf A caller-provided buffer to store the UTF-8 encoded password.
 * @param[in]  bufSize The total size in bytes of utf8PasswordBuf.
 * @param[out] pBytesWritten A pointer to an integer that will receive the number
 *                           of bytes written to the buffer, including the null terminator.
 * @return Returns TRUE on success, FALSE on failure (e.g., I/O error, buffer too small).
 */
BOOL GetPasswordFromConsole(char* utf8PasswordBuf, int bufSize, int* pBytesWritten)
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD originalMode = 0;
    // Temporary stack buffers for passwords
    wchar_t widePassword[VC_MAX_PASSWORD + 1] = { 0 };
    wchar_t confirmPassword[VC_MAX_PASSWORD + 1] = { 0 };
    DWORD charsRead = 0;
    DWORD confirmCharsRead = 0;
    BOOL success = FALSE;
	int requiredSize = 0;

    if (hStdin == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"\nError: Could not get standard input handle.\n");
        return FALSE;
    }
    if (!GetConsoleMode(hStdin, &originalMode)) {
        fwprintf(stderr, L"\nError: Could not get console mode.\n");
        return FALSE;
    }

    // Disable character echoing to hide the password.
    if (!SetConsoleMode(hStdin, originalMode & (~ENABLE_ECHO_INPUT))) {
        fwprintf(stderr, L"\nError: Could not set console mode to hide input.\n");
        return FALSE;
    }

    // First password entry
    if (!ReadAndCleanPassword(hStdin, L"Enter Password: ", widePassword, _countof(widePassword))) {
        goto cleanup;
    }

    // Second password entry for confirmation
    if (!ReadAndCleanPassword(hStdin, L"Confirm Password: ", confirmPassword, _countof(confirmPassword))) {
        goto cleanup;
    }

    // Compare passwords
    if (wcscmp(widePassword, confirmPassword) != 0) {
        fwprintf(stderr, L"Error: Passwords do not match. Please try again.\n");
        goto cleanup;
    }

    // Calculate the required buffer size for the UTF-8 conversion.
    requiredSize = WideCharToMultiByte(CP_UTF8, 0, widePassword, -1, NULL, 0, NULL, NULL);

    if (requiredSize == 0 && *widePassword != L'\0') {
        fwprintf(stderr, L"Error: Could not calculate required buffer size for password. Win32 Error: %lu\n", GetLastError());
    }
    else if (requiredSize > bufSize) {
        fwprintf(stderr, L"Error: The provided buffer is too small. Required: %d bytes, Provided: %d bytes.\n", requiredSize, bufSize);
    }
    else {
        // Convert the wide-character password to UTF-8 directly into the caller's buffer.
        int bytesWritten = WideCharToMultiByte(CP_UTF8, 0, widePassword, -1, utf8PasswordBuf, bufSize, NULL, NULL);
        if (bytesWritten > 0) {
            if (pBytesWritten) {
                *pBytesWritten = bytesWritten;
            }
            success = TRUE;
        }
        else {
            fwprintf(stderr, L"Error: Failed to convert password to UTF-8. Win32 Error: %lu\n", GetLastError());
            // In case of failure, wipe the destination buffer to be safe.
            SecureZeroMemory(utf8PasswordBuf, bufSize);
        }
    }

cleanup:
    // Always restore the original console mode.
    SetConsoleMode(hStdin, originalMode);
    
    // Always wipe both password buffers from the stack memory.
    SecureZeroMemory(widePassword, sizeof(widePassword));
    SecureZeroMemory(confirmPassword, sizeof(confirmPassword));

    return success;
}

const char* GetErrorMessage(int errorCode) {
    switch (errorCode) {
    case VCF_SUCCESS: return "Success";
    case VCF_ERROR_GENERIC: return "A generic or unknown error occurred.";
    case VCF_ERROR_INVALID_PARAMETER: return "An invalid parameter was passed (e.g., NULL path or invalid size).";
    case VCF_ERROR_PASSWORD_OR_KEYFILE_REQUIRED: return "A password and/or keyfile must be provided.";
    case VCF_ERROR_INVALID_ENCRYPTION_ALGORITHM: return "The specified encryption algorithm is not supported.";
    case VCF_ERROR_INVALID_HASH_ALGORITHM: return "The specified hash algorithm is not supported.";
    case VCF_ERROR_INVALID_FILESYSTEM: return "The specified filesystem is not supported.";
    case VCF_ERROR_PASSWORD_POLICY: return "Password is too long or violates a policy.";
    case VCF_ERROR_KEYFILE_ERROR: return "An error occurred while reading or processing a keyfile. Check if the file exists and is accessible.";
    case VCF_ERROR_OUT_OF_MEMORY: return "The system is out of memory.";
    case VCF_ERROR_OS_ERROR: return "A Windows API call failed. Check system logs for details.";
    case VCF_ERROR_CANNOT_GET_DEVICE_SIZE: return "Could not determine the size of the specified device.";
    case VCF_ERROR_VOLUME_SIZE_TOO_SMALL: return "The specified volume size is too small to be a valid VeraCrypt volume.";
    case VCF_ERROR_RNG_INIT_FAILED: return "The random number generator failed to initialize.";
    case VCF_ERROR_NO_DRIVER: return "The VeraCrypt driver is not installed or not running.";
    case VCF_ERROR_SELF_TEST_FAILED: return "A required cryptographic algorithm failed its self-test.";
    case VCF_ERROR_USER_ABORT: return "The operation was aborted by the user (via callback).";
    case VCF_ERROR_INITIALIZATION_FAILED: return "The SDK failed to initialize.";
    case VCF_ERROR_NOT_INITIALIZED: return "An SDK function was called before VeraCryptFormat_Initialize().";
    case VCF_ERROR_INVALID_VOLUME_SIZE: return "The volume size is not a multiple of the sector size (512 bytes).";
    case VCF_ERROR_FILESYSTEM_INVALID_FOR_SIZE: return "The selected filesystem is not valid for the given volume size.";
    case VCF_ERROR_CONTAINER_TOO_LARGE_FOR_HOST: return "The file container is larger than the available free space on the host volume.";
    case VCF_ERROR_ACCESS_DENIED: return "Access was denied. Check permissions for the path or run as Administrator.";
    default: return "An unknown error code was returned.";
    }
}

/**
 * @brief Parses a size string that may contain a suffix (KB, MB, GB, TB).
 * 
 * @param sizeStr The input string (e.g., "100", "1GB", "512MB")
 * @param pSize Pointer to store the parsed size in bytes
 * @return TRUE on success, FALSE on failure
 */
BOOL ParseSizeWithSuffix(const wchar_t* sizeStr, uint64_t* pSize)
{
    if (!sizeStr || !pSize) {
        return FALSE;
    }

    wchar_t* endPtr;
    uint64_t baseValue = _wcstoui64(sizeStr, &endPtr, 10);
    
    // Check for conversion errors
    if (endPtr == sizeStr) {
        return FALSE; // No digits were converted
    }

    // Skip whitespace after the number
    while (*endPtr == L' ' || *endPtr == L'\t') {
        endPtr++;
    }

    uint64_t multiplier = 1;
    
    if (*endPtr != L'\0') {
        // Parse the suffix
        if (_wcsicmp(endPtr, L"KB") == 0 || _wcsicmp(endPtr, L"KiB") == 0 || _wcsicmp(endPtr, L"K") == 0) {
            multiplier = 1024ULL;
        }
        else if (_wcsicmp(endPtr, L"MB") == 0 || _wcsicmp(endPtr, L"MiB") == 0 || _wcsicmp(endPtr, L"M") == 0) {
            multiplier = 1024ULL * 1024ULL;
        }
        else if (_wcsicmp(endPtr, L"GB") == 0 || _wcsicmp(endPtr, L"GiB") == 0 || _wcsicmp(endPtr, L"G") == 0) {
            multiplier = 1024ULL * 1024ULL * 1024ULL;
        }
        else if (_wcsicmp(endPtr, L"TB") == 0 || _wcsicmp(endPtr, L"TiB") == 0 || _wcsicmp(endPtr, L"T") == 0) {
            multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        }
        else {
            return FALSE; // Invalid suffix
        }
    }

    // Check for overflow
    if (baseValue > UINT64_MAX / multiplier) {
        return FALSE; // Would overflow
    }

    *pSize = baseValue * multiplier;
    return TRUE;
}

/**
 * @brief Converts a relative path to an absolute path.
 * 
 * @param relativePath The input path (relative or absolute)
 * @param absolutePath Pointer to store the allocated absolute path (caller must free)
 * @return TRUE on success, FALSE on failure
 */
BOOL GetAbsolutePath(const wchar_t* relativePath, wchar_t** absolutePath)
{
    if (!relativePath || !absolutePath) {
        return FALSE;
    }

    *absolutePath = NULL;

    // Check if path is already absolute
    if (PathIsRelativeW(relativePath)) {
        // Get the full path
        DWORD bufferSize = GetFullPathNameW(relativePath, 0, NULL, NULL);
        if (bufferSize == 0) {
            return FALSE;
        }

        *absolutePath = (wchar_t*)malloc(bufferSize * sizeof(wchar_t));
        if (!*absolutePath) {
            return FALSE;
        }

        DWORD result = GetFullPathNameW(relativePath, bufferSize, *absolutePath, NULL);
        if (result == 0 || result >= bufferSize) {
            free(*absolutePath);
            *absolutePath = NULL;
            return FALSE;
        }
    }
    else {
        // Path is already absolute, make a copy
        size_t pathLen = wcslen(relativePath) + 1;
        *absolutePath = (wchar_t*)malloc(pathLen * sizeof(wchar_t));
        if (!*absolutePath) {
            return FALSE;
        }
        wcscpy_s(*absolutePath, pathLen, relativePath);
    }

    return TRUE;
}

/**
 * @brief Checks if the given disk file path is a volume device path.
 *
 * This function performs a case-insensitive check if the provided path 
 * starts with "\\Device\\" indicating it is a volume device hosted by the system.
 *
 * @param lpszDiskFile The disk file path to check.
 * @return TRUE if it is a volume device path, FALSE otherwise.
 */
BOOL IsVolumeDeviceHosted(const wchar_t *lpszDiskFile)
{
    const wchar_t* devicePrefix = L"\\Device\\";
    size_t prefixLen = wcslen(devicePrefix);
    
    // Make sure the path is long enough to contain the prefix
    if (wcslen(lpszDiskFile) < prefixLen)
        return FALSE;
    
    // Use _wcsnicmp for case-insensitive comparison of the first part of the string
    return _wcsnicmp(lpszDiskFile, devicePrefix, prefixLen) == 0;
}
