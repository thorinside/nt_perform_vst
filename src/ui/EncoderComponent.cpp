#include "EncoderComponent.h"
#include <cmath>

EncoderComponent::EncoderComponent(Role role, const juce::String& label)
    : role_(role), label_(label)
{
}

void EncoderComponent::setAngle(float normalised)
{
    angle_ = juce::jlimit(0.0f, 1.0f, normalised);
    repaint();
}

void EncoderComponent::paint(juce::Graphics& g)
{
    const auto bounds   = getLocalBounds().toFloat().reduced(4.0f);
    const float size    = std::min(bounds.getWidth(), bounds.getHeight() - 16.0f);
    const float cx      = bounds.getCentreX();
    const float cy      = bounds.getY() + size * 0.5f;
    const float r       = size * 0.5f;

    const juce::Colour ring    = juce::Colour(0xFF585B70);
    const juce::Colour marker  = juce::Colour(0xFF89B4FA);
    const juce::Colour bg      = juce::Colour(0xFF313244);
    const juce::Colour textCol = juce::Colour(0xFF9399B2);

    // Outer ring
    g.setColour(bg);
    g.fillEllipse(cx - r, cy - r, size, size);
    g.setColour(ring);
    g.drawEllipse(cx - r, cy - r, size, size, 2.0f);

    // Marker dot on the rim — sweeps 210° arc (from -210° to 30° in standard coords)
    // Resting at bottom-left at 0, top at 0.5, bottom-right at 1.0
    const float startAngle = juce::MathConstants<float>::pi * 0.75f;  // ~135°
    const float totalArc   = juce::MathConstants<float>::twoPi * 0.833f; // ~300°
    const float markerAngle = startAngle + angle_ * totalArc;

    const float mx = cx + (r - 4.0f) * std::cos(markerAngle);
    const float my = cy + (r - 4.0f) * std::sin(markerAngle);
    g.setColour(marker);
    g.fillEllipse(mx - 3.0f, my - 3.0f, 6.0f, 6.0f);

    // Label below
    g.setColour(textCol);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f)));
    g.drawText(label_,
               bounds.getX(), cy + r + 2.0f,
               bounds.getWidth(), 14.0f,
               juce::Justification::centred);
}

void EncoderComponent::mouseDown(const juce::MouseEvent& e)
{
    startY_ = e.getPosition().y;
}

void EncoderComponent::mouseDrag(const juce::MouseEvent& e)
{
    const int dy = startY_ - e.getPosition().y; // up = positive
    if (std::abs(dy) >= 8)
    {
        startY_ = e.getPosition().y;
        dispatchDelta(dy > 0 ? 1 : -1);
    }
}

void EncoderComponent::mouseWheelMove(const juce::MouseEvent& /*e*/,
                                      const juce::MouseWheelDetails& wheel)
{
    const int delta = (wheel.deltaY > 0.0f) ? 1 : -1;
    dispatchDelta(delta);
}

void EncoderComponent::dispatchDelta(int delta)
{
    if (role_ == Role::PageNav && onPageDelta)
        onPageDelta(delta);
    else if (role_ == Role::ValueAdjust && onValueDelta)
        onValueDelta(delta);
}
