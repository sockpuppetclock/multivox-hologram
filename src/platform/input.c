#include "input.h"

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct termios termzero;

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &termzero);
}

void input_set_nonblocking(void) {
    struct termios term;
    
    tcgetattr(STDIN_FILENO, &term);
    memcpy(&termzero, &term, sizeof(term));

    atexit(reset_terminal_mode);

    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    int fc = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, fc | O_NONBLOCK);
}


