cmake_minimum_required(VERSION 3.22)

project(BassSeq VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: tell the compiler the source files are UTF-8 so non-ASCII characters
# (used in comments) compile correctly on Windows.
if (MSVC)
    add_compile_options(/utf-8)
endif()

# --- JUCE ---------------------------------------------------------------------
# FetchContent clones JUCE at configure time. The first run takes a while
# (a few hundred MB); afterwards it is cached in the build directory.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        2cdfca8feb300fb424002ba2c2751569e5bacb64  # tag 8.0.14, pinned to its commit SHA
    GIT_SHALLOW    FALSE                                     # full clone reliably checks out a pinned SHA
)
FetchContent_MakeAvailable(JUCE)

# --- Open303 DSP (isolated third-party library) -------------------------------
add_subdirectory(source/dsp/open303)

# --- The plugin ---------------------------------------------------------------
# Plugin formats per platform: iOS has no VST3/LV2 -> Standalone + AUv3.
if (IOS)
    set(BASSSEQ_FORMATS Standalone AUv3)
else()
    set(BASSSEQ_FORMATS VST3 LV2 Standalone)
endif()

juce_add_plugin(BassSeq
    COMPANY_NAME                "BASS SEQ"
    PLUGIN_MANUFACTURER_CODE    Evdl          # 4 chars, first uppercase
    PLUGIN_CODE                 Bsq1          # 4 chars, unique per plugin
    BUNDLE_ID                   com.evdleer.bassseq
    FORMATS                     ${BASSSEQ_FORMATS}
    PRODUCT_NAME                "BASS SEQ"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE          # note forwarding + MIDI clock out
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE          # copies to ~/.vst3 and ~/.lv2
    LV2URI                      "https://github.com/EvdLeer/bass-seq"
)

target_sources(BassSeq PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/dsp/Open303Voice.cpp
    source/dsp/Saturation.cpp
    source/dsp/Delay.cpp
    source/sequencer/StepSequencer.cpp
    source/sequencer/PatternBank.cpp
    source/sequencer/Arpeggiator.cpp
    source/ui/BassSeqLookAndFeel.cpp
    source/ui/PadsCabinet.cpp
    source/ui/EditController.cpp
    source/ui/SettingsOverlay.cpp
)

target_include_directories(BassSeq PRIVATE source)

target_compile_definitions(BassSeq PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(BassSeq
    PRIVATE
        open303
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# --- Headless UI render tool (optional, for visual checks) --------------------
option(BASSSEQ_BUILD_RENDER "Build the headless UI render tool" OFF)
if (BASSSEQ_BUILD_RENDER)
    juce_add_console_app(render_ui PRODUCT_NAME "render_ui")
    target_sources(render_ui PRIVATE
        tools/render_ui.cpp
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        source/dsp/Open303Voice.cpp
        source/dsp/Saturation.cpp
        source/dsp/Delay.cpp
        source/sequencer/StepSequencer.cpp
        source/sequencer/PatternBank.cpp
        source/sequencer/Arpeggiator.cpp
        source/ui/BassSeqLookAndFeel.cpp
        source/ui/PadsCabinet.cpp
        source/ui/EditController.cpp
        source/ui/SettingsOverlay.cpp
    )
    target_include_directories(render_ui PRIVATE source)
    target_compile_definitions(render_ui PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(render_ui PRIVATE
        open303
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()

# --- Headless state-machine logic test (optional) -----------------------------
option(BASSSEQ_BUILD_TEST "Build the headless EditController logic test" OFF)
if (BASSSEQ_BUILD_TEST)
    juce_add_console_app(test_editcontroller PRODUCT_NAME "test_editcontroller")
    target_sources(test_editcontroller PRIVATE
        tools/test_editcontroller.cpp
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        source/dsp/Open303Voice.cpp
        source/dsp/Saturation.cpp
        source/dsp/Delay.cpp
        source/sequencer/StepSequencer.cpp
        source/sequencer/PatternBank.cpp
        source/sequencer/Arpeggiator.cpp
        source/ui/BassSeqLookAndFeel.cpp
        source/ui/PadsCabinet.cpp
        source/ui/EditController.cpp
        source/ui/SettingsOverlay.cpp
    )
    target_include_directories(test_editcontroller PRIVATE source)
    target_compile_definitions(test_editcontroller PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(test_editcontroller PRIVATE
        open303
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
