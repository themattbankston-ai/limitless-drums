#pragma once

#include <visage/app.h>
#include <visage/widgets.h>
#include <functional>
#include <map>
#include <string>

namespace ld::ui
{
/** What the New Session window holds. */
struct NewSessionState
{
    std::string name;
    std::string library;
    unsigned libraryColour = 0xff9b6bff;
    std::string startFrom = "The library's standard";   ///< what the Start From box shows
    std::string drum = "snare";       ///< kick, snare, tom, hat, ride, crash, china, splash, kit
    std::string artic = "Center";
    int vel = 6, rr = 6;
    bool countsFromNames = false;     ///< the counts came from the file names (not typed)
    std::string folder;               ///< the take's folder name (empty: nothing added yet)
    std::string folderDetail;         ///< "8 mics · 48 kHz"
};

/** The New Session window: name, library, what to start from, the drum, the articulation, the counts, and the
    audio (drop a folder or browse). Drawn over the studio on the same black plate as the header. */
class NewSession : public visage::Frame
{
    /** A text field that tells the window when it gains or loses focus (for the glow). */
    struct Field : visage::TextEditor
    {
        std::function<void()> onFocus;
        void focusChanged (bool f, bool clicked) override { visage::TextEditor::focusChanged (f, clicked); if (onFocus) onFocus(); }
    };

public:
    NewSession();
    ~NewSession() override;

    void open (NewSessionState);
    void close();
    bool isOpen() const { return isVisible(); }
    NewSessionState& state() { return st; }
    /** After changing state() from outside: puts it back in the fields and redraws. */
    void refresh();

    // the box that was clicked, in window pixels, for the menu that opens under it
    std::function<void (float x, float y, float w, float h)> onPickLibrary, onPickStart;
    std::function<void()> onNewLibrary, onBrowse, onCancel, onCreate;
    std::function<void (const std::vector<std::string>&)> onFilesDropped;

    void draw (visage::Canvas&) override;
    void resized() override;
    void mouseDown (const visage::MouseEvent&) override;
    void mouseMove (const visage::MouseEvent&) override;
    void mouseExit (const visage::MouseEvent&) override;
    bool keyPress (const visage::KeyEvent&) override;
    bool receivesDragDropFiles() override { return true; }
    std::string dragDropFileExtensionRegex() override { return ".*"; }
    void dragFilesEnter (const std::vector<std::string>&) override { dropHover = true; redraw(); }
    void dragFilesExit() override { dropHover = false; redraw(); }
    void dropFiles (const std::vector<std::string>&) override;

    static constexpr float DW = 900, DH = 800;   ///< the panel, in design units

private:
    NewSessionState st;
    Field nameEd, articEd;
    visage::Palette pal;
    float k = 1, ox = 0, oy = 0;      ///< scale and the panel's top-left, in window pixels
    std::string hover;
    bool dropHover = false;
    std::map<std::string, visage::Svg> svgs;
    struct Hit { std::string id; float x, y, w, h; };
    std::vector<Hit> hits;            ///< design units, rebuilt each draw

    float X (float v) const { return v * k; }
    float PX (float v) const { return ox + v * k; }
    float PY (float v) const { return oy + v * k; }
    std::string hitAt (float x, float y) const;
    void icon (visage::Canvas&, const std::string& key, const std::string& svg, float x, float y, float w, float h);
    std::vector<std::string> chipsFor() const;
    bool canCreate() const;
};
} // namespace ld::ui
