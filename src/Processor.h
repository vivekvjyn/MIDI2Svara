#pragma once
#include <JuceHeader.h>
#include <set>
#include <map>
#include <atomic>
#include "dsp/NoteData.h"
#include "midi/RecordingBuffer.h"
#include "midi/MidiOut.h"

class Processor : public AudioProcessor,
                              public AsyncUpdater,
                              private Timer
{
public:
    Processor();
    ~Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(AudioBuffer<float>&, MidiBuffer&) override;
    using AudioProcessor::processBlock;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool supportsMPE() const override { return true; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}

    void getStateInformation(MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    struct State
    {
        std::vector<NoteData> notes;
        double bpm = 120.0;
        float masterVolume = 0.8f;
        int midiOutputMode = 0;
        int midiPitchBendRange = 48;
        double viewStartBeat = 0.0;
        double viewEndBeat = 16.0;
        double viewLowest = 36.0;
        double viewHighest = 84.0;
    };

    State gatherState() const;
    void applyState(const State& state);

    std::function<void()> onStateLoaded;

    NoteSequence& getNoteSequence() { return noteSequence; }

    SpinLock sequenceLock;

    bool isPlaying() const { return playing.load(); }
    void setPlaying(bool p);

    void requestMidiPanic() { midiPanicRequested.store(true); }
    double getPlayheadBeat() const { return playheadBeat; }
    void setPlayheadBeat(double beat) { playheadBeat = beat; }
    double getBPM() const { return bpm; }
    void setBPM(double newBpm) { bpm = newBpm; }

    bool isDawSyncEnabled() const { return dawSyncEnabled; }
    void setDawSyncEnabled(bool enabled) { dawSyncEnabled = enabled; }
    bool isHostedInDAW() const { return wrapperType != wrapperType_Standalone; }

    bool isRecording() const { return recordingBuffer.isRecording(); }
    bool isRecordArmed() const { return recordArmed.load(); }
    void armRecording();
    void startRecording();
    void stopRecording();
    void disarmRecording();

    bool isOverdubEnabled() const { return overdubEnabled.load(); }
    void setOverdubEnabled(bool on) { overdubEnabled.store(on); }

    bool getPunchRange(double& start, double& end) const
    {
        if (!recordingBuffer.isRecording() || overdubEnabled.load()) return false;
        start = recordStartBeatInSequence;
        end = playheadBeat;
        return true;
    }

    float getExpressionDuration() const { return expressionDurationBeats.load(); }
    void setExpressionDuration(float beats) { expressionDurationBeats.store(beats); }
    float getExpressionIntensity() const { return expressionIntensity.load(); }
    void setExpressionIntensity(float v) { expressionIntensity.store(jlimit(0.0f, 2.0f, v)); }

    void setMasterVolume(float v) { masterVolume.store(jlimit(0.0f, 1.0f, v)); }
    float getMasterVolume() const { return masterVolume.load(); }

    void setMidiOutputMode(MidiOut::Mode m) { midiEngine.setMode(m); }
    MidiOut::Mode getMidiOutputMode() const { return midiEngine.getMode(); }
    MidiOut& getMidiOutput() { return midiEngine; }

    void setAmplitudeOutputMode(MidiOut::AmplitudeMode m) { midiEngine.setAmplitudeMode(m); }
    MidiOut::AmplitudeMode getAmplitudeOutputMode() const { return midiEngine.getAmplitudeMode(); }

    void setUseSegmentedPlayback(bool) {}
    bool getUseSegmentedPlayback() const { return false; }

    void injectNoteOn(int noteNumber, float velocity);
    void injectNoteOff(int noteNumber);
    void injectPitchWheel(int value14bit);
    void injectCC(int ccNumber, int value);

    struct LiveNoteState {
        std::set<int> activeNotes;
        String lastExpression;
        bool expressionActive = false;
    };
    LiveNoteState getLiveNoteState() const;

    struct RecordingPreviewPitch {
        double relTime = 0.0;
        float offset = 0.0f;
    };
    struct RecordingPreviewNote {
        int noteNumber = 60;
        float velocity = 0.8f;
        double startBeat = 0.0;
        double durationBeats = 0.0;
        std::vector<RecordingPreviewPitch> pitchCurve;
    };
    std::vector<RecordingPreviewNote> getRecordingPreview() const;

    void handleAsyncUpdate() override;
    void timerCallback() override;

    std::function<void()> onBeforeRecordingMerge;
    std::function<void()> onRecordingFinished;

private:
    static XmlElement stateToXml(const State& state);
    static State xmlToState(const XmlElement& xml);
    static void stateToBinary(const State& state, MemoryBlock& destData);
    static State binaryToState(const void* data, int sizeInBytes);
    static XmlElement noteToXml(const NoteData& note);
    static NoteData xmlToNote(const XmlElement& xml);
    static XmlElement pitchPointToXml(const PitchPoint& pt);
    static PitchPoint xmlToPitchPoint(const XmlElement& xml);
    static XmlElement vibratoToXml(const SegmentVibrato& vib);
    static SegmentVibrato xmlToVibrato(const XmlElement& xml);

    void processSequencePlayback(AudioBuffer<float>& buffer, MidiBuffer& midiMessages,
                                 double beatsPerSample, bool hostControlled);
    void processLiveMidi(MidiBuffer& midiMessages, double beatsPerSample);

    NoteSequence noteSequence;
    MidiOut midiEngine;

    std::atomic<bool> playing { false };
    double playheadBeat = 0.0;
    double bpm = 120.0;
    double sampleRate = 44100.0;

    bool hostWasPlaying = false;
    bool dawSyncEnabled = true;

    std::map<std::pair<int,double>, int> activeSeqNotes;

    struct LiveNote {
        int noteNumber = 0;
        float velocity = 0.0f;
        double startBeat = 0.0;
        int midiChannel = 1;
        int recordingId = -1;
    };
    std::vector<LiveNote> liveNotes;
    float livePitchBendSemitones = 0.0f;
    struct ActiveExpression {
        std::vector<PitchPoint> pattern;
        double durationBeats = 1.0;
        double triggerBeat = 0.0;
        bool active = false;
        String name;
        int targetNoteNumber = -1;
        float lastSentBend = 0.0f;

        float evaluate(double currentBeat) const;
    };
    ActiveExpression activeExpression;

    MidiBuffer injectedMidi;
    SpinLock injectedMidiLock;

    mutable SpinLock liveStateLock;
    LiveNoteState liveState;
    void updateLiveState();

    RecordingBuffer recordingBuffer;
    std::atomic<bool> overdubEnabled { false };
    std::atomic<bool> recordArmed { false };
    std::atomic<float> expressionDurationBeats { 1.0f };
    std::atomic<float> expressionIntensity { 1.0f };
    int64_t absoluteSamplePosition = 0;
    double recordStartBeatInSequence = 0.0;
    double recordEndBeatInSequence = 0.0;

    void punchReplaceRange(double rangeStart, double rangeEnd);

    std::atomic<bool> recordingStopPending { false };
    std::atomic<bool> midiPanicRequested { false };
    bool needsNoteCatchUp = false;

    std::atomic<float> masterVolume { 0.8f };

    MidiBuffer outputMidiBuffer;

    
    std::unique_ptr<juce::MidiOutput> virtualMidiOut;
    MidiBuffer pendingVirtualMidi;
    SpinLock pendingVirtualMidiLock;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};
