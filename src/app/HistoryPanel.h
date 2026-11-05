#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Theme.h"

namespace ld
{
/** The undo history, newest at the bottom. Click any step to go back (or forward) to it. */
class HistoryPanel : public juce::Component, private juce::ChangeListener
{
public:
    HistoryPanel (juce::UndoManager& um, std::function<theme::Palette()> p) : undo (um), palette (std::move (p))
    {
        undo.addChangeListener (this);
    }
    ~HistoryPanel() override { undo.removeChangeListener (this); }

    void paint (juce::Graphics& g) override
    {
        const auto p = palette();
        g.fillAll (p.panel);
        g.setColour (p.line);
        g.fillRect (0, 0, 1, getHeight());

        g.setColour (p.inkQuiet);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText ("HISTORY", 16, 10, getWidth() - 32, 18, juce::Justification::centredLeft);

        auto entries = rows();
        int y = 36;
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            const bool current = i == currentRow();
            const bool future = i > currentRow();
            auto r = juce::Rectangle<int> (8, y, getWidth() - 16, rowHeight);
            if (current)
            {
                g.setColour (p.select.withAlpha (0.18f));
                g.fillRoundedRectangle (r.toFloat(), theme::radius::s);
            }
            g.setColour (future ? p.inkQuiet.withAlpha (0.6f) : current ? p.ink : p.inkQuiet);
            g.setFont (juce::FontOptions (13.0f, current ? juce::Font::bold : juce::Font::plain));
            g.drawText (entries[(size_t) i], r.reduced (8, 0), juce::Justification::centredLeft, true);
            y += rowHeight;
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int row = (e.y - 36) / rowHeight;
        if (row < 0 || row >= (int) rows().size()) return;
        int delta = row - currentRow();
        while (delta < 0 && undo.undo()) ++delta;
        while (delta > 0 && undo.redo()) --delta;
    }

private:
    juce::UndoManager& undo;
    std::function<theme::Palette()> palette;
    static constexpr int rowHeight = 28;

    // Row 0 is the opened session; then each undoable step, oldest first; then steps you can redo.
    std::vector<juce::String> rows() const
    {
        std::vector<juce::String> out { "Opened session" };
        auto past = undo.getUndoDescriptions();
        for (int i = past.size(); --i >= 0;) out.push_back (past[i].isNotEmpty() ? past[i] : "Edit");
        for (auto& f : undo.getRedoDescriptions()) out.push_back (f.isNotEmpty() ? f : "Edit");
        return out;
    }
    int currentRow() const { return undo.getUndoDescriptions().size(); }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
};
} // namespace ld
