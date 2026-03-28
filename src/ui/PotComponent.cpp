#include "PotComponent.h"
#include <algorithm>

PotComponent::PotComponent()
{
    setRepaintsOnMouseActivity(false);
}

void PotComponent::setData(const Data& d)
{
    data_ = d;
    if (!isDragging_)
    {
        // Don't overwrite the live drag position — suppresses CC feedback jitter
        localFillFraction_ = d.fillFraction >= 0.0f ? d.fillFraction : 0.0f;
        repaint();
    }
}

//==============================================================================
// Painting — mirrors _KnobSlot in hardware_preview_widget.dart
//==============================================================================

void PotComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const float radius = 8.0f;

    // Background
    const juce::Colour panelBg  = juce::Colour(0xFF1E1E2E);
    const juce::Colour highlight = juce::Colour(0xFF89B4FA); // accent blue
    const juce::Colour outline   = data_.isEmpty
                                      ? juce::Colour(0x40FFFFFF)
                                      : juce::Colour(0x80FFFFFF);
    const juce::Colour textPrimary   = juce::Colour(0xFFCDD6F4);
    const juce::Colour textSecondary = juce::Colour(0xFF9399B2);

    // Border
    g.setColour(panelBg);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(outline);
    g.drawRoundedRectangle(bounds, radius, 1.0f);

    if (!data_.isEmpty && data_.enabled)
    {
        // Gradient fill bar
        const float fillWidth = bounds.getWidth() * localFillFraction_;
        if (fillWidth > 0.0f)
        {
            juce::Path fillPath;
            fillPath.addRoundedRectangle(bounds.getX(), bounds.getY(),
                                         fillWidth, bounds.getHeight(),
                                         radius, radius,
                                         true, fillWidth >= bounds.getWidth(),
                                         true, fillWidth >= bounds.getWidth());
            g.setColour(highlight.withAlpha(0.25f));
            g.fillPath(fillPath);
        }
    }

    // Labels (inside the border) — 8px padding on all sides
    const auto inner = bounds.reduced(8.0f, 8.0f);
    const float h    = inner.getHeight();

    // Position label ("Pot L" / "Pot C" / "Pot R") — top, small, secondary
    g.setColour(textSecondary);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
    g.drawText(data_.posLabel, inner.withHeight(12.0f), juce::Justification::centredLeft);

    if (data_.isEmpty || !data_.enabled)
    {
        // Empty slot
        g.setColour(textSecondary);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
        g.drawText("---", inner, juce::Justification::centred);
        return;
    }

    // Upper label (parameter name) — centred, bold
    const float midY = inner.getY() + h * 0.45f;
    g.setColour(textPrimary);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f).withStyle("Bold")));
    g.drawText(data_.upperLabel,
               inner.getX(), midY - 8.0f, inner.getWidth(), 16.0f,
               juce::Justification::centred, true);

    // Lower label (algorithm name) — below, smaller, secondary
    g.setColour(textSecondary);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.0f)));
    g.drawText(data_.lowerLabel,
               inner.getX(), midY + 10.0f, inner.getWidth(), 12.0f,
               juce::Justification::centred, true);
}

//==============================================================================
// Mouse interaction
//==============================================================================

void PotComponent::mouseDown(const juce::MouseEvent& e)
{
    if (data_.isEmpty || !data_.enabled)
        return;
    isDragging_        = true;
    dragStartX_        = static_cast<float>(e.getPosition().x);
    dragStartFill_     = localFillFraction_;
    lastThrottleMs_    = juce::Time::currentTimeMillis();
    if (onDragStart) onDragStart();
}

void PotComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging_)
        return;

    const float w = static_cast<float>(getWidth());
    if (w <= 0.0f)
        return;

    const float delta   = (static_cast<float>(e.getPosition().x) - dragStartX_) / w;
    localFillFraction_  = std::clamp(dragStartFill_ + delta, 0.0f, 1.0f);
    repaint();

    // Throttle notifications
    const int64_t now = juce::Time::currentTimeMillis();
    if (now - lastThrottleMs_ >= kThrottleMs)
    {
        lastThrottleMs_ = now;
        notifyChange(true);
    }
}

void PotComponent::mouseUp(const juce::MouseEvent& e)
{
    if (!isDragging_)
        return;
    isDragging_ = false;

    // Single click (< 4px movement) on a boolean param (range == 1) toggles the value
    const float totalMove = std::abs(static_cast<float>(e.getPosition().x) - dragStartX_);
    if (totalMove < 4.0f && (data_.rangeMax - data_.rangeMin) == 1)
    {
        localFillFraction_ = (localFillFraction_ < 0.5f) ? 1.0f : 0.0f;
        repaint();
    }

    notifyChange(false);
    if (onDragEnd) onDragEnd();
}

//==============================================================================
// Helpers
//==============================================================================

int PotComponent::fillFractionToValue(float f) const
{
    if (data_.rangeMax <= data_.rangeMin)
        return data_.rangeMin;
    const float result = data_.rangeMin
                       + f * static_cast<float>(data_.rangeMax - data_.rangeMin);
    return static_cast<int>(result + 0.5f);
}

void PotComponent::notifyChange(bool isDragging)
{
    if (onValueChange)
        onValueChange(data_.slotIndex, data_.paramNumber,
                      fillFractionToValue(localFillFraction_), isDragging);
}
