#include "PageSelectorComponent.h"

PageSelectorComponent::PageSelectorComponent()
{
    for (int i = 0; i < kNumPages; ++i)
    {
        auto& btn = buttons_[i];
        btn.setButtonText(juce::String(i + 1));
        btn.setClickingTogglesState(false);
        btn.setColour(juce::TextButton::buttonColourId,    juce::Colour(0xFF313244));
        btn.setColour(juce::TextButton::buttonOnColourId,  juce::Colour(0xFF89B4FA));
        btn.setColour(juce::TextButton::textColourOffId,   juce::Colour(0xFF9399B2));
        btn.setColour(juce::TextButton::textColourOnId,    juce::Colour(0xFF1E1E2E));

        const int page = i;
        btn.onClick = [this, page]()
        {
            setCurrentPage(page);
            if (onPageSelected)
                onPageSelected(page);
        };
        addAndMakeVisible(btn);
    }
}

void PageSelectorComponent::setCurrentPage(int page)
{
    currentPage_ = page;
    for (int i = 0; i < kNumPages; ++i)
    {
        const bool sel = (i == page);
        buttons_[i].setColour(juce::TextButton::buttonColourId,
                              sel ? juce::Colour(0xFF89B4FA)
                                  : juce::Colour(0xFF313244));
        buttons_[i].setColour(juce::TextButton::textColourOffId,
                              sel ? juce::Colour(0xFF1E1E2E)
                                  : juce::Colour(0xFF9399B2));
        buttons_[i].repaint();
    }
}

void PageSelectorComponent::resized()
{
    const int w = getWidth() / kNumPages;
    for (int i = 0; i < kNumPages; ++i)
        buttons_[i].setBounds(i * w, 0, w, getHeight());
}

void PageSelectorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E2E));
}
