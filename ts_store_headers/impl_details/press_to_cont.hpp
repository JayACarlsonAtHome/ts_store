// ts_store/ts_store_headers/impl_details/press_to_cont.hpp

#pragma once

#include <iostream>
#include <limits>

void press_any_key()
{
    std::cout << "\nPress ENTER to continue...\n";
    //std::cin.clear();
    //std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();



}