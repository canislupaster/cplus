#include <stdio.h>

#include "term.h"

#if _WIN32

#ifndef WINDOWS_INC
#include "windows.h"
#endif //WINDOWS_INC

void set_col(FILE* f, char color) {
    DWORD output;
    if (f==stdout) output = STD_OUTPUT_HANDLE;
    else if (f==stderr) output = STD_ERROR_HANDLE;
    else return;

    SetConsoleTextAttribute(GetStdHandle(output), (unsigned)color);
}

#else // WINDOWS

void set_col(FILE* f, char color) {
  fprintf(f, "\x1b[%im", (char)color);
}

#endif // WINDOWS