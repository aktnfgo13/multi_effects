#include "effects/Compressor.h"

#include <iomanip>
#include <iostream>


int main(
    int argc,
    char* argv[]
)
{
    if (argc < 8)
    {
        std::cerr
            << "Usage: compressor_cli "
            << "<sample_rate> "
            << "<threshold_db> "
            << "<ratio> "
            << "<attack_ms> "
            << "<release_ms> "
            << "<makeup_gain_db> "
            << "<samples...>"
            << std::endl;

        return 1;
    }


    const float sampleRate =
        std::stof(argv[1]);

    const float thresholdDb =
        std::stof(argv[2]);

    const float ratio =
        std::stof(argv[3]);

    const float attackMs =
        std::stof(argv[4]);

    const float releaseMs =
        std::stof(argv[5]);

    const float makeupGainDb =
        std::stof(argv[6]);


    multi_effects::Compressor compressor;

    compressor.setSampleRate(sampleRate);
    compressor.setThresholdDb(thresholdDb);
    compressor.setRatio(ratio);
    compressor.setAttackMs(attackMs);
    compressor.setReleaseMs(releaseMs);
    compressor.setMakeupGainDb(makeupGainDb);


    std::cout
        << std::fixed
        << std::setprecision(9);


    for (
        int i = 7;
        i < argc;
        ++i
    )
    {
        const float input =
            std::stof(argv[i]);

        const float output =
            compressor.processSample(input);

        std::cout
            << output
            << '\n';
    }


    return 0;
}