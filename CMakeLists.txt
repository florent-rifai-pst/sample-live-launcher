cmake_minimum_required(VERSION 3.22)

project(SampleLiveLauncher VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE lives as a shallow clone in libs/JUCE
add_subdirectory(libs/JUCE)

juce_add_gui_app(SampleLiveLauncher
    PRODUCT_NAME "Sample Live Launcher"
    COMPANY_NAME "MidnightTheory"
    ICON_BIG "Resources/AppIcon.png")

juce_generate_juce_header(SampleLiveLauncher)

target_sources(SampleLiveLauncher PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/AudioEngine.cpp
    Source/Models.cpp
    Source/TrackRow.cpp
    Source/ClickTrackPanel.cpp
    Source/SoundBankView.cpp
    Source/SongsView.cpp
    Source/SetlistsView.cpp
    Source/LiveView.cpp)

if (MSVC)
    target_compile_options(SampleLiveLauncher PRIVATE /utf-8)
endif()

target_compile_definitions(SampleLiveLauncher PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SampleLiveLauncher,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SampleLiveLauncher,JUCE_VERSION>")

target_link_libraries(SampleLiveLauncher PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
