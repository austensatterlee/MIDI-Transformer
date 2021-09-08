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

 name:                  MIDI-Transformer
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
 mainClass:             MidiTransformerPluginProcessor

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
class MidiTransformerPluginProcessor : public AudioProcessor,
                                       private Timer
{
public:
    MidiTransformerPluginProcessor()
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
                            }
                        }, -1, nullptr);
        startTimerHz (60);
    }

    ~MidiTransformerPluginProcessor() override { stopTimer(); }

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
        ValueTree tmp = state.createCopy();

        // Reset the nodes within the state ("curveState") and then re-add them
        tmp.getChildWithName("curveState").removeAllChildren(nullptr);
        for (size_t i = 0; i < curveEditorModel.nodes.size(); i++) {
            const auto& node = curveEditorModel.nodes[i];
            juce::Identifier id{"pt" + std::to_string (i)};
            tmp.getChildWithName("curveState").addChild (node->toValueTree (id), -1, nullptr);
        }

        if (auto xmlState = tmp.createXml())
            copyXmlToBinary (*xmlState, destData);
    }

    void setStateInformation(const void* data, int size) override
    {
        if (auto xmlState = getXmlFromBinary(data, size)) {
            state = ValueTree::fromXml(*xmlState);
            curveEditorModel.fromValueTree(state.getChildWithName("curveState"));
        }
    }

private:
    class Editor : public AudioProcessorEditor,
                   private Value::Listener
    {
    public:
        static const int VELOCITY_DROPDOWN_ID = -1;
        static const int PITCH_DROPDOWN_ID = -2;

        explicit Editor(MidiTransformerPluginProcessor& ownerIn)
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

            // Fill input/output midi dropdowns
            midiInputDropdown.addItem ("Velocity", VELOCITY_DROPDOWN_ID);
            midiInputDropdown.addItem ("Pitch", PITCH_DROPDOWN_ID);
            midiOutputDropdown.addItem ("Pitch", PITCH_DROPDOWN_ID);
            for (auto i = 0; i < 128; i++)
            {
                const auto* const rawControllerName = MidiMessage::getControllerName (i);
                std::string controllerName;
                if (rawControllerName)
                {
                    controllerName = std::string (rawControllerName) + "(CC " + std::to_string (i) + ")";
                }
                else
                {
                    controllerName = "CC " + std::to_string (i);
                }
                midiInputDropdown.addItem (controllerName, i + 1);
                midiOutputDropdown.addItem (controllerName, i + 1);
            }

            lastMidiInput.referTo (owner.state.getChildWithName ("uiState").getPropertyAsValue ("midiInput", nullptr));
            lastMidiOutput.referTo (owner.state.getChildWithName ("uiState").getPropertyAsValue ("midiOutput", nullptr));
            midiInputDropdown.setSelectedId (static_cast<int> (lastMidiInput.getValue()));
            midiOutputDropdown.setSelectedId (static_cast<int> (lastMidiOutput.getValue()));
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

        MidiTransformerPluginProcessor& owner;

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
        using NumericType = decltype(curveEditorModel)::NumericType;
        const int CC_IN = midiInputModel.selectedItemId - 1; // TODO: May have threading issues
        const int CC_OUT = midiOutputModel.selectedItemId - 1;

        MidiBuffer newMidiBuffer;
        for (auto it : midi)
        {
            MidiMessage msg = it.getMessage();
            const auto timestamp = msg.getTimeStamp();
            const auto sampleNumber = static_cast<int> (timestamp * getSampleRate());
            NumericType inputValue = 0;
            NumericType outputValue = 0;
            if (msg.isController() && msg.getControllerNumber() == CC_IN)
            {
                inputValue = static_cast<NumericType> (msg.getControllerValue());
            }
            else if (CC_IN == Editor::VELOCITY_DROPDOWN_ID - 1 && msg.isNoteOn (false))
            {
                inputValue = static_cast<NumericType> (msg.getVelocity());
                newMidiBuffer.addEvent (msg, sampleNumber);
            }
            else if (CC_IN == Editor::PITCH_DROPDOWN_ID - 1 && msg.isPitchWheel())
            {
                inputValue = (static_cast<NumericType> (msg.getPitchWheelValue()) / static_cast<NumericType> (1 << 14)) *
                        (curveEditorModel.maxY - curveEditorModel.minY);
            }
            else
            {
                newMidiBuffer.addEvent (msg, sampleNumber);
                continue;
            }

            // Map original MIDI value to a new MIDI value using the function defined by the CurveEditor
            curveEditorModel.lastInputValue.setValue (inputValue);
            outputValue = curveEditorModel.compute (static_cast<float> (inputValue));

            MidiMessage newMsg;
            if (CC_OUT >= 0)
            {
                newMsg = juce::MidiMessage::controllerEvent (msg.getChannel(), CC_OUT, static_cast<int> (outputValue));
            }
            else if (CC_OUT == Editor::PITCH_DROPDOWN_ID - 1)
            {
                outputValue = (outputValue * (1 << 14)) / (curveEditorModel.maxY - curveEditorModel.minY);
                newMsg = juce::MidiMessage::pitchWheel (msg.getChannel(), static_cast<int> (outputValue));
            }

            newMidiBuffer.addEvent (newMsg, sampleNumber);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiTransformerPluginProcessor)
};
