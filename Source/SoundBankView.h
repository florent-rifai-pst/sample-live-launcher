#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "Text.h"

//==============================================================================
// Réglages panel to manage the library of click sound banks. Each bank has a
// label, a strong-beat sample (temps fort) and a weak-beat sample (temps faible).
// Left: list + add/remove. Right: editor for the selected bank.
class SoundBankView  : public juce::Component,
                       public juce::ListBoxModel
{
public:
    explicit SoundBankView (Library& library);

    void refresh();
    void resized() override;

    std::function<void()> onChange;   // host saves the library + refreshes the song views

    //== ListBoxModel =========================================================
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int lastRow) override;

private:
    void addBank();
    void deleteBank();
    void selectBank (int index);
    void chooseFile (bool accent);
    void updateFileLabels();

    ClickSoundBank* current();

    Library& library;

    juce::Label      title { {}, "Banques de sons (click)"_t };
    juce::ListBox    list;
    juce::TextButton addBtn { "Nouvelle" };
    juce::TextButton delBtn { "Supprimer" };

    juce::Label      nameCaption   { {}, "Libell\xc3\xa9:"_t };   // "Libellé:"
    juce::TextEditor nameEditor;
    juce::Label      accentCaption { {}, "Temps fort:"_t };
    juce::Label      accentFile;
    juce::TextButton accentBtn     { "Choisir..."_t };
    juce::Label      normalCaption { {}, "Temps faible:"_t };
    juce::Label      normalFile;
    juce::TextButton normalBtn     { "Choisir..."_t };

    int selected = -1;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundBankView)
};
