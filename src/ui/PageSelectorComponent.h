#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <array>

class PageSelectorComponent : public juce::Component
{
public:
    static constexpr int kNumPages = 10;

    PageSelectorComponent();

    void setCurrentPage(int page); // 0-indexed
    int  getCurrentPage() const { return currentPage_; }

    std::function<void(int page)> onPageSelected;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    int currentPage_ = 0;
    std::array<juce::TextButton, kNumPages> buttons_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PageSelectorComponent)
};
