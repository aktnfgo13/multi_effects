#include "effects/TimeEffects.h"
#include "effects/AmpSim.h"
#include "effects/Cabinet.h"
#include <iostream>
#include <iomanip>
#include <string>
int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    const std::string mode = argv[1];
    const float rate = std::stof(argv[2]);
    multi_effects::Delay delay; delay.prepare(rate);
    multi_effects::Reverb reverb; reverb.prepare(rate);
    multi_effects::AmpSim amp; amp.prepare(rate);
    multi_effects::Tremolo tremolo; tremolo.prepare(rate);
    multi_effects::Cabinet cabinet; cabinet.prepare({.75f, -.25f, .125f});
    std::cout << std::setprecision(9);
    float x;
    while (std::cin >> x)
    {
        if (mode == "delay") x = delay.processSample(x);
        else if (mode == "reverb") x = reverb.processSample(x);
        else if (mode == "amp") x = amp.processSample(x);
        else if (mode == "tremolo") x = tremolo.processSample(x);
        else if (mode == "cabinet") x = cabinet.processSample(x);
        else return 1;
        std::cout << x << '\n';
    }
    return 0;
}
