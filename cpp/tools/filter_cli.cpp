#include "effects/Filters.h"

#include <iomanip>
#include <iostream>
#include <string>


int main(
    int argc,
    char* argv[]
)
{
    if (argc < 7)
    {
        std::cerr
            << "Usage: filter_cli "
            << "<mode> "
            << "<sample_rate> "
            << "<frequency> "
            << "<param1> "
            << "<param2> "
            << "<samples...>"
            << std::endl;

        return 1;
    }


    const std::string mode =
        argv[1];

    const float sampleRate =
        std::stof(argv[2]);

    const float frequency =
        std::stof(argv[3]);

    const float param1 =
        std::stof(argv[4]);

    const float param2 =
        std::stof(argv[5]);


    multi_effects::Filter filter;


    if (mode == "lowpass")
    {
        filter.setLowPass(
            sampleRate,
            frequency,
            param1
        );
    }
    else if (mode == "highpass")
    {
        filter.setHighPass(
            sampleRate,
            frequency,
            param1
        );
    }
    else if (mode == "lowshelf")
    {
        filter.setLowShelf(
            sampleRate,
            frequency,
            param1,
            param2
        );
    }
    else if (mode == "highshelf")
    {
        filter.setHighShelf(
            sampleRate,
            frequency,
            param1,
            param2
        );
    }
    else if (mode == "peaking")
    {
        filter.setPeaking(
            sampleRate,
            frequency,
            param1,
            param2
        );
    }
    else
    {
        std::cerr
            << "Invalid filter mode."
            << std::endl;

        return 1;
    }


    std::cout
        << std::fixed
        << std::setprecision(9);


    for (
        int i = 6;
        i < argc;
        ++i
    )
    {
        const float input =
            std::stof(argv[i]);

        std::cout
            << filter.processSample(input)
            << '\n';
    }


    return 0;
}