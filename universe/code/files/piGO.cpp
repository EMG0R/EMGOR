//Generative Synth !!

//run program to generate a new composition with a unique key and BPM
//great for sleep, focus and mood

#include <JuceHeader.h>

class GenerativeSynth : public juce::AudioAppComponent, private juce::Timer
{
public:
    GenerativeSynth()
    {
        setAudioChannels(0, 2);

        // BPM range
        bpm = juce::Random::getSystemRandom().nextFloat() * (120.0f - 65.0f) + 65.0f;
        startTimerHz(static_cast<int>(bpm / 50.0f)); // BPM to timer

        // random key across octave starting at Bb
        keyOffset = juce::Random::getSystemRandom().nextFloat() * (-8.0f) + 2.0f;

        // voice allocation index
        currentVoiceIndex = 0;


        // Initialize weightedNotes with random weights
        std::vector<int> cMajorScale = {48, 50, 52, 53, 55, 57, 59};
        for (int note : cMajorScale)
        {
            float randomWeight = juce::Random::getSystemRandom().nextFloat() * 10.0f;

            // Add the base note with its weight
            weightedNotes.emplace_back(note, randomWeight);

            // Add F# (54) if note is F (53) with a 20% chance
            if (note == 53 && juce::Random::getSystemRandom().nextFloat() < 0.2f)
            {
                float sharpWeight = juce::Random::getSystemRandom().nextFloat() * 5.0f; 
                weightedNotes.emplace_back(54, sharpWeight);
            }

            // Add G# (56) if note is G (55) with a 20% chance
            if (note == 55 && juce::Random::getSystemRandom().nextFloat() < 0.2f)
            {
                float sharpWeight = juce::Random::getSystemRandom().nextFloat() * 5.0f; 
                weightedNotes.emplace_back(56, sharpWeight);
            }
        }
    }

    ~GenerativeSynth() override //destructor !!
    {
        shutdownAudio(); 
    }

void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override
{
    currentSampleRate = sampleRate;

    // ADSR ranges
    float minAttack = 1.5f;
    float maxAttack = 4.0f;
    float minDecayRelease = 4.0f;
    float maxDecayRelease = 9.0f;

    for (int i = 0; i < numVoices; ++i)
    {
        // randomize envelope ADSR
        adsrParams.attack = juce::Random::getSystemRandom().nextFloat() * (maxAttack - minAttack) + minAttack;
        adsrParams.decay = juce::Random::getSystemRandom().nextFloat() * (maxDecayRelease - minDecayRelease) + minDecayRelease;
        adsrParams.sustain = 0.0f;
        adsrParams.release = juce::Random::getSystemRandom().nextFloat() * (maxDecayRelease - minDecayRelease) + minDecayRelease;

        adsrs[i].setSampleRate(currentSampleRate); // set envelope sample rate
        adsrs[i].setParameters(adsrParams);        // apply ADSR
        adsrs[i].reset();                          // reset 

        // randomize FM ADSR
        fmAdsrParams.attack = juce::Random::getSystemRandom().nextFloat() * (maxAttack - minAttack) + minAttack;
        fmAdsrParams.decay = juce::Random::getSystemRandom().nextFloat() * (maxDecayRelease - minDecayRelease) + minDecayRelease;
        fmAdsrParams.sustain = 0.0f;
        fmAdsrParams.release = juce::Random::getSystemRandom().nextFloat() * (maxDecayRelease - minDecayRelease) + minDecayRelease;

        fmAdsrs[i].setSampleRate(currentSampleRate); // set FM envelope sample rate
        fmAdsrs[i].setParameters(fmAdsrParams);      // apply FM ADSR
        fmAdsrs[i].reset();                          // Reset


        // initialize parameters for each voice
        phases[i] = 0.0;
        lfoPhases[i] = 0.0;
        frequencies[i] = 440.0;
        modulatorPhases[i] = 0.0;
        voiceActive[i] = false;
    }

        // vibrato
        vibratoRate = 0.71;
        vibratoDepth = 2.2;
        vibratoIncrement = (2.0 * juce::MathConstants<double>::pi * vibratoRate) / currentSampleRate;

        // chorus
        chorusDelayBufferSize = static_cast<int>(currentSampleRate * 0.1); // 100ms max delay
        chorusDelayBuffer.setSize(2, chorusDelayBufferSize);
        chorusDelayBuffer.clear();
        chorusDelayBufferWritePosition = 0;

        // chorus
        chorusLfoRate = 0.11;
        chorusLfoDepth = 0.005f;
        chorusLfoPhase = 0.0;
        chorusLfoIncrement = (2.0 * juce::MathConstants<double>::pi * chorusLfoRate) / currentSampleRate;
        chorusMix = 0.6f;


        // flanger
        flangerDelayBufferSize = static_cast<int>(currentSampleRate * 0.05); // 50ms max delay
        flangerDelayBuffer.setSize(2, flangerDelayBufferSize);
        flangerDelayBuffer.clear();
        flangerDelayBufferWritePosition = 0;
        flangerLfoRate = 0.15;     
        flangerLfoDepth = 0.0022f;   
        flangerLfoPhase = 0.0;
        flangerLfoIncrement = (2.0 * juce::MathConstants<double>::pi * flangerLfoRate) / currentSampleRate;
        flangerFeedback = 0.99f;    
        flangerMix = 0.01f;        


        // phaser
        phaserLfoRate = 0.19; 
        phaserLfoPhase = 0.0;
        phaserLfoIncrement = (2.0 * juce::MathConstants<double>::pi * phaserLfoRate) / currentSampleRate;
        phaserMix = 0.5f; 
        numPhaserStages = 8; 
        phaserStages.resize(numPhaserStages);

        // vector of phaserStage objects for allpass filter network
        for (auto& stage : phaserStages)
        {
            stage.feedback = 0.6f; // feedback start
            stage.buffer = 0.0f;   // clear buffer
        }
        // all pzss filter coefficients
        for (auto& stage : phaserStages)
        {
            stage.feedback = 0.09f; //feedback
            stage.buffer = 0.09f; //bufferr
        }



    // reverb 
    reverbParams.roomSize = 0.99f;    
    reverbParams.damping = 0.6f;  
    reverbParams.wetLevel = 0.5f;   
    reverbParams.dryLevel = 0.5f;   
    reverbParams.width = 1.0f;      
    reverbParams.freezeMode = 0.0f; 
    reverb.setParameters(reverbParams);

    }

// generate and process synth audio
void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
{
    static bool fadeInActive = true;
    static int fadeInSamples = 120; // 120 sample fade-in 
    static int currentFadeInSample = 0;

    // retrieves pointers for left and right audio channels
    auto* leftChannel = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* rightChannel = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

    // process each sample in buffer
    for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
    {
        float drySample = 0.0f;

        for (int i = 0; i < numVoices; ++i)
        {
            if (adsrs[i].isActive())
            {
                float amplitudeEnvelope = adsrs[i].getNextSample();
                float fmEnvelope = fmAdsrs[i].getNextSample();
                float modulationIndex = fmEnvelope * fmMaxIndex;

                float modulatorFrequency = frequencies[i] * fmRatios[i];
                float modulatorIncrement = (juce::MathConstants<double>::twoPi * modulatorFrequency) / currentSampleRate;
                modulatorPhases[i] += modulatorIncrement;

                if (modulatorPhases[i] >= juce::MathConstants<double>::twoPi)
                    modulatorPhases[i] -= juce::MathConstants<double>::twoPi;

                float modulatorValue = modulationIndex * std::sin(modulatorPhases[i]);

                float carrierFrequency = frequencies[i];
                float carrierIncrement = (juce::MathConstants<double>::twoPi * carrierFrequency) / currentSampleRate;
                phases[i] += carrierIncrement;

                if (phases[i] >= juce::MathConstants<double>::twoPi)
                    phases[i] -= juce::MathConstants<double>::twoPi;

                float carrierSample = std::sin(phases[i] + modulatorValue);
                drySample += carrierSample * amplitudeEnvelope * amplitude;
            }
        }

        // normalize combined voices to prevent clipping
        drySample /= static_cast<float>(numVoices);

        // apply chorus/flanger/phaser
        float chorusSampleL, chorusSampleR;
        applyChorus(drySample, chorusSampleL, chorusSampleR);

        float flangerSampleL, flangerSampleR;
        applyFlanger(chorusSampleL, chorusSampleR, flangerSampleL, flangerSampleR);

        float phaserSampleL = applyPhaser(flangerSampleL);
        float phaserSampleR = applyPhaser(flangerSampleR);

        // fade-in 
        float fadeFactor = fadeInActive ? static_cast<float>(currentFadeInSample) / fadeInSamples : 1.0f;
        leftChannel[sample] = phaserSampleL * fadeFactor * masterGain;
        rightChannel[sample] = phaserSampleR * fadeFactor * masterGain;

        if (fadeInActive)
        {
            if (currentFadeInSample < fadeInSamples)
                ++currentFadeInSample;
            else
                fadeInActive = false;
        }
    }

    // apply reverb to the final audio buffer
    reverb.processStereo(leftChannel, rightChannel, bufferToFill.numSamples);
}

    void releaseResources() override {}

private:
    // establish private var

    static const int numVoices = 12;

    // normal stuff
    double currentSampleRate = 44100.0;
    double amplitude = 0.9;
    float bpm;
    float keyOffset;
    float masterGain = 1.0f; // adjust as needed to prevent clipping

    // weights for note selection
    std::vector<std::pair<int, float>> weightedNotes;

    // voice allocation index
    int currentVoiceIndex;



    // adsr
    juce::ADSR adsrs[numVoices];
    juce::ADSR::Parameters adsrParams;

    // fm stuff
    juce::ADSR fmAdsrs[numVoices];               // FM ADSR emv pervoice
    juce::ADSR::Parameters fmAdsrParams;         // FM ADSR param
    double fmMaxIndex = 4.0;                   // max FM modulation index 
    int fmRatios[numVoices]; // random FM ratio for each voice (0 to 8)
    double modulatorPhases[numVoices]; // FM modulator phase for each voice




    // phasor
    double phases[numVoices];
    double lfoPhases[numVoices];
    double frequencies[numVoices];
    double phaseIncrements[numVoices];
    bool   voiceActive[numVoices];
    double phaserLfoRate = 0.3;       
    double phaserLfoPhase = 0.0;     
    double phaserLfoIncrement;       
    float phaserMix = 0.7f;  
        
    struct PhaserStage
    {
        float feedback;
        float buffer;
    };

    std::vector<PhaserStage> phaserStages;
    int numPhaserStages;




    // vibrato
    double vibratoRate;
    double vibratoDepth;
    double vibratoIncrement;




    // chorus
    juce::AudioBuffer<float> chorusDelayBuffer;
    int chorusDelayBufferSize;
    int chorusDelayBufferWritePosition;
    double chorusLfoRate;
    double chorusLfoDepth;
    double chorusLfoPhase;
    double chorusLfoIncrement;
    float chorusMix;



    // flanger
    juce::AudioBuffer<float> flangerDelayBuffer;
    int flangerDelayBufferSize;
    int flangerDelayBufferWritePosition;
    double flangerLfoRate;
    double flangerLfoDepth;
    double flangerLfoPhase;
    double flangerLfoIncrement;
    float flangerFeedback;
    float flangerMix;


     

    juce::Reverb reverb;                 // JUCE Reverb object
    juce::Reverb::Parameters reverbParams; // reverb parameters



// periodic event handler that triggers whenever the timer associated with the class fires
void timerCallback() override
{
    if (juce::Random::getSystemRandom().nextFloat() < 1.0f) // trigger with 32% chance
    {
        int voiceIndex = currentVoiceIndex; // select the current voice
        currentVoiceIndex = (currentVoiceIndex + 1) % numVoices;

        int selectedNoteIndex = selectWeightedNoteIndex(); // randomly selects a note index based on weighted probabilities
        int selectedNote = weightedNotes[selectedNoteIndex].first; // retrieves MIDI note

        // apply key offset
        frequencies[voiceIndex] = midiNoteToFrequency(selectedNote + keyOffset);

        // assign a random FM ratio (0 to 8) to this voice
        fmRatios[voiceIndex] = juce::Random::getSystemRandom().nextInt(9); // Random integer between 0 and 8

        // reset and start amplitude ADSR
        adsrs[voiceIndex].reset();
        adsrs[voiceIndex].noteOn();

        // reset and start FM ADSR
        fmAdsrs[voiceIndex].reset();
        fmAdsrs[voiceIndex].noteOn();

        // marks the current voice as active
        voiceActive[voiceIndex] = true;

        // schedule note off after a duration
        juce::Timer::callAfterDelay(200, [this, voiceIndex]()
        {
            adsrs[voiceIndex].noteOff(); // signals the amplitude envelope to stop/transition to the release phase
            fmAdsrs[voiceIndex].noteOff(); // stop fm env
            voiceActive[voiceIndex] = false; // marks the voice as inactive
        });
    }
}

    // apply chorus
    void applyChorus(float drySample, float& chorusOutputL, float& chorusOutputR)
    {
        // advance chorus LFO phase
        chorusLfoPhase += chorusLfoIncrement;
        if (chorusLfoPhase >= juce::MathConstants<double>::twoPi)
            chorusLfoPhase -= juce::MathConstants<double>::twoPi;

        // calculate the delayed sample with chorus LFO modulation
        float chorusDelayTime = 0.02f + (std::sin(chorusLfoPhase) * chorusLfoDepth); // base delay + modulation
        float chorusDelaySamples = chorusDelayTime * currentSampleRate; // delay time from seconds to samples

        // separate integer and fractional parts of the delay
        int chorusDelaySamplesInt = static_cast<int>(chorusDelaySamples); // float to int
        float chorusFrac = chorusDelaySamples - chorusDelaySamplesInt; //calculates fractional part of the chorusDelaySamples value

        int chorusReadPosition1 = (chorusDelayBufferWritePosition - chorusDelaySamplesInt + chorusDelayBufferSize) % chorusDelayBufferSize; // calculates the first read position in the circular delay buffer for the chorus effect
        int chorusReadPosition2 = (chorusReadPosition1 - 1 + chorusDelayBufferSize) % chorusDelayBufferSize; // calculates the second read position one sample before chorusReadPosition1

        // reads the delayed samples for the left and right hannels from the buffer
        float chorusSampleL1 = chorusDelayBuffer.getSample(0, chorusReadPosition1);
        float chorusSampleL2 = chorusDelayBuffer.getSample(0, chorusReadPosition2);
        float chorusSampleR1 = chorusDelayBuffer.getSample(1, chorusReadPosition1);
        float chorusSampleR2 = chorusDelayBuffer.getSample(1, chorusReadPosition2);

        // linear interp chorus
        float chorusSampleL = chorusSampleL1 + chorusFrac * (chorusSampleL2 - chorusSampleL1);
        float chorusSampleR = chorusSampleR1 + chorusFrac * (chorusSampleR2 - chorusSampleR1);

        // blend chorus dry/wet
        chorusOutputL = drySample * (1.0f - chorusMix) + chorusSampleL * chorusMix;
        chorusOutputR = drySample * (1.0f - chorusMix) + chorusSampleR * chorusMix;

        // write the dry sample to the chorus delay buffer
        chorusDelayBuffer.setSample(0, chorusDelayBufferWritePosition, drySample);
        chorusDelayBuffer.setSample(1, chorusDelayBufferWritePosition, drySample);

        // advance the chorus delay buffer write position
        chorusDelayBufferWritePosition = (chorusDelayBufferWritePosition + 1) % chorusDelayBufferSize;
    }

    // apply flanger
    void applyFlanger(float inputL, float inputR, float& flangerOutputL, float& flangerOutputR)
    {
        // advance flanger LFO phase
        flangerLfoPhase += flangerLfoIncrement;
        if (flangerLfoPhase >= juce::MathConstants<double>::twoPi)
            flangerLfoPhase -= juce::MathConstants<double>::twoPi;

        // calculate the delayed sample with flanger LFO modulation
        float flangerDelayTime = 0.003f + (std::sin(flangerLfoPhase) * flangerLfoDepth); // Base delay + modulation
        float flangerDelaySamples = flangerDelayTime * currentSampleRate;

        // separate integer and fractional parts of delay
        int flangerDelaySamplesInt = static_cast<int>(flangerDelaySamples);
        float flangerFrac = flangerDelaySamples - flangerDelaySamplesInt;

        // calculate the read positions in the circular delay buffer
        int flangerReadPosition1 = (flangerDelayBufferWritePosition - flangerDelaySamplesInt + flangerDelayBufferSize) % flangerDelayBufferSize;
        int flangerReadPosition2 = (flangerReadPosition1 - 1 + flangerDelayBufferSize) % flangerDelayBufferSize;

        // retrieve delayed samples for left and right channels  
        float flangerDelayedSampleL1 = flangerDelayBuffer.getSample(0, flangerReadPosition1);
        float flangerDelayedSampleL2 = flangerDelayBuffer.getSample(0, flangerReadPosition2);
        float flangerDelayedSampleR1 = flangerDelayBuffer.getSample(1, flangerReadPosition1);
        float flangerDelayedSampleR2 = flangerDelayBuffer.getSample(1, flangerReadPosition2);

        // linear interpolation for flanger
        float flangerDelayedSampleL = flangerDelayedSampleL1 + flangerFrac * (flangerDelayedSampleL2 - flangerDelayedSampleL1);
        float flangerDelayedSampleR = flangerDelayedSampleR1 + flangerFrac * (flangerDelayedSampleR2 - flangerDelayedSampleR1);

        // calculate flanger input sample with feedback
        float flangerInputSampleL = inputL + flangerDelayedSampleL * flangerFeedback;
        float flangerInputSampleR = inputR + flangerDelayedSampleR * flangerFeedback;

        // write the flanger input sample into the flanger delay buffer
        flangerDelayBuffer.setSample(0, flangerDelayBufferWritePosition, flangerInputSampleL);
        flangerDelayBuffer.setSample(1, flangerDelayBufferWritePosition, flangerInputSampleR);

        // mix the flanged dry/wet
        flangerOutputL = inputL * (1.0f - flangerMix) + flangerDelayedSampleL * flangerMix;
        flangerOutputR = inputR * (1.0f - flangerMix) + flangerDelayedSampleR * flangerMix;

        // advance the flanger delay buffer write position
        flangerDelayBufferWritePosition = (flangerDelayBufferWritePosition + 1) % flangerDelayBufferSize;
    }

    // apply phasor
    float applyPhaser(float inputSample)
    {
        // advance phaser LFO phase
        phaserLfoPhase += phaserLfoIncrement;
        if (phaserLfoPhase >= juce::MathConstants<double>::twoPi)
            phaserLfoPhase -= juce::MathConstants<double>::twoPi;

        // calculate the modulation value for sweep
        float modulation = std::sin(phaserLfoPhase) * 0.5f + 0.5f;

        float output = inputSample;

        // apply each stage of the all-pass filters
        for (auto& stage : phaserStages)
        {
            float stageFeedback = stage.feedback + modulation * 0.1f;
            float delay = modulation * 0.5f + 0.3f;

            float temp = output + stageFeedback * stage.buffer;
            stage.buffer = temp * delay + stage.buffer * (1.0f - delay);
            output = stage.buffer;
        }

        // blend the phaser dry/wet
        float wetSignal = output * phaserMix;
        float drySignal = inputSample * (1.0f - phaserMix);

        return wetSignal + drySignal;
    }

// select note based on probability distribution
int selectWeightedNoteIndex()
{
    // calculate total weight of all notes
    float totalWeight = 0.0f;
    for (const auto& pair : weightedNotes)
        totalWeight += pair.second;

    // generate random value between 0 and total weight
    float randomValue = juce::Random::getSystemRandom().nextFloat() * totalWeight;

    // iterate through weighted notes and select based on cumulative weight
    float cumulativeWeight = 0.0f;
    for (int i = 0; i < weightedNotes.size(); ++i)
    {
        cumulativeWeight += weightedNotes[i].second;
        if (randomValue <= cumulativeWeight)
        {
            return i; // Return the index of the selected note
        }
    }

    // default case, if no note is selected, return index of last note
    return weightedNotes.size() - 1;
}

    // MTOF
    double midiNoteToFrequency(double midiNoteNumber)
    {
        return 440.0 * std::pow(2.0, (midiNoteNumber - 69.0) / 12.0);
    }
    // leak detection and non-copy
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenerativeSynth)
};

// main application
class GenerativeSynthApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "GenerativeSynth"; } // return GenerativeSynth
    const juce::String getApplicationVersion() override { return "1.0"; } // return version
    bool moreThanOneInstanceAllowed() override { return true; } // allows multiple instances

    void initialise(const juce::String&) override // initialize/command line args
    {
        mainWindow.reset(new MainWindow("GENERATING", new GenerativeSynth(), *this)); // creates main application window
    }

    // shutdown vibes
    void shutdown() override
    {
        mainWindow = nullptr; 
    }

    void anotherInstanceStarted(const juce::String&) override {} // multiple instances

// defines the main window class
private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        // initializes the MainWindow
        MainWindow(juce::String name, juce::AudioAppComponent* mainComponent, juce::JUCEApplication& app)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons),
              app(app)
        {
            // properties window n stuff
            setUsingNativeTitleBar(true);
            setContentOwned(mainComponent, true);
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(false);
        }

        // closen/quit
        void closeButtonPressed() override
        {
            app.systemRequestedQuit();
        }


    private:
        juce::JUCEApplication& app; // window to communicate with the application
    };

    std::unique_ptr<MainWindow> mainWindow; // manage lifecycle of MainWindow
};

START_JUCE_APPLICATION(GenerativeSynthApp) //initialize the JUCE runtime