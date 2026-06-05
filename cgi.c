#include <stdio.h>
#include <windows.h>

int main() {
    HANDLE hRead, hWrite;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = 0;

    BOOL result = CreatePipe(&hRead, &hWrite, &sa, 0);
    if (!result) {
        fprintf(stderr, "Failed to create pipe\n");
        return 1;
    }

    char cmd[] = "ping www.baidu.com";

    STARTUPINFOA si;
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = hWrite;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    PROCESS_INFORMATION pi = { 0 };

    BOOL processResult = CreateProcessA(
        NULL,   // No module name (use command line)
        cmd, // Command line
        NULL,   // Process handle not inheritable
        NULL,   // Thread handle not inheritable
        TRUE,   // Set handle inheritance to TRUE
        0,      // No creation flags
        NULL,   // Use parent's environment block
        NULL,   // Use parent's starting directory 
        &si,    // Pointer to STARTUPINFO structure
        &pi     // Pointer to PROCESS_INFORMATION structure
    );

    if (!processResult) {
        fprintf(stderr, "Failed to create process\n");
        return 1;
    }

    // test pipe
    char buffer[128];
    DWORD bytesRead;

    while(1) {
        // printf("Enter a message to send through the pipe (or 'exit' to quit): ");
        // fgets(buffer, sizeof(buffer), stdin);
        // if (strncmp(buffer, "exit", 4) == 0) {
        //     break;
        // }
        // WriteFile(hWrite, buffer, strlen(buffer), &bytesRead, NULL);

        ReadFile(hRead, buffer, sizeof(buffer), &bytesRead, NULL);
        buffer[bytesRead] = '\0';

        printf("Received from pipe: %s\n", buffer);
    }

    return 0;
}