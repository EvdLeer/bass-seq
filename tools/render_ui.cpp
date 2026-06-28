// Headless UI render: builds the editor and writes it to a PNG so the look can be
// checked without a screen. No window/X needed — pure software rasterising.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/SettingsOverlay.h"
#include "ui/PadsCabinet.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    BassSeqProcessor processor;

    // Optional: render an overlay on its own (argv[2] == "settings" / "help").
    const juce::String mode = (argc > 2) ? juce::String (argv[2]) : juce::String();

    int w = 900, h = 560;
    std::unique_ptr<juce::Component> comp;
    if (mode == "settings")
    {
        w = 640; h = 460;
        auto ov = std::make_unique<SettingsOverlay> (processor);
        ov->setBounds (0, 0, w, h);
        ov->setVisible (true);
        comp = std::move (ov);
    }
    else if (mode == "help")
    {
        w = PadsCabinet::VIEW_W; h = PadsCabinet::VIEW_H;
        auto cab = std::make_unique<PadsCabinet> (processor);
        cab->setBounds (0, 0, w, h);
        cab->keyPressed (juce::KeyPress (0, juce::ModifierKeys(), (juce::juce_wchar) '?'));
        comp = std::move (cab);
    }
    else
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        if (editor == nullptr) return 2;
        w = editor->getWidth()  > 0 ? editor->getWidth()  : w;
        h = editor->getHeight() > 0 ? editor->getHeight() : h;
        editor->setBounds (0, 0, w, h);
        comp = std::move (editor);
    }

    juce::Image image (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (image);
        comp->paintEntireComponent (g, false);
    }

    juce::File out = (argc > 1)
        ? juce::File (juce::String (argv[1]))
        : juce::File::getCurrentWorkingDirectory().getChildFile ("bass_seq_ui.png");
    out.deleteFile();

    juce::FileOutputStream stream (out);
    if (! stream.openedOk())
        return 3;

    juce::PNGImageFormat png;
    png.writeImageToStream (image, stream);
    return 0;
}
