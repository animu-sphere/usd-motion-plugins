# SPDX-License-Identifier: Apache-2.0

# Live intake, the recorded trace format, replay and the recorder.
add_executable(motionRecording_liveCapture_tests test_live_capture.cpp)
target_link_libraries(motionRecording_liveCapture_tests
    PRIVATE motionRecording::motionRecording)
# This suite checks with `assert()`, which NDEBUG compiles away -- and Release
# is the default build type, so NDEBUG is undefined for it explicitly. A suite
# that asserts nothing in the configuration CI runs passes by construction.
target_compile_options(motionRecording_liveCapture_tests
    PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME motionRecording_liveCapture
         COMMAND motionRecording_liveCapture_tests)

set(_motion_recording_tests motionRecording_liveCapture)

# The corpus is the trace format's reproducibility contract: every committed
# trace must parse, and must survive a read/write round trip byte-identically,
# or every golden comparison downstream of it is meaningless. The same test
# binary takes the corpus directory as an argument -- deliberately, rather than
# referencing a target from another directory, because a standalone build of
# this library has no such target and would fail at generate time.
set(_motion_recording_corpus "${CMAKE_CURRENT_SOURCE_DIR}/corpus")
if(EXISTS "${_motion_recording_corpus}")
    add_test(NAME motionRecording_corpus
        COMMAND motionRecording_liveCapture_tests "${_motion_recording_corpus}")
    list(APPEND _motion_recording_tests motionRecording_corpus)
endif()

if(WIN32)
    if(TARGET gf)
        set(_motion_recording_gf gf)
    else()
        set(_motion_recording_gf pxr::gf)
    endif()
    # The libraries are static, but they call exported Gf entry points, so the
    # test executable loads usd_gf.dll at run time; motionSampling's tests say
    # what else that pulls in, and why a plain-CMake `ctest` needs the PATH.
    set(_motion_recording_test_path
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:motionRecording>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_motion_recording_gf}>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_motion_recording_gf}>/../bin")
    if(USDMOTION_TEST_PYTHON)
        get_filename_component(_motion_recording_python_dir
                               "${USDMOTION_TEST_PYTHON}" DIRECTORY)
        list(APPEND _motion_recording_test_path
             "PATH=path_list_prepend:${_motion_recording_python_dir}")
    endif()
    set_tests_properties(${_motion_recording_tests} PROPERTIES
        ENVIRONMENT_MODIFICATION "${_motion_recording_test_path}")
endif()

if(USDMOTION_TEST_PYTHON)
    add_test(NAME motionRecording_boundaries
        COMMAND "${USDMOTION_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/check_boundaries.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/.."
            "$<TARGET_FILE:motionRecording>")

    # motionRecording_corpus proves the committed traces are canonical for the
    # C++ writer; this proves they still match the generator that authored
    # them. Without it a hand-edited fixture would stay green while the
    # generator no longer reproduces it, and the corpus would quietly become
    # unregenerable.
    if(EXISTS "${_motion_recording_corpus}")
        add_test(NAME motionRecording_traceGen
            COMMAND "${USDMOTION_TEST_PYTHON}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../tools/generate_traces.py"
                --check --output "${_motion_recording_corpus}")
    endif()
endif()

# The layer's label, on every test this directory registers
# (`ctest -L motion.recording`).
get_property(_motion_recording_labelled DIRECTORY PROPERTY TESTS)
set_property(TEST ${_motion_recording_labelled}
    APPEND PROPERTY LABELS "motion.recording")
