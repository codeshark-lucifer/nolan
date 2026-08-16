#include "core/core.hpp"
#include <stdexcept>
#include <iostream>

int main()
{
    try
    {
        OLIA_ENGINE::Run(setup, loop);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
