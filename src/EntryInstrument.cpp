#include "ShellProcessor.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShellProcessor (ShellKind::Instrument); }
