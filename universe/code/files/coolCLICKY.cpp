#include <JuceHeader.h>

class MetronomeSynth : public juce::AudioAppComponent, private juce::Timer
{
public:
    MetronomeSynth()
    {
        setAudioChannels(0, 2); // No inputs, stereo output

        // Shift BPM range up by 3X
        bpm = juce::Random::getSystemRandom().nextFloat() * (240.0f - 135.0f) + 135.0f;
        startTimerHz(static_cast<int>(bpm / 60.0f)); // Convert BPM to timer frequency

        // Randomize key offset to a float between -8.0 and 2.0 semitones
        keyOffset = juce::Random::getSystemRandom().nextFloat() * (-8.0f) + 2.0f;

        // Initialize voice allocation index
        currentVoiceIndex = 0;
    }

    ~MetronomeSynth() override
    {
        shutdownAudio();
    }

    void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        currentSampleRate = sampleRate;

        // Set ADSR parameters for a plucky sound
        adsrParams.attack = 2.0f;
        adsrParams.decay = 2.8f;
        adsrParams.sustain = 0.0f;
        adsrParams.release = 3.0f;

        // Initialize voice parameters
        for (int i = 0; i < numVoices; ++i)
        {
            phases[i] = 0.0;
            lfoPhases[i] = 0.0;
            frequencies[i] = 440.0;
            phaseIncrements[i] = 0.0;
            voiceActive[i] = false;

            adsrs[i].setSampleRate(currentSampleRate);
            adsrs[i].setParameters(adsrParams);
            adsrs[i].reset();
        }

        // Vibrato settings
        vibratoRate = 0.4;
        vibratoDepth = 0.0;
        vibratoIncrement = (2.0 * juce::MathConstants<double>::pi * vibratoRate) / currentSampleRate;

        // Initialize chorus parameters with a larger buffer size
        chorusDelayBufferSize = static_cast<int>(currentSampleRate * 0.1); // 100ms max delay
        chorusDelayBuffer.setSize(2, chorusDelayBufferSize);
        chorusDelayBuffer.clear();
        chorusDelayBufferWritePosition = 0;

        chorusLfoRate = 0.1;        // Slow LFO rate for noticeable modulation
        chorusLfoDepth = 0.0f;    // Modulation depth in seconds (5ms)
        chorusLfoPhase = 0.0;
        chorusLfoIncrement = (2.0 * juce::MathConstants<double>::pi * chorusLfoRate) / currentSampleRate;
        chorusMix = 0.6f;           // Balanced mix level for smooth chorus effect

        // Initialize flanger parameters with a large buffer size
        flangerDelayBufferSize = static_cast<int>(currentSampleRate * 0.05); // 50ms max delay
        flangerDelayBuffer.setSize(2, flangerDelayBufferSize);
        flangerDelayBuffer.clear();
        flangerDelayBufferWritePosition = 0;

        flangerLfoRate = 0.3;       // Faster than vibrato and chorus LFOs
        flangerLfoDepth = 0.005f;   // Modulation depth in seconds (2ms)
        flangerLfoPhase = 0.0;
        flangerLfoIncrement = (2.0 * juce::MathConstants<double>::pi * flangerLfoRate) / currentSampleRate;
        flangerFeedback = 0.90f;    // 75% feedback
        flangerMix = 0.3f;          // 60% mix
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto* leftChannel  = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* rightChannel = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

        for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
        {
            float drySample = 0.0f;

            // Generate audio from voices
            for (int i = 0; i < numVoices; ++i)
            {
                if (adsrs[i].isActive())
                {
                    float envelopeValue = adsrs[i].getNextSample();

                    // Apply vibrato using an LFO
                    float vibrato = std::sin(lfoPhases[i]) * vibratoDepth;
                    float modulatedFrequency = frequencies[i] + vibrato;
                    float modulatedIncrement = (juce::MathConstants<double>::twoPi * modulatedFrequency) / currentSampleRate;

                    float voiceSample = std::sin(phases[i]) * envelopeValue;

                    phases[i] += modulatedIncrement;
                    if (phases[i] >= juce::MathConstants<double>::twoPi)
                        phases[i] -= juce::MathConstants<double>::twoPi;

                    lfoPhases[i] += vibratoIncrement;
                    if (lfoPhases[i] >= juce::MathConstants<double>::twoPi)
                        lfoPhases[i] -= juce::MathConstants<double>::twoPi;

                    drySample += voiceSample * amplitude;
                }
            }

            // Advance the chorus LFO phase
            chorusLfoPhase += chorusLfoIncrement;
            if (chorusLfoPhase >= juce::MathConstants<double>::twoPi)
                chorusLfoPhase -= juce::MathConstants<double>::twoPi;

            // Calculate the delayed sample with chorus LFO modulation
            float chorusDelayTime = 0.02f + (std::sin(chorusLfoPhase) * chorusLfoDepth); // Base delay + modulation
            float chorusDelaySamples = chorusDelayTime * currentSampleRate;

            int chorusDelaySamplesInt = static_cast<int>(chorusDelaySamples);
            float chorusFrac = chorusDelaySamples - chorusDelaySamplesInt;

            int chorusReadPosition1 = (chorusDelayBufferWritePosition - chorusDelaySamplesInt + chorusDelayBufferSize) % chorusDelayBufferSize;
            int chorusReadPosition2 = (chorusReadPosition1 - 1 + chorusDelayBufferSize) % chorusDelayBufferSize;

            float chorusSampleL1 = chorusDelayBuffer.getSample(0, chorusReadPosition1);
            float chorusSampleL2 = chorusDelayBuffer.getSample(0, chorusReadPosition2);
            float chorusSampleR1 = chorusDelayBuffer.getSample(1, chorusReadPosition1);
            float chorusSampleR2 = chorusDelayBuffer.getSample(1, chorusReadPosition2);

            // Linear interpolation for chorus
            float chorusSampleL = chorusSampleL1 + chorusFrac * (chorusSampleL2 - chorusSampleL1);
            float chorusSampleR = chorusSampleR1 + chorusFrac * (chorusSampleR2 - chorusSampleR1);

            // Blend the dry and chorus signals
            float chorusOutputL = drySample * (1.0f - chorusMix) + chorusSampleL * chorusMix;
            float chorusOutputR = drySample * (1.0f - chorusMix) + chorusSampleR * chorusMix;

            // Write the dry sample to the chorus delay buffer
            chorusDelayBuffer.setSample(0, chorusDelayBufferWritePosition, drySample);
            chorusDelayBuffer.setSample(1, chorusDelayBufferWritePosition, drySample);

            // Advance the chorus delay buffer write position
            chorusDelayBufferWritePosition = (chorusDelayBufferWritePosition + 1) % chorusDelayBufferSize;

            // Advance the flanger LFO phase
            flangerLfoPhase += flangerLfoIncrement;
            if (flangerLfoPhase >= juce::MathConstants<double>::twoPi)
                flangerLfoPhase -= juce::MathConstants<double>::twoPi;

            // Calculate the delayed sample with flanger LFO modulation
            float flangerDelayTime = 0.003f + (std::sin(flangerLfoPhase) * flangerLfoDepth); // Base delay + modulation
            float flangerDelaySamples = flangerDelayTime * currentSampleRate;

            int flangerDelaySamplesInt = static_cast<int>(flangerDelaySamples);
            float flangerFrac = flangerDelaySamples - flangerDelaySamplesInt;

            int flangerReadPosition1 = (flangerDelayBufferWritePosition - flangerDelaySamplesInt + flangerDelayBufferSize) % flangerDelayBufferSize;
            int flangerReadPosition2 = (flangerReadPosition1 - 1 + flangerDelayBufferSize) % flangerDelayBufferSize;

            float flangerDelayedSampleL1 = flangerDelayBuffer.getSample(0, flangerReadPosition1);
            float flangerDelayedSampleL2 = flangerDelayBuffer.getSample(0, flangerReadPosition2);
            float flangerDelayedSampleR1 = flangerDelayBuffer.getSample(1, flangerReadPosition1);
            float flangerDelayedSampleR2 = flangerDelayBuffer.getSample(1, flangerReadPosition2);

            // Linear interpolation for flanger
            float flangerDelayedSampleL = flangerDelayedSampleL1 + flangerFrac * (flangerDelayedSampleL2 - flangerDelayedSampleL1);
            float flangerDelayedSampleR = flangerDelayedSampleR1 + flangerFrac * (flangerDelayedSampleR2 - flangerDelayedSampleR1);

            // Calculate flanger input sample with feedback
            float flangerInputSampleL = chorusOutputL + flangerDelayedSampleL * flangerFeedback;
            float flangerInputSampleR = chorusOutputR + flangerDelayedSampleR * flangerFeedback;

            // Write the flanger input sample into the flanger delay buffer
            flangerDelayBuffer.setSample(0, flangerDelayBufferWritePosition, flangerInputSampleL);
            flangerDelayBuffer.setSample(1, flangerDelayBufferWritePosition, flangerInputSampleR);

            // Mix the flanged sample with the chorus output
            float outputSampleL = chorusOutputL * (1.0f - flangerMix) + flangerDelayedSampleL * flangerMix;
            float outputSampleR = chorusOutputR * (1.0f - flangerMix) + flangerDelayedSampleR * flangerMix;

            // Advance the flanger delay buffer write position
            flangerDelayBufferWritePosition = (flangerDelayBufferWritePosition + 1) % flangerDelayBufferSize;

            // Write the final output samples to the output buffer
            leftChannel[sample]  = outputSampleL;
            rightChannel[sample] = outputSampleR;
        }
    }

    void releaseResources() override {}

private:
    static const int numVoices = 12;

    double currentSampleRate = 44100.0;
    double amplitude = 0.5;

    double phases[numVoices];
    double lfoPhases[numVoices];
    double frequencies[numVoices];
    double phaseIncrements[numVoices];
    bool   voiceActive[numVoices];

    juce::ADSR adsrs[numVoices];
    juce::ADSR::Parameters adsrParams;

    float bpm;
    float keyOffset;

    double vibratoRate;
    double vibratoDepth;
    double vibratoIncrement;

    // Chorus-specific parameters
    juce::AudioBuffer<float> chorusDelayBuffer;
    int chorusDelayBufferSize;
    int chorusDelayBufferWritePosition;

    double chorusLfoRate;
    double chorusLfoDepth;
    double chorusLfoPhase;
    double chorusLfoIncrement;
    float chorusMix;

    // Flanger-specific parameters
    juce::AudioBuffer<float> flangerDelayBuffer;
    int flangerDelayBufferSize;
    int flangerDelayBufferWritePosition;

    double flangerLfoRate;
    double flangerLfoDepth;
    double flangerLfoPhase;
    double flangerLfoIncrement;
    float flangerFeedback;
    float flangerMix;

    // Weight configuration for note selection
    std::vector<std::pair<int, float>> weightedNotes = {
        {48, 4}, {50, 2}, {52, 6}, {53, 2}, {55, 4}, {57, 5}, {59, 3}
    };

    // Voice allocation index
    int currentVoiceIndex;

    // Timer callback for triggering notes
    void timerCallback() override
    {
        // Reduce the odds of triggering down to 1/3
        if (juce::Random::getSystemRandom().nextFloat() < 0.2667f)
        {
            int voiceIndex = currentVoiceIndex;

            currentVoiceIndex = (currentVoiceIndex + 1) % numVoices;

            int selectedNoteIndex = selectWeightedNoteIndex();
            int selectedNote = weightedNotes[selectedNoteIndex].first;

            // Apply octave transposition with specified probabilities
            float octaveRandom = juce::Random::getSystemRandom().nextFloat();
            if (octaveRandom < 0.08f) // 8% chance to go up 2 octaves
            {
                selectedNote += 24; // Up 2 octaves
            }
            else if (octaveRandom < 0.33f) // Additional 25% chance (total 33%) to go up 1 octave
            {
                selectedNote += 12; // Up 1 octave
            }

            // Apply key offset (transposition between -8 and 2 semitones)
            frequencies[voiceIndex] = midiNoteToFrequency(selectedNote + keyOffset);
            phaseIncrements[voiceIndex] = (juce::MathConstants<double>::twoPi * frequencies[voiceIndex]) / currentSampleRate;

            adsrs[voiceIndex].reset();
            adsrs[voiceIndex].noteOn();

            voiceActive[voiceIndex] = true;

            // Schedule note off after a duration
            juce::Timer::callAfterDelay(200, [this, voiceIndex]()
            {
                adsrs[voiceIndex].noteOff();
                voiceActive[voiceIndex] = false;
            });
        }
    }

    // Select a weighted note index based on probability distribution
    int selectWeightedNoteIndex()
    {
        float totalWeight = 0.0f;
        for (const auto& pair : weightedNotes)
            totalWeight += pair.second;

        float randomValue = juce::Random::getSystemRandom().nextFloat() * totalWeight;
        float cumulativeWeight = 0.0f;

        for (int i = 0; i < weightedNotes.size(); ++i)
        {
            cumulativeWeight += weightedNotes[i].second;
            if (randomValue <= cumulativeWeight)
                return i;
        }

        return weightedNotes.size() - 1; // Default to the last note if something goes wrong
    }

    // Convert MIDI note number to frequency
    double midiNoteToFrequency(double midiNoteNumber)
    {
        return 440.0 * std::pow(2.0, (midiNoteNumber - 69.0) / 12.0);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetronomeSynth)
};

// Main application entry point
class MetronomeApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MetronomeSynth"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow.reset(new MainWindow("Metronome Synth", new MetronomeSynth(), *this));
    }

    void shutdown() override
    {
        mainWindow = nullptr; // Clean up
    }

    void anotherInstanceStarted(const juce::String&) override {}

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name, juce::AudioAppComponent* mainComponent, juce::JUCEApplication& app)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons),
              app(app)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(mainComponent, true);
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            app.systemRequestedQuit();
        }

    private:
        juce::JUCEApplication& app;
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(MetronomeApp)