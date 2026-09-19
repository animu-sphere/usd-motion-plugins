// SPDX-License-Identifier: Apache-2.0
//
// The frozen diagnostic set: its strings, its two halves, and its formatting.
//
// The string table is checked against a list written out here rather than
// against the library's own table, because the two would otherwise be the same
// statement twice: a rename in `Diagnostics.cpp` that broke every downstream
// matcher would keep a test that read the table green.
#include "motionBvh/Diagnostics.h"

#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include <string_view>

namespace
{

using openstrata::motion::bvh::Diagnostic;
using openstrata::motion::bvh::DiagnosticCode;
using openstrata::motion::bvh::DiagnosticSeverity;

constexpr std::string_view kExpectedStrings[] = {
    "MOTION_BVH_PARSE_FAILED",           "MOTION_BVH_UNSUPPORTED_CHANNEL",
    "MOTION_BVH_FRAME_WIDTH_MISMATCH",   "MOTION_BVH_INVALID_FRAME_TIME",
    "MOTION_BVH_NON_FINITE_VALUE",       "MOTION_BVH_PROFILE_REQUIRED",
    "MOTION_BVH_PROFILE_MISMATCH",       "MOTION_BVH_UNMAPPED_JOINT",
    "MOTION_BVH_REQUIRED_JOINT_MISSING", "MOTION_BVH_INVALID_ROTATION_ORDER",
    "MOTION_BVH_INVALID_ROOT_POLICY",
};

void
TestCodeStrings()
{
    static_assert(std::size(kExpectedStrings) == openstrata::motion::bvh::DiagnosticCodeCount,
                  "the set is frozen: a new code needs a contract change in "
                  "docs/roadmap/recorded-motion-sources.md §6 first");

    std::set<std::string_view> unique;
    for (std::size_t index = 0; index < openstrata::motion::bvh::DiagnosticCodeCount; ++index)
    {
        const auto code = static_cast<DiagnosticCode>(index);
        const std::string_view text = openstrata::motion::bvh::DiagnosticCodeString(code);
        assert(text == kExpectedStrings[index]);
        assert(text.rfind("MOTION_BVH_", 0) == 0);
        assert(unique.insert(text).second);
        assert(openstrata::motion::bvh::FindDiagnosticCode(text) == code);
    }

    // Not a code, not this namespace's, and not an enumerator spelling.
    assert(!openstrata::motion::bvh::FindDiagnosticCode("VRM_VMC_PACKET_MALFORMED"));
    assert(!openstrata::motion::bvh::FindDiagnosticCode("ParseFailed"));
    assert(!openstrata::motion::bvh::FindDiagnosticCode(""));
    assert(openstrata::motion::bvh::DiagnosticCodeString(DiagnosticCode::Count).empty());
}

// The split is the layer boundary: a reader raises the first five and nothing
// else, because it does not know what a profile is.
void
TestSyntaxAndSemanticHalves()
{
    assert(openstrata::motion::bvh::SyntaxDiagnosticCodeCount == 5);
    assert(openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::ParseFailed));
    assert(openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::UnsupportedChannel));
    assert(openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::FrameWidthMismatch));
    assert(openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::InvalidFrameTime));
    assert(openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::NonFiniteValue));

    assert(!openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::ProfileRequired));
    assert(!openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::ProfileMismatch));
    assert(!openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::UnmappedJoint));
    assert(!openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::RequiredJointMissing));
    assert(!openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::InvalidRotationOrder));
    assert(!openstrata::motion::bvh::DiagnosticIsSyntax(DiagnosticCode::InvalidRootPolicy));
}

// One code continues, the rest stop. Nothing in the syntax half is recoverable:
// there is no half-read document to continue from.
void
TestSeverityAndRecoverability()
{
    for (std::size_t index = 0; index < openstrata::motion::bvh::DiagnosticCodeCount; ++index)
    {
        const auto code = static_cast<DiagnosticCode>(index);
        const bool isUnmapped = code == DiagnosticCode::UnmappedJoint;
        assert(openstrata::motion::bvh::DiagnosticIsRecoverable(code) == isUnmapped);
        assert(openstrata::motion::bvh::DiagnosticDefaultSeverity(code) ==
               (isUnmapped ? DiagnosticSeverity::Warning : DiagnosticSeverity::Error));
        if (openstrata::motion::bvh::DiagnosticIsSyntax(code))
        {
            assert(!openstrata::motion::bvh::DiagnosticIsRecoverable(code));
        }
    }

    assert(openstrata::motion::bvh::DiagnosticSeverityString(DiagnosticSeverity::Info) == "info");
    assert(openstrata::motion::bvh::DiagnosticSeverityString(DiagnosticSeverity::Warning) == "warning");
    assert(openstrata::motion::bvh::DiagnosticSeverityString(DiagnosticSeverity::Error) == "error");
}

// MakeDiagnostic fills severity and recoverable from the code, so the two
// cannot silently disagree with the table.
void
TestMakeDiagnostic()
{
    const Diagnostic parse = openstrata::motion::bvh::MakeDiagnostic(DiagnosticCode::ParseFailed, "why");
    assert(parse.severity == DiagnosticSeverity::Error);
    assert(!parse.recoverable);
    assert(parse.detail == "why");
    assert(!parse.line);

    const Diagnostic unmapped = openstrata::motion::bvh::MakeDiagnostic(DiagnosticCode::UnmappedJoint);
    assert(unmapped.severity == DiagnosticSeverity::Warning);
    assert(unmapped.recoverable);
    assert(unmapped.detail.empty());
}

void
TestFormatting()
{
    Diagnostic diagnostic = openstrata::motion::bvh::MakeDiagnostic(DiagnosticCode::FrameWidthMismatch,
                                                      "expected 57 values, read 54");
    diagnostic.source = "capture.bvh";
    diagnostic.line = 42;
    diagnostic.subject = "frame 3";
    assert(openstrata::motion::bvh::FormatDiagnostic(diagnostic) ==
           "[MOTION_BVH_FRAME_WIDTH_MISMATCH] error source=capture.bvh line=42 "
           "subject=frame 3: expected 57 values, read 54");

    // Absent optional fields are omitted rather than printed empty.
    const Diagnostic bare = openstrata::motion::bvh::MakeDiagnostic(DiagnosticCode::ProfileRequired);
    assert(openstrata::motion::bvh::FormatDiagnostic(bare) == "[MOTION_BVH_PROFILE_REQUIRED] error");

    // `recoverable` is printed only when it is true -- the default is what
    // stops the read, and saying so on every line hides the one case that
    // does not.
    Diagnostic warning = openstrata::motion::bvh::MakeDiagnostic(DiagnosticCode::UnmappedJoint, "no mapping");
    warning.subject = "PropAnchor";
    assert(openstrata::motion::bvh::FormatDiagnostic(warning) ==
           "[MOTION_BVH_UNMAPPED_JOINT] warning recoverable subject=PropAnchor"
           ": no mapping");
}

} // namespace

int
main()
{
    TestCodeStrings();
    TestSyntaxAndSemanticHalves();
    TestSeverityAndRecoverability();
    TestMakeDiagnostic();
    TestFormatting();
    std::printf("motionBvh diagnostics: %zu code(s) verified\n", openstrata::motion::bvh::DiagnosticCodeCount);
    return 0;
}
