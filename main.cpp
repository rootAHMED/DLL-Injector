#include <windows.h>
#include <tlhelp32.h>
#include <iostream>

// Function to get the process ID (PID) of a running process by its executable name
DWORD GetProcessId(const WCHAR* szExeName) {
    DWORD pid = 0;
    // Take a snapshot of all the processes currently running
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        // Structure to store process information
        PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
        
        // Check the first process in the snapshot
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                // Compare the executable name to find the correct process
                if (_wcsicmp(szExeName, pe.szExeFile) == 0) {
                    pid = pe.th32ProcessID; // Store the process ID
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe)); // Continue checking the next processes
        }
        // Close the snapshot handle after use
        CloseHandle(hSnapshot);
    }
    return pid; // Return the process ID
}

int wmain() {
    // Path to the DLL to be injected (update this to your DLL's path)
    const WCHAR* dllPath = L"C:\\..\\..\\..\\MessageBoxDLL.dll";
    
    // Get the process ID of notepad.exe
    DWORD pid = GetProcessId(L"notepad.exe");
    if (pid == 0) {
        std::wcout << L"Notepad process not found!" << std::endl; // If the process isn't found
        return 1;
    }
    std::wcout << L"Notepad PID: " << pid << std::endl; // Print the process ID

    // Open the target process with necessary permissions (all access)
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (hProcess == NULL) {
        std::wcout << L"Failed to open process. Error: " << GetLastError() << std::endl; // If opening the process fails
        return 1;
    }

    // Allocate memory in the target process to store the DLL path
    SIZE_T dllPathSize = (wcslen(dllPath) + 1) * sizeof(WCHAR); // Calculate the size of the DLL path
    LPVOID remoteMemory = VirtualAllocEx(hProcess, NULL, dllPathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); // Allocate memory in the target process
    if (remoteMemory == NULL) {
        std::wcout << L"Failed to allocate memory. Error: " << GetLastError() << std::endl; // If memory allocation fails
        CloseHandle(hProcess); // Close the process handle
        return 1;
    }

    // Write the DLL path to the allocated memory in the target process
    if (!WriteProcessMemory(hProcess, remoteMemory, dllPath, dllPathSize, NULL)) {
        std::wcout << L"Failed to write memory. Error: " << GetLastError() << std::endl; // If writing to memory fails
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE); // Free the allocated memory
        CloseHandle(hProcess); // Close the process handle
        return 1;
    }

    // Get the address of the LoadLibraryW function from kernel32.dll (used to load the DLL)
    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    if (loadLibraryAddr == NULL) {
        std::wcout << L"Failed to get LoadLibraryW address. Error: " << GetLastError() << std::endl; // If getting the function address fails
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE); // Free the allocated memory
        CloseHandle(hProcess); // Close the process handle
        return 1;
    }

    // Create a remote thread in the target process to call LoadLibraryW and load the DLL
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteMemory, 0, NULL);
    if (hThread == NULL) {
        std::wcout << L"Failed to create remote thread. Error: " << GetLastError() << std::endl; // If creating the thread fails
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE); // Free the allocated memory
        CloseHandle(hProcess); // Close the process handle
        return 1;
    }

    // Wait for the remote thread to finish executing
    WaitForSingleObject(hThread, INFINITE);
    std::wcout << L"DLL injected successfully!" << std::endl; // If DLL injection succeeds

    // Clean up resources after the DLL injection
    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE); // Free the allocated memory in the target process
    CloseHandle(hThread); // Close the remote thread handle
    CloseHandle(hProcess); // Close the process handle
    
    return 0; // End the program successfully
}
