#include "effects/Gain.h"

#include <iomanip>
#include <iostream>
#include <string>


int main(
    int argc,
    char* argv[]
)
{
    if (argc < 3)
    {
        std::cerr
            << "Usage: gain_cli <gain_db> <sample1> <sample2> ..."
            << std::endl;

        return 1;
    }


    const float gainDb =
        std::stof(
            argv[1]
        );


    multi_effects::Gain gain;

    gain.setGainDb(
        gainDb
    );


    std::cout
        << std::fixed
        << std::setprecision(9);


    for (
        int i = 2;
        i < argc;
        ++i
    )
    {
        const float input =
            std::stof(
                argv[i]
            );

        const float output =
            gain.processSample(
                input
            );

        std::cout
            << output
            << '\n';
    }


    return 0;
}