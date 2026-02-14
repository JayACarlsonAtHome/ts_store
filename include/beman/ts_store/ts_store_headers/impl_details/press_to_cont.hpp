#pragma once

#include <iostream>
#include <limits>
#include <cctype>

char press_any_key() const {
    std::print("\nPress ENTER to continue, Q to quit, E to jump to end Your response? ");

    while (std::cin.peek() != EOF) {
        char ch = static_cast<char>(std::cin.get());
        ch = static_cast<char>(std::toupper(ch));
        if (ch == 'Q' || ch == 'E' ) {
            return ch;
        }
        if (ch == '\n') return ' ';
    }
    return ' ';
}


