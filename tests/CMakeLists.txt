# ─── Unit Tests (NO JUCE) ─────────────────────────────────────
add_executable(dc_unit_tests
    # Phase 3: foundation tests
    unit/foundation/test_colour.cpp
    unit/foundation/test_string_utils.cpp
    unit/foundation/test_base64.cpp
    unit/foundation/test_random.cpp
    unit/foundation/test_spsc_queue.cpp
    unit/foundation/test_message_queue.cpp
    unit/foundation/test_listener_list.cpp
    unit/foundation/test_worker_thread.cpp

    # Phase 4: model tests
    unit/model/test_variant.cpp
    unit/model/test_property_id.cpp
    unit/model/test_property_tree.cpp
    unit/model/test_undo_manager.cpp

    # Phase 5: midi tests
    unit/midi/test_midi_message.cpp
    unit/midi/test_midi_buffer.cpp
    unit/midi/test_midi_sequence.cpp

    # Phase 6: audio tests
    unit/audio/test_audio_block.cpp
    unit/audio/test_audio_file_io.cpp
    unit/audio/test_disk_streamer.cpp
    unit/audio/test_threaded_recorder.cpp

    # Plugin scanner tests
    unit/test_plugin_scanner.cpp

    # Plugin tests
    unit/plugins/test_probe_cache.cpp
    unit/plugins/test_plugin_description.cpp
    unit/plugins/test_vst3_module.cpp

    # Regression tests
    regression/issue_001_uid_from_file_path.cpp
    regression/issue_001_multi_yabridge_load.cpp
    regression/issue_002_juce_state_format.cpp
    regression/issue_003_set_state_deactivation.cpp

    # Phase 8: higher-level model tests (JUCE-free classes only)
    unit/model_layer/test_tempo_map.cpp
    unit/model_layer/test_grid_system.cpp
    unit/model_layer/test_clipboard.cpp

    # App-layer sources needed by model_layer unit tests (JUCE-free subset)
    ${CMAKE_SOURCE_DIR}/src/model/TempoMap.cpp
    ${CMAKE_SOURCE_DIR}/src/model/GridSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Clipboard.cpp

    # Plugin sources needed by plugin unit tests
    ${CMAKE_SOURCE_DIR}/src/dc/plugins/ProbeCache.cpp
    ${CMAKE_SOURCE_DIR}/src/dc/plugins/VST3Module.cpp
    "${VST3_SDK_DIR}/pluginterfaces/base/coreiids.cpp"
    "${VST3_SDK_DIR}/pluginterfaces/base/funknown.cpp"
)

target_link_libraries(dc_unit_tests PRIVATE
    dc_foundation
    dc_model
    dc_midi
    dc_audio
    Catch2::Catch2WithMain
    trompeloeil::trompeloeil
    yaml-cpp::yaml-cpp
    ${CMAKE_DL_LIBS}
)

# Unit tests include model/ headers which live in src/
target_include_directories(dc_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SNDFILE_INCLUDE_DIRS}
    "${VST3_SDK_DIR}"
)

# Ensure test-results directory exists for XML output
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test-results)

catch_discover_tests(dc_unit_tests
    TEST_PREFIX "unit."
    REPORTER XML
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)

# ─── Integration Tests ───────────────────────────────────────
add_executable(dc_integration_tests

    # Phase 7: integration tests
    integration/test_session_roundtrip.cpp
    integration/test_vim_commands.cpp
    integration/test_audio_graph.cpp

    # Phase 8: higher-level model tests (require JUCE or app-layer sources)
    integration/test_project.cpp
    integration/test_track.cpp
    integration/test_arrangement.cpp
    integration/test_vim_context.cpp
    integration/test_action_registry.cpp
    integration/test_transport.cpp

    # ─── App-layer sources needed by integration tests ────────
    # Model
    ${CMAKE_SOURCE_DIR}/src/model/Project.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Track.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Arrangement.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Clipboard.cpp
    ${CMAKE_SOURCE_DIR}/src/model/TempoMap.cpp
    ${CMAKE_SOURCE_DIR}/src/model/GridSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/model/StepSequencer.cpp
    ${CMAKE_SOURCE_DIR}/src/model/AudioClip.cpp
    ${CMAKE_SOURCE_DIR}/src/model/MidiClip.cpp
    ${CMAKE_SOURCE_DIR}/src/model/MixerState.cpp

    # Serialization
    ${CMAKE_SOURCE_DIR}/src/model/serialization/YAMLSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/model/serialization/SessionWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/model/serialization/SessionReader.cpp

    # Vim
    ${CMAKE_SOURCE_DIR}/src/vim/VimContext.cpp
    ${CMAKE_SOURCE_DIR}/src/vim/VimEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/vim/ActionRegistry.cpp

    # Engine
    ${CMAKE_SOURCE_DIR}/src/engine/TransportController.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/TrackProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/MixBusProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/dc/engine/MidiBlock.cpp

    # Utils
    ${CMAKE_SOURCE_DIR}/src/utils/UndoSystem.cpp
)

target_include_directories(dc_integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(dc_integration_tests PRIVATE
    dc_foundation
    dc_model
    dc_midi
    dc_audio
    Catch2::Catch2WithMain
    yaml-cpp::yaml-cpp
)

catch_discover_tests(dc_integration_tests
    TEST_PREFIX "integration."
    REPORTER XML
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)

# --- E2E tests (shell-based, exercise real binary) ---
if(BUILD_TESTING)
    add_test(NAME e2e.smoke
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_smoke.sh
                $<TARGET_FILE:DremCanvas>)
    set_tests_properties(e2e.smoke PROPERTIES
        LABELS "e2e" TIMEOUT 30)

    add_test(NAME e2e.load_project
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_load_project.sh
                $<TARGET_FILE:DremCanvas>
                ${CMAKE_SOURCE_DIR}/tests/fixtures/e2e-plugin-project)
    set_tests_properties(e2e.load_project PROPERTIES
        LABELS "e2e" TIMEOUT 60)

    add_test(NAME e2e.scan_cold
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_scan_cold.sh
                $<TARGET_FILE:DremCanvas>
                ${CMAKE_SOURCE_DIR}/tests/fixtures/e2e-scan-project)
    set_tests_properties(e2e.scan_cold PROPERTIES
        LABELS "e2e" TIMEOUT 90)

    add_test(NAME e2e.scan_warm
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_scan_warm.sh
                $<TARGET_FILE:DremCanvas>
                ${CMAKE_SOURCE_DIR}/tests/fixtures/e2e-scan-project)
    set_tests_properties(e2e.scan_warm PROPERTIES
        LABELS "e2e" TIMEOUT 90)

    add_test(NAME e2e.browser_scan
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_browser_scan.sh
                $<TARGET_FILE:DremCanvas>)
    set_tests_properties(e2e.browser_scan PROPERTIES
        LABELS "e2e" TIMEOUT 150)
endif()
