#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
WishcraftMasteringLimiterAudioProcessorEditor::WishcraftMasteringLimiterAudioProcessorEditor (WishcraftMasteringLimiterAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    setSize (400, 300);
}

WishcraftMasteringLimiterAudioProcessorEditor::~WishcraftMasteringLimiterAudioProcessorEditor()
{
}

//==============================================================================
void WishcraftMasteringLimiterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Wishcraft Mastering Limiter\n(Stage 1 scaffold: bypass)",
                       getLocalBounds(), juce::Justification::centred, 2);
}

void WishcraftMasteringLimiterAudioProcessorEditor::resized()
{
}
