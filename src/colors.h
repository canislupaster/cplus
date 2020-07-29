#pragma once

#if _WIN32

#ifndef WINDOWS_INC
#include <windows.h>
#endif //WINDOWS_INC

enum COLORS {
		BLACK=0,
		DARK_BLUE=1,
		DARK_GREEN=2,
		DARK_AQUA,DARK_CYAN=3,
		DARK_RED=4,
		DARK_PURPLE=5,DARK_PINK=5,DARK_MAGENTA=5,
		DARK_YELLOW=6,
		DARK_WHITE=7,
		GRAY=8,
		BLUE=9,
		GREEN=10,
		AQUA=11,CYAN=11,
		RED=12,
		PURPLE=13,PINK=13,MAGENTA=13,
		YELLOW=14,
		WHITE=15
};

void set_col(FILE* f, char color) {
	DWORD output;
	if (f==stdout) output = STD_OUTPUT_HANDLE;
	else if (f==stderr) output = STD_ERROR_HANDLE;
	else return;

	SetConsoleTextAttribute(GetStdHandle(output), (unsigned)color);
}

#else // WINDOWS

enum TERM_COLORS {
	RESET = 0,
	BLACK = 30,
	RED, GREEN,
	YELLOW, BLUE,
	MAGENTA, CYAN,
	GRAY,
	WHITE = 97
};

void set_col(FILE* f, char color) {
	fprintf(f, "\x1b[%im", (char) color);
}

#endif // WINDOWS