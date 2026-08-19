#pragma once

// Trial-build-only licensing check. Compiled in ONLY when WISHCRAFT_TRIAL_BUILD is 1
// (see CMakeLists.txt's WISHCRAFT_TRIAL_BUILD option) -- the normal Release build
// shipped via the real installer defines this to 0, so none of this code exists in
// that binary at all.
//
// Reads a tamper-evident marker file the installer writes at install time, ONLY if one
// doesn't already exist (see Packaging/macOS/build_installer.sh's TRIAL=1 mode and
// Packaging/Windows/installer.iss's /DTRIAL=1 mode). The marker file holds the install
// date plus a salted hash of it; PluginProcessor blocks audio once WISHCRAFT_TRIAL_DAYS
// have elapsed since that date, or if the file is missing/tampered/backdated.
//
// This is NOT hardened DRM -- it's proportionate friction for known beta testers, not a
// defense against a determined attacker. A moderately technical user could still defeat
// it (back up the marker file before it ages, or reinstall after manually deleting it --
// which the installer deliberately doesn't clean up on uninstall, so at least deleting
// it requires noticing a hidden, root/admin-owned file exists in the first place). The
// goal is to make "just make it work again" enough of a hassle that emailing
// wishcraftmusicstudio@gmail.com for a free full version is the easier path.

#if WISHCRAFT_TRIAL_BUILD

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

namespace TrialLicense
{
    // MUST exactly match the secret string embedded in Packaging/macOS/build_installer.sh's
    // postinstall script and Packaging/Windows/installer.iss's marker-writing PowerShell
    // script -- all three compute the same salted hash over the same install-date string,
    // and disagreeing would make every trial install read as tampered.
    static constexpr const char* secret = "Wishcraft-Trial-K7q2Zx9p";

   #if JUCE_MAC
    static inline juce::File markerFile()
    {
        return juce::File ("/Library/Application Support/Wishcraft Mastering Limiter/.trial");
    }
   #elif JUCE_WINDOWS
    static inline juce::File markerFile()
    {
        const auto programData = juce::SystemStats::getEnvironmentVariable ("ProgramData", "C:\\ProgramData");
        return juce::File (programData + "\\Wishcraft Mastering Limiter\\.trial");
    }
   #else
    static inline juce::File markerFile() { return {}; }
   #endif

    static inline juce::String hashFor (const juce::String& dateStr)
    {
        const juce::String combined = juce::String (secret) + "|" + dateStr;
        juce::SHA256 hash (combined.toRawUTF8(), combined.getNumBytesAsUTF8());
        return hash.toHexString();
    }

    struct Status
    {
        bool blocked = true;
        int daysRemaining = 0;
    };

    // Reads and verifies the marker file once. Cheap enough for the message thread
    // (called from PluginProcessor's constructor) but deliberately NOT called from
    // processBlock -- the result is cached into an atomic the audio thread just reads.
    static inline Status checkStatus()
    {
        auto file = markerFile();
        if (! file.existsAsFile())
            return {}; // blocked=true -- no file means either this copy wasn't
                       // installed via our installer, or the user deleted it hoping to
                       // reset the trial.

        auto lines = juce::StringArray::fromLines (file.loadFileAsString());
        if (lines.size() < 2)
            return {};

        const auto dateStr = lines[0].trim();
        const auto storedHash = lines[1].trim();
        if (storedHash.isEmpty() || ! storedHash.equalsIgnoreCase (hashFor (dateStr)))
            return {}; // date/hash disagree -- edited by hand -- treat exactly like missing.

        // The hash check above guarantees dateStr is byte-for-byte what the installer
        // wrote (any edit fails the comparison above), so it's always well-formed here.
        const auto installDate = juce::Time::fromISO8601 (dateStr + "T00:00:00");
        const auto now = juce::Time::getCurrentTime();
        const double daysElapsed = (now - installDate).inDays();

        // Negative elapsed time means the system clock reads earlier than the install
        // date -- e.g. someone rolled it back hoping to "extend" the trial. Block
        // rather than silently granting more time.
        if (daysElapsed < 0.0)
            return {};

        constexpr int trialDays = WISHCRAFT_TRIAL_DAYS;
        const int remaining = trialDays - (int) daysElapsed;
        if (remaining <= 0)
            return {};

        return { false, remaining };
    }
}

#endif // WISHCRAFT_TRIAL_BUILD
