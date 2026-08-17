#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "WishcraftColours.h"

// Horizontal slider matching the JSFX's draw_hslider() + hslider_interact(): a track
// with a center tick and a thumb, label above (centered over the control, per this
// session's explicit deviation from the JSFX's left-aligned text), low/high end labels
// flanking the track, value below.
//
// Interaction, matching hslider_interact() exactly:
//  - Click-drag: click ANYWHERE on the track jumps straight to that position
//    (conventional slider behavior, unlike the knob's relative-drag-only). Continued
//    drag tracks the mouse position directly.
//  - Shift+drag: instead of snapping straight to the mapped position, eases toward it
//    (val += (mapped - val) * 0.08 per event) for fine control.
//  - Ctrl/Cmd+click: opens a small text-entry box instead of jumping/dragging. Enter
//    commits (clamped to range), Escape cancels, clicking away cancels.
//  - Double-click: resets to the parameter's default value.
class WishcraftHSlider : public juce::Component
{
public:
    WishcraftHSlider (juce::RangedAudioParameter& parameterIn, juce::String labelText,
                       std::function<juce::String (float)> formatterIn,
                       juce::String lowLabelText, juce::String highLabelText)
        : param (parameterIn),
          label (std::move (labelText)),
          formatter (std::move (formatterIn)),
          lowLabel (std::move (lowLabelText)),
          highLabel (std::move (highLabelText)),
          attachment (parameterIn, [this] (float newValue) { currentValue = newValue; repaint(); })
    {
        currentValue = param.convertFrom0to1 (param.getValue());
        attachment.sendInitialUpdate();
        setWantsKeyboardFocus (false);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        labelBounds = b.removeFromTop (20); // extra room for descenders (g/y/p) below the baseline
        valueBounds = b.removeFromBottom (18);
        endLabelBounds = b.removeFromBottom (14);
        b.removeFromTop (juce::jmax (0, (b.getHeight() - 18) / 2));
        trackBounds = b.removeFromTop (18).reduced (6, 0);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (WishcraftColours::controlLabel);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (label, labelBounds, juce::Justification::centred, false);

        auto trackF = trackBounds.toFloat().withSizeKeepingCentre ((float) trackBounds.getWidth(), 6.0f);
        g.setColour (WishcraftColours::sliderTrack);
        g.fillRect (trackF);
        g.setColour (WishcraftColours::sliderBorder);
        g.drawRect (trackF, 1.0f);
        g.setColour (WishcraftColours::sliderCenterTick);
        g.drawVerticalLine ((int) trackF.getCentreX(), (float) trackBounds.getY() - 2.0f, (float) trackBounds.getBottom() + 2.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, param.convertTo0to1 (currentValue));
        const float thumbX = (float) trackBounds.getX() + norm * (float) trackBounds.getWidth();
        g.setColour (WishcraftColours::sliderThumb);
        g.fillRect (juce::Rectangle<float> (8.0f, 18.0f).withCentre ({ thumbX, trackF.getCentreY() }));

        g.setColour (WishcraftColours::controlEndLabel);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (lowLabel, endLabelBounds, juce::Justification::centredLeft, false);
        g.drawText (highLabel, endLabelBounds, juce::Justification::centredRight, false);

        g.setColour (WishcraftColours::controlValueText);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (formatter (currentValue), valueBounds, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        {
            openTextEditor();
            return;
        }

        dragging = true;
        attachment.beginGesture();
        applyMappedPosition (e, false);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging)
            applyMappedPosition (e, e.mods.isShiftDown());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging)
        {
            dragging = false;
            attachment.endGesture();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        const float def = param.convertFrom0to1 (param.getDefaultValue());
        currentValue = def;
        attachment.setValueAsCompleteGesture (def);
        repaint();
    }

private:
    void applyMappedPosition (const juce::MouseEvent& e, bool damped)
    {
        const float frac = juce::jlimit (0.0f, 1.0f,
            trackBounds.getWidth() > 0
                ? (e.position.x - (float) trackBounds.getX()) / (float) trackBounds.getWidth()
                : 0.0f);
        const float mapped = param.convertFrom0to1 (frac);
        const float newReal = damped ? currentValue + (mapped - currentValue) * 0.08f : mapped;
        currentValue = newReal;
        attachment.setValueAsPartOfGesture (newReal);
        repaint();
    }

    void openTextEditor()
    {
        textEditor = std::make_unique<juce::TextEditor>();
        textEditor->setInputRestrictions (10, "-0123456789.");
        textEditor->setJustification (juce::Justification::centred);
        textEditor->setText (juce::String (currentValue, 2), juce::dontSendNotification);
        textEditor->setSelectAllWhenFocused (true);
        textEditor->onReturnKey = [this] { commitTextEditor(); };
        textEditor->onEscapeKey = [this] { closeTextEditor(); };
        textEditor->onFocusLost = [this] { closeTextEditor(); };
        addAndMakeVisible (*textEditor);
        textEditor->setBounds (getLocalBounds().withSizeKeepingCentre (90, 26));
        textEditor->grabKeyboardFocus();
    }

    void commitTextEditor()
    {
        if (textEditor == nullptr)
            return;

        const auto& range = param.getNormalisableRange();
        const float typed = textEditor->getText().getFloatValue();
        const float clamped = juce::jlimit (range.start, range.end, typed);
        currentValue = clamped;
        attachment.setValueAsCompleteGesture (clamped);
        closeTextEditor();
    }

    void closeTextEditor()
    {
        textEditor.reset();
        repaint();
    }

    juce::RangedAudioParameter& param;
    juce::String label;
    std::function<juce::String (float)> formatter;
    juce::String lowLabel, highLabel;
    float currentValue = 0.0f;

    juce::Rectangle<int> labelBounds, valueBounds, endLabelBounds, trackBounds;

    bool dragging = false;

    std::unique_ptr<juce::TextEditor> textEditor;

    juce::ParameterAttachment attachment;
};
