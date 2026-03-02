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

    # Phase 8: higher-level model tests (JUCE-free classes only)
    unit/model_layer/test_tempo_map.cpp
    unit/model_layer/test_grid_system.cpp
    unit/model_layer/test_clipboard.cpp

    # App-layer sources needed by model_layer unit tests (JUCE-free subset)
    ${CMAKE_SOURCE_DIR}/src/model/TempoMap.cpp
    ${CMAKE_SOURCE_DIR}/src/model/GridSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Clipboard.cpp
)

target_link_libraries(dc_unit_tests PRIVATE
    dc_foundation
    dc_model
    dc_midi
    dc_audio
    Catch2::Catch2WithMain
    trompeloeil::trompeloeil
)

# Unit tests include model/ headers which live in src/
target_include_directories(dc_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SNDFILE_INCLUDE_DIRS}
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
