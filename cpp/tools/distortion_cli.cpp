#include "effects/Distortion.h"

#include <iomanip>
#include <iostream>
#include <string>


int main(
    int argc,
    char* argv[]
)
{
    if (argc < 5)
    {
        std::cerr
            << "Usage: distortion_cli "
            << "<hard|soft> "
            << "<drive_db> "
            << "<threshold> "
            << "<samples...>"
            << std::endl;

        return 1;
    }


    const std::string mode =
        argv[1];

    const float driveDb =
        std::stof(
            argv[2]
        );

    const float threshold =
        std::stof(
            argv[3]
        );


    multi_effects::Distortion distortion;


    if (mode == "hard")
    {
        distortion.setMode(
            multi_effects::DistortionMode::HardClip
        );
    }
    else if (mode == "soft")
    {
        distortion.setMode(
            multi_effects::DistortionMode::SoftClip
        );
    }
    else
    {
        std::cerr
            << "Invalid distortion mode."
            << std::endl;

        return 1;
    }


    distortion.setDriveDb(
        driveDb
    );

    distortion.setThreshold(
        threshold
    );


    std::cout
        << std::fixed
        << std::setprecision(9);


    for (
        int i = 4;
        i < argc;
        ++i
    )
    {
        const float input =
            std::stof(
                argv[i]
            );


        std::cout
            << distortion.processSample(
                input
            )
            << '\n';
    }


    return 0;
}