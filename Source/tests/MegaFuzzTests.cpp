/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include "../PluginTests.h"
#include "../TestUtilities.h"
#include <future>
#include <thread>

//==============================================================================
static inline int countTooLoud (juce::AudioBuffer<float>& ab, float max) noexcept
{
    int count = 0;
    iterateAudioBuffer (ab, [&count, max] (float s)
                            {
                                if (s < -max || s > max)
                                    ++count;
                            });
    return count;
}

struct MegaFuzzTest  : public PluginTest
{
    MegaFuzzTest()
        : PluginTest ("Mega fuzz", 10)
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        const bool subnormalsAreErrors = ut.getOptions().strictnessLevel > 5;
        //const bool isPluginInstrument = instance.getPluginDescription().isInstrument;

        const std::vector<double>& sampleRates = ut.getOptions().sampleRates;
        const int blockSize = 512;

        jassert (sampleRates.size() > 0);
        callReleaseResourcesOnMessageThreadIfVST3 (instance);
        callPrepareToPlayOnMessageThreadIfVST3 (instance, sampleRates[0], blockSize);

        auto r = ut.getRandom();

        for (auto sr : sampleRates)
        {
            // Test one hour's worth of processing (test runs faster than realtime)
            const int totalSize = int(sr * 60.0 * 60.0);

            ut.logMessage (juce::String ("Testing with sample rate [SR]").replace ("SR",juce::String (sr, 0), false));

            callReleaseResourcesOnMessageThreadIfVST3 (instance);
            callPrepareToPlayOnMessageThreadIfVST3 (instance, sr, blockSize);

            int numSamplesDone = 0;
            const int numChannelsRequired = juce::jmax (instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
            juce::AudioBuffer<float> ab (numChannelsRequired, blockSize);
            juce::MidiBuffer mb;

            for (;;)
            {
                const int subBlockSize = r.nextInt (blockSize + 1);

                // TODO: add synth stuff back in (trigger notes at random points)
                // // Add a random note on if the plugin is a synth
                // const int noteChannel = r.nextInt ({ 1, 17 });
                // const int noteNumber = r.nextInt (128);

                // if (isPluginInstrument)
                //     addNoteOn (mb, noteChannel, noteNumber, juce::jmin (10, subBlockSize));

                // Set random parameter values
                {
                    auto parameters = getNonBypassAutomatableParameters (instance);

                    int numParamsToChange = r.nextInt (parameters.size());
                    for (int i = 0; i < numParamsToChange; ++i)
                    {
                        const int paramIndex = r.nextInt (parameters.size());
                        parameters[paramIndex]->setValue (r.nextFloat());
                    }
                }

                // TODO: add synth stuff back in
                // // Trigger a note off in the last sub block
                // if (isPluginInstrument && (bs - numSamplesDone) <= subBlockSize)
                //     addNoteOff (mb, noteChannel, noteNumber, juce::jmin (10, subBlockSize));

                // Create a sub-buffer and process
                juce::AudioBuffer<float> subBuffer (ab.getArrayOfWritePointers(),
                                                    ab.getNumChannels(),
                                                    0,
                                                    subBlockSize);
                fillNoise (subBuffer);
                instance.processBlock (subBuffer, mb);
                numSamplesDone += subBlockSize;

                mb.clear();

                ut.expectEquals (countNaNs (subBuffer), 0, "NaNs found in buffer");
                ut.expectEquals (countInfs (subBuffer), 0, "Infs found in buffer");
                ut.expectEquals (countTooLoud (subBuffer, 10.0f), 0, "samples over 20 dB found in buffer");

                const int subnormals = countSubnormals (subBuffer);
                if (subnormalsAreErrors)
                    ut.expectEquals (subnormals, 0, "Subnormals found in buffer");
                else if (subnormals > 0)
                    ut.logMessage ("!!! WARNING: " + juce::String (subnormals) + " subnormals found in buffer");

                if (numSamplesDone >= totalSize)
                    break;
            }
        }
    }
};

static MegaFuzzTest megaFuzzTest;
