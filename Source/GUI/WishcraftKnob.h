#pragma once

#include <cmath>
#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "WishcraftColours.h"

// Rotary knob matching the JSFX's draw_knob() + knob_interact(): a filled circle with a
// needle line at (-135 + norm*270) degrees, label above (centered over the control, per
// this session's explicit deviation from the JSFX's left-aligned text), value below.
//
// Interaction, matching knob_interact() exactly:
//  - Click-drag: VERTICAL drag only (up increases, down decreases) -- relative, not
//    click-to-position, since a knob has no single "this pixel = this value" mapping.
//    Sensitivity: 1/250 of the full range per pixel dragged (a ~250px drag spans the
//    whole range).
//  - Shift+drag: same drag, but 1/2000 sensitivity (8x finer).
//  - Ctrl/Cmd+click: opens a small text-entry box instead of starting a drag. Enter
//    commits (clamped to range), Escape cancels, clicking away cancels.
//  - Double-click: resets to the parameter's default value.
class WishcraftKnob : public juce::Component
{
public:
    WishcraftKnob (juce::RangedAudioParameter& parameterIn, juce::String labelText,
                   std::function<juce::String (float)> formatterIn)
        : param (parameterIn),
          label (std::move (labelText)),
          formatter (std::move (formatterIn)),
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
        valueBounds = b.removeFromBottom (20);
        const int diameter = juce::jmin (b.getWidth(), b.getHeight());
        circleBounds = juce::Rectangle<int> (diameter, diameter).withCentre (b.getCentre());
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (WishcraftColours::controlLabel);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (label, labelBounds, juce::Justification::centred, false);

        auto circleF = circleBounds.toFloat();
        const float r = circleF.getWidth() * 0.5f;
        const auto centre = circleF.getCentre();

        g.setColour (WishcraftColours::knobTrack);
        g.fillEllipse (circleF);
        g.setColour (WishcraftColours::knobBorder);
        g.drawEllipse (circleF, 1.5f);

        const float norm = juce::jlimit (0.0f, 1.0f, param.convertTo0to1 (currentValue));
        const float angle = juce::degreesToRadians (-135.0f + norm * 270.0f);
        const float needleLen = juce::jmax (0.0f, r - 7.0f);
        const juce::Point<float> tip (centre.x + std::sin (angle) * needleLen,
                                       centre.y - std::cos (angle) * needleLen);
        g.setColour (WishcraftColours::knobNeedle);
        g.drawLine ({ centre, tip }, 2.0f);

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
        dragStartNorm = param.convertTo0to1 (currentValue);
        dragStartY = e.getPosition().y;
        attachment.beginGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging)
            return;

        const float sens = e.mods.isShiftDown() ? (1.0f / 2000.0f) : (1.0f / 250.0f);
        const float deltaY = (float) (dragStartY - e.getPosition().y);
        const float newNorm = juce::jlimit (0.0f, 1.0f, dragStartNorm + deltaY * sens);
        const float newReal = param.convertFrom0to1 (newNorm);
        currentValue = newReal;
        attachment.setValueAsPartOfGesture (newReal);
        repaint();
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
    float currentValue = 0.0f;

    juce::Rectangle<int> labelBounds, valueBounds, circleBounds;

    bool dragging = false;
    float dragStartNorm = 0.0f;
    int dragStartY = 0;

    std::unique_ptr<juce::TextEditor> textEditor;

    juce::ParameterAttachment attachment;
};
