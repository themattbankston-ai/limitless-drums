#include "Project.h"

namespace ld::project
{
namespace te = tracktion;

static const juce::Identifier ldKind ("ldKind"), ldRank ("ldRank"), ldIndex ("ldIndex"), ldSide ("ldSide"),
                              ldDetail ("ldDetail"), ldConfident ("ldConfident"), ldName ("ldRoleName"),
                              ldProjectFile ("ldProjectFile");

void storeRoles (te::Edit& edit, const std::vector<MicRole>& roles)
{
    auto tracks = te::getAudioTracks (edit);
    for (int i = 0; i < tracks.size() && i < (int) roles.size(); ++i)
    {
        auto& r = roles[(size_t) i];
        auto s = tracks[i]->state;
        s.setProperty (ldKind, (int) r.kind, nullptr);
        s.setProperty (ldRank, r.rank, nullptr);
        s.setProperty (ldIndex, r.index, nullptr);
        s.setProperty (ldSide, r.side, nullptr);
        s.setProperty (ldDetail, r.detail, nullptr);
        s.setProperty (ldConfident, r.confident, nullptr);
        s.setProperty (ldName, r.displayName, nullptr);
    }
}

std::vector<MicRole> readRoles (te::Edit& edit)
{
    std::vector<MicRole> roles;
    for (auto* t : te::getAudioTracks (edit))
    {
        auto s = t->state;
        MicRole r;
        if (s.hasProperty (ldKind))
        {
            r.kind = (MicRole::Kind) (int) s[ldKind];
            r.rank = s[ldRank];
            r.index = s[ldIndex];
            r.side = s[ldSide].toString();
            r.detail = s[ldDetail].toString();
            r.confident = s[ldConfident];
            r.displayName = s[ldName].toString();
        }
        else
        {
            r = classifyMic (t->getName());
        }
        roles.push_back (r);
    }
    return roles;
}

bool save (te::Edit& edit, const juce::File& file)
{
    // Write to a temp file first, then swap it in, so a crash mid-save never destroys the old project.
    juce::TemporaryFile temp (file);
    if (! te::EditFileOperations (edit).writeToFile (temp.getFile(), false))
        return false;
    if (! temp.overwriteTargetFileWithTemporary())
        return false;

    edit.editFileRetriever = [file] { return file; };
    edit.resetChangedStatus();
    return true;
}

std::unique_ptr<te::Edit> open (te::Engine& engine, const juce::File& file,
                                std::vector<MicRole>& roles, juce::StringArray& missingFiles)
{
    if (! file.existsAsFile())
        return {};

    auto edit = te::loadEditFromFile (engine, file, te::Edit::EditRole::forEditing);
    if (edit == nullptr)
        return {};

    edit->editFileRetriever = [file] { return file; };
    roles = readRoles (*edit);

    for (auto* t : te::getAudioTracks (*edit))
        for (auto* c : t->getClips())
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (c))
            {
                auto f = ac->getAudioFile().getFile();
                if (! f.existsAsFile())
                    missingFiles.addIfNotAlreadyThere (f.getFullPathName());
            }

    edit->resetChangedStatus();
    return edit;
}

juce::File autosaveFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
                 .getChildFile ("Application Support")
#endif
                 .getChildFile ("Limitless Drums").getChildFile ("Autosave");
    d.createDirectory();
    return d;
}

juce::File autosaveFileFor (const juce::File& projectFile, const juce::String& sessionKey)
{
    const auto base = projectFile != juce::File() ? projectFile.getFileNameWithoutExtension() + "-"
                                                        + juce::String::toHexString (projectFile.getFullPathName().hashCode64())
                                                  : "Untitled-" + sessionKey;
    return autosaveFolder().getChildFile (base + extension);
}

bool autosave (te::Edit& edit, const juce::File& autosaveFile)
{
    if (! edit.hasChangedSinceSaved() && autosaveFile.existsAsFile())
        return false;

    juce::TemporaryFile temp (autosaveFile);
    if (! te::EditFileOperations (edit).writeToFile (temp.getFile(), false))
        return false;

    // Remember which project this autosave belongs to, for recovery.
    if (auto xml = juce::parseXML (temp.getFile()))
    {
        xml->setAttribute (ldProjectFile, edit.editFileRetriever ? edit.editFileRetriever().getFullPathName() : juce::String());
        xml->writeTo (temp.getFile());
    }
    return temp.overwriteTargetFileWithTemporary();
}

juce::Array<juce::File> findRecoverableAutosaves()
{
    juce::Array<juce::File> out;
    for (const auto& entry : juce::RangedDirectoryIterator (autosaveFolder(), false, juce::String ("*") + extension))
    {
        auto f = entry.getFile();
        juce::File projectFile;
        if (auto xml = juce::parseXML (f))
            projectFile = juce::File (xml->getStringAttribute (ldProjectFile));

        if (projectFile == juce::File() || ! projectFile.existsAsFile()
            || projectFile.getLastModificationTime() < f.getLastModificationTime())
            out.add (f);
    }
    return out;
}

} // namespace ld::project
