// The tail length reported to Logic, in seconds. One number for the plugin and the engine test (which checks it).
// Measured with tools/engine_test, section 6: noise burst through the side chain, time until the output is 80 dB down,
// with Logic's tempo at 40, 60 and 120 BPM (the plugin treats slower tempos as 40 BPM). The longest of the "covered" settings
// listed there is 10.5 s (echo Repeats 0.9, Density 0.5, 40 BPM); 12 s is that plus a margin.
// Not covered (they ring much longer or for ever, by the firmware's design): reverb Decay at full with Spacetime away from
// about 0.6 (and Decay 0.75 with Spacetime 0.3: 24 s), resonator Feedback 0.9 and up, echo Repeats at full, echo Repeats 0.7 and
// up with Density at full. The looper is not part of the tail.
#pragma once
namespace aether
{
constexpr double kTailSeconds = 12.0;
}
