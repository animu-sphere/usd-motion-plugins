#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Negative fixtures for the connector gate, including real CMake closures."""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import tempfile
import unittest

import check_connector_boundaries as rules

CMAKE = "cmake"


class Boundaries(unittest.TestCase):
    def test_forbidden_includes(self):
        for header in (
            "motionConnectorCore/MotionFrame.h", "motion-connectors/api.h", "vmc/sdk.h",
            "openxr/openxr.h", "mediapipe/framework.h", "osc/OscPacketListener.h",
            "oscpack/ip/UdpSocket.h", "websocketpp/client.hpp", "ixwebsocket/IXWebSocket.h",
            "libwebsockets.h", "boost/asio.hpp", "boost/beast.hpp", "asio.hpp",
            "winsock2.h", "sys/socket.h", "netinet/in.h", "netdb.h", "curl/curl.h",
            "emscripten/html5.h", "node_api.h", "JavaScriptCore/JavaScript.h",
            "xsens/xsens.h", "k4a/k4a.h", "librealsense2/rs.h", "LeapC.h",
            "NatNetClient.h", "ViconDataStreamSDK/Client.h", "mocopi/api.h",
        ):
            with self.subTest(header=header):
                self.assertTrue(rules.source_errors(f'#include <{header}>\n', "motionRecording", "fixture"))

    def test_owned_type_even_without_include(self):
        for name in ("MotionFrame", "IMotionConnector", "TrackerObservation"):
            with self.subTest(name=name):
                self.assertTrue(rules.source_errors(f"struct {name};\n", "motionUsd", "fixture"))

    def test_provenance_and_comments_are_values(self):
        text = '''// #include <openxr/openxr.h> and TrackerObservation stay upstream.
/* MotionFrame is a connector type. */
pose.metadata.provider = "vmc";
pose.metadata.protocol = "websocket";
const char *url = "https://example.com/osc/TrackerObservation";
const char *description = R"text(
#include <sys/socket.h>
TrackerObservation; "vmc" == sourceName)text";
#include <motionCore/MotionPose.h>
#include <pxr/base/gf/vec3f.h>
float Oscillation = 0;
'''
        self.assertEqual(rules.source_errors(text, "motionRecording", "fixture"), [])
        product_text, errors = rules.product_check_text(text, "motionRecording", "fixture")
        self.assertEqual(errors, [])
        self.assertNotIn('"vmc"', product_text)
        product_text, errors = rules.product_check_text('#include "vrm/api.h"', "motionRecording", "fixture")
        self.assertIn('"vrm/api.h"', product_text)

    def test_source_specific_comparisons(self):
        for expression in ('sourceName == "vmc"', '"mocopi" != sourceName',
                           'sourceName.compare(\n "openxr")', 'strcmp(sourceName, "vmc")',
                           'sourceName.contains("mediapipe")'):
            with self.subTest(expression=expression):
                self.assertTrue(rules.source_errors(f"if ({expression}) {{}}", "motionSampling", "fixture"))

    def test_spliced_include(self):
        self.assertTrue(rules.source_errors('#inc\\\nlude <openxr/openxr.h>', "motionCore", "fixture"))

    def test_socket_calls_without_header(self):
        self.assertTrue(rules.source_errors('socket(AF_INET, SOCK_DGRAM, 0);', "motionRecording", "fixture"))

    def test_foundation_socket_allowance_is_scoped(self):
        arch = {"imported": True, "properties": ["C:/sdk/lib/usd_arch.lib", "Ws2_32"], "edges": []}
        root = {"properties": [], "edges": ["arch"]}
        graph = {"schema": 1, "roots": ["motionCore"], "targets": {"motionCore": root, "arch": arch}}
        self.assertEqual(rules.graph_errors(graph), [])
        root["properties"] = ["Ws2_32"]
        self.assertTrue(rules.graph_errors(graph))
        root["properties"] = []
        arch["imported"] = False
        self.assertTrue(rules.graph_errors(graph))
        arch["imported"] = True
        arch["properties"] = ["C:/unrelated/arch.lib", "Ws2_32"]
        self.assertTrue(rules.graph_errors(graph))

    def test_core_layering(self):
        for header in ("motionSampling/PoseFilter.h", "motionRetarget/Retarget.h",
                       "pxr/usd/usd/stage.h", "pxr/exec/exec/system.h"):
            with self.subTest(header=header):
                self.assertTrue(rules.source_errors(f'#include <{header}>', "motionCore", "fixture"))
        self.assertEqual(rules.source_errors('#include <pxr/usd/usd/stage.h>', "motionUsd", "fixture"), [])
        self.assertEqual(rules.source_errors('#include <pxr/exec/exec/system.h>', "execMotion", "fixture"), [])

    def test_manifest_dependencies(self):
        for dependency in ("motionConnectorVmc", "OpenXR::Loader", "MediaPipe", "oscpack",
                           "websocketpp", "XsensSDK", "motion-connectors"):
            with self.subTest(dependency=dependency):
                self.assertTrue(rules.manifest_errors(
                    f'requires:\n  libraries:\n    - id: "{dependency}"\n', "fixture"))
        self.assertEqual(rules.manifest_errors('''# motion-connectors is an upstream consumer
library:
  id: motionRecording
requires:
  libraries:
    - id: motionCore # no OpenXR
      version: ">=0.5,<0.6"
provenance: "mocopi"
''', "fixture"), [])

    def test_missing_graph_is_not_a_pass(self):
        self.assertTrue(rules.graph_errors({}))
        self.assertTrue(rules.graph_errors({"schema": 1, "roots": ["motionCore"], "targets": {"other": {}}}))

    def test_link_flags_and_core_stage_libraries(self):
        for dependency in ("-lopenxr_loader", "-lws2_32", "C:/sdk/lib/libusd.so",
                           "$<LINK_ONLY:pxr::usd>", "C:/sdk/lib/usd_sdf.lib", "pxr::exec"):
            with self.subTest(dependency=dependency):
                graph = {"schema": 1, "roots": ["motionCore"], "targets": {
                    "motionCore": {"properties": [dependency], "edges": []}}}
                self.assertTrue(rules.graph_errors(graph))
        for dependency in ("usdmotion::pxr::gf", "C:/sdk/lib/usd_gf.lib", "libgf.so"):
            with self.subTest(dependency=dependency):
                graph = {"schema": 1, "roots": ["motionCore"], "targets": {
                    "motionCore": {"properties": [dependency], "edges": []}}}
                self.assertEqual(rules.graph_errors(graph), [])
        installed = {"schema": 1, "roots": ["motionCore::motionCore"], "targets": {
            "motionCore::motionCore": {"properties": ["motionSampling::motionSampling"], "edges": []}}}
        self.assertTrue(rules.graph_errors(installed))

    def test_configured_cmake_graph(self):
        # No compiler or USD SDK is needed: real interface/imported targets
        # reproduce private wrappers, aliases, cycles, genex and import paths.
        module = (rules.REPO / "cmake/UsdMotionBoundaryGraph.cmake").as_posix()
        with tempfile.TemporaryDirectory(prefix="motion-boundary-") as directory:
            root = pathlib.Path(directory)
            source = f'''cmake_minimum_required(VERSION 3.22)
project(BoundaryFixture NONE)
include("{module}")
add_library(motionCore INTERFACE)
add_library(gf INTERFACE)
add_library(wrapper INTERFACE)
add_library(wrapperAlias ALIAS wrapper)
target_link_libraries(motionCore INTERFACE gf "$<LINK_ONLY:wrapperAlias>")
target_link_libraries(wrapper INTERFACE gf)
target_link_libraries(gf INTERFACE wrapper)
if(FORBIDDEN)
  add_library(neutral_sdk STATIC IMPORTED)
  set_target_properties(neutral_sdk PROPERTIES IMPORTED_CONFIGURATIONS RELEASE
    IMPORTED_LOCATION_RELEASE "${{CMAKE_CURRENT_BINARY_DIR}}/libOpenXR_loader.a")
  target_link_libraries(wrapper INTERFACE "$<$<BOOL:1>:neutral_sdk>" ws2_32)
endif()
if(CORE_LAYER)
  add_library(motionSampling INTERFACE)
  target_link_libraries(wrapper INTERFACE motionSampling)
endif()
usdmotion_write_boundary_graph("${{CMAKE_BINARY_DIR}}/graph.json" motionCore)
'''
            (root / "CMakeLists.txt").write_text(source, encoding="utf-8")
            for mode, should_fail in (("clean", False), ("FORBIDDEN", True), ("CORE_LAYER", True)):
                with self.subTest(mode=mode):
                    build = root / mode
                    # Use a platform-native generator without needing Ninja.
                    command = [CMAKE, "-S", str(root), "-B", str(build)]
                    if should_fail:
                        command.append(f"-D{mode}=ON")
                    result = subprocess.run(command, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    graph = json.loads((build / "graph.json").read_text(encoding="utf-8"))
                    self.assertIn("wrapperAlias", graph["targets"]["motionCore"]["edges"])
                    errors = rules.graph_errors(graph)
                    self.assertEqual(bool(errors), should_fail, errors)
                    if mode == "FORBIDDEN":
                        self.assertTrue(any("OpenXR" in error for error in errors), errors)
                        self.assertTrue(any("ws2_32" in error for error in errors), errors)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default="cmake")
    args, unittest_args = parser.parse_known_args()
    CMAKE = args.cmake
    unittest.main(argv=[__file__, *unittest_args])
