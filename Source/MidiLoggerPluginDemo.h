/*
  ==============================================================================

   This file is part of the JUCE examples.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:                  MIDILogger
 version:               1.0.0
 vendor:                JUCE
 website:               http://juce.com
 description:           Logs incoming MIDI messages.

 dependencies:          juce_audio_basics, juce_audio_devices, juce_audio_formats,
                        juce_audio_plugin_client, juce_audio_processors,
                        juce_audio_utils, juce_core, juce_data_structures,
                        juce_events, juce_graphics, juce_gui_basics, juce_gui_extra
 exporters:             xcode_mac, vs2019, linux_make

 moduleFlags:           JUCE_STRICT_REFCOUNTEDPOINTER=1

 type:                  AudioProcessor
 mainClass:             MidiLoggerPluginDemoProcessor

 useLocalCopy:          1

 pluginCharacteristics: pluginWantsMidiIn, pluginProducesMidiOut

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include <iterator>

#include "CurveEditor.h"

class MidiQueue
{
public:
    void push(const MidiBuffer& buffer)
    {
        for (const auto metadata : buffer)
            fifo.write (1).forEach ([&](int dest) { messages[(size_t)dest] = metadata.getMessage(); });
    }

    template <typename OutputIt>
    void pop(OutputIt out)
    {
        fifo.read (fifo.getNumReady()).forEach ([&](int source) { *out++ = messages[(size_t)source]; });
    }

private:
    static constexpr auto queueSize = 1 << 14;
    AbstractFifo fifo{queueSize};
    std::vector<MidiMessage> messages = std::vector<MidiMessage> (queueSize);
};

struct DropdownListModel
{
    int selectedItemId;
};

//==============================================================================
class MidiLoggerPluginDemoProcessor : public AudioProcessor,
                                      private Timer
{
public:
    MidiLoggerPluginDemoProcessor()
        :
        AudioProcessor (getBusesLayout()),
        curveEditorModel (0.0f, 127.0f, 0.0f, 127.0f)
    {
        state.addChild ({
                            "uiState", {
                                {"width", 500},
                                {"height", 300},
                                {"midiInput", 1},
                                {"midiOutput", 1}
                            },
                            {}
                        }, -1, nullptr);
        startTimerHz (60);
    }

    ~MidiLoggerPluginDemoProcessor() override { stopTimer(); }

    void processBlock(AudioBuffer<float>& audio, MidiBuffer& midi) override { process (audio, midi); }
    void processBlock(AudioBuffer<double>& audio, MidiBuffer& midi) override { process (audio, midi); }

    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    bool isMidiEffect() const override { return true; }
    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override { return new Editor (*this); }

    const String getName() const override { return "MIDI Logger"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }

    void setCurrentProgram(int) override
    {
    }

    const String getProgramName(int) override { return {}; }

    void changeProgramName(int, const String&) override
    {
    }

    void prepareToPlay(double, int) override
    {
    }

    void releaseResources() override
    {
    }

    void getStateInformation(MemoryBlock& destData) override
    {
        if (auto xmlState = state.createXml())
            copyXmlToBinary (*xmlState, destData);
    }

    void setStateInformation(const void* data, int size) override
    {
        if (auto xmlState = getXmlFromBinary (data, size))
            state = ValueTree::fromXml (*xmlState);
    }

private:
    class Editor : public AudioProcessorEditor,
                   private Value::Listener
    {
    public:
        explicit Editor(MidiLoggerPluginDemoProcessor& ownerIn)
            :
            AudioProcessorEditor (ownerIn),
            owner (ownerIn),
            curveEditor (ownerIn.curveEditorModel)
        {
            addAndMakeVisible (curveEditor);
            addAndMakeVisible (midiInputDropdown);
            addAndMakeVisible (midiOutputDropdown);

            setResizable (true, true);
            lastUIWidth.referTo (owner.state.getChildWithName ("uiState").getPropertyAsValue ("width", nullptr));
            lastUIHeight.referTo (owner.state.getChildWithName ("uiState").getPropertyAsValue ("height", nullptr));
            setSize (lastUIWidth.getValue(), lastUIHeight.getValue());

            lastUIWidth.addListener (this);
            lastUIHeight.addListener (this);

            // Setup input/output dropdowns
            midiInputDropdown.onChange = [&]
            {
                lastMidiInput = midiInputDropdown.getSelectedId();
                owner.midiInputModel.selectedItemId = midiInputDropdown.getSelectedId();
            };
            midiOutputDropdown.onChange = [&]
            {
                lastMidiOutput = midiOutputDropdown.getSelectedId();
                owner.midiOutputModel.selectedItemId = midiOutputDropdown.getSelectedId();
            };

            for (auto i = 0; i < 128; i++)
            {
                const auto* const controllerName = MidiMessage::getControllerName (i);
                if (controllerName)
                {
                    midiInputDropdown.addItem (controllerName, i + 1);
                    midiOutputDropdown.addItem (controllerName, i + 1);
                }
            }

            lastMidiInput.referTo(owner.state.getChildWithName("uiState").getPropertyAsValue("midiInput", nullptr));
            lastMidiOutput.referTo(owner.state.getChildWithName("uiState").getPropertyAsValue("midiOutput", nullptr));
            midiInputDropdown.setSelectedId(static_cast<int> (lastMidiInput.getValue()));
            midiOutputDropdown.setSelectedId(static_cast<int> (lastMidiOutput.getValue()));
        }

        void paint(Graphics& g) override
        {
            g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
        }

        void resized() override
        {
            auto bounds = getLocalBounds();

            const auto inputMidiBounds = bounds.removeFromTop (50);
            midiInputDropdown.setBounds (inputMidiBounds.withRight (getWidth() / 2));
            midiOutputDropdown.setBounds (inputMidiBounds.withLeft (getWidth() / 2));
            curveEditor.setBounds (bounds.removeFromBottom (bounds.proportionOfHeight (0.9f)).withTrimmedLeft (10).
                                          withTrimmedRight (10));

            lastUIWidth = getWidth();
            lastUIHeight = getHeight();
        }

    private:
        void valueChanged(Value& value) override
        {
            if (value == lastUIWidth || value == lastUIHeight)
                setSize (lastUIWidth.getValue(), lastUIHeight.getValue());
        }

        MidiLoggerPluginDemoProcessor& owner;

        aas::CurveEditor<float> curveEditor;
        juce::ComboBox midiInputDropdown;
        juce::ComboBox midiOutputDropdown;

        Value lastMidiInput, lastMidiOutput;
        Value lastUIWidth, lastUIHeight;
    };

    void timerCallback() override
    {
        std::vector<MidiMessage> messages;
        queue.pop (std::back_inserter (messages));
    }

    template <typename Element>
    void process(AudioBuffer<Element>& audio, MidiBuffer& midi)
    {
        const int CC_IN = midiInputModel.selectedItemId - 1; // TODO: May have threading issues
        const int CC_OUT = midiOutputModel.selectedItemId - 1;

        // audio.clear(); // TODO: Remove?

        MidiBuffer newMidiBuffer;
        for (auto it : midi)
        {
            MidiMessage msg = it.getMessage();
            const auto timestamp = msg.getTimeStamp();
            const auto sampleNumber = static_cast<int> (timestamp * getSampleRate());
            if (msg.isController() && msg.getControllerNumber() == CC_IN)
            {
                // Map original CC value to new CC value using data from CurveEditor
                const auto value = msg.getControllerValue();
                curveEditorModel.lastInputValue.setValue (value);

                const auto newValue = static_cast<int> (curveEditorModel.compute (static_cast<float> (value)));
                const auto newMsg = juce::MidiMessage::controllerEvent (msg.getChannel(), CC_OUT, newValue);
                newMidiBuffer.addEvent (newMsg, sampleNumber);
            }
            else
            {
                newMidiBuffer.addEvent (msg, sampleNumber);
            }
        }
        midi.swapWith (newMidiBuffer);
        queue.push (midi);
    }

    static BusesProperties getBusesLayout()
    {
        // Live doesn't like to load midi-only plugins, so we add an audio output there.
        return PluginHostType().isAbletonLive()
                   ? BusesProperties().withOutput ("out", AudioChannelSet::stereo())
                   : BusesProperties();
    }

    ValueTree state{"state"};
    MidiQueue queue;
    // The data to show in the UI. We keep it around in the processor so that the view is persistent even when the plugin UI is closed and reopened.
    DropdownListModel midiOutputModel;
    DropdownListModel midiInputModel;
    aas::CurveEditorModel<float> curveEditorModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLoggerPluginDemoProcessor)
};
