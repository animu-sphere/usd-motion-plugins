// SPDX-License-Identifier: Apache-2.0

#include "motionBvh/Diagnostics.h"

#include <array>
#include <cstdio>

namespace openstrata::motion::bvh
{

namespace
{

// Indexed by DiagnosticCode. The strings are the contract, so this table is
// written out rather than derived from the enumerator spelling: a rename in the
// enum must not silently rename a code a downstream tool matches on.
constexpr std::array<std::string_view, DiagnosticCodeCount> kCodeStrings = {
    "MOTION_BVH_PARSE_FAILED",           "MOTION_BVH_UNSUPPORTED_CHANNEL",
    "MOTION_BVH_FRAME_WIDTH_MISMATCH",   "MOTION_BVH_INVALID_FRAME_TIME",
    "MOTION_BVH_NON_FINITE_VALUE",       "MOTION_BVH_PROFILE_REQUIRED",
    "MOTION_BVH_PROFILE_MISMATCH",       "MOTION_BVH_UNMAPPED_JOINT",
    "MOTION_BVH_REQUIRED_JOINT_MISSING", "MOTION_BVH_INVALID_ROTATION_ORDER",
    "MOTION_BVH_INVALID_ROOT_POLICY",
};

} // namespace

std::string_view
DiagnosticCodeString(DiagnosticCode code) noexcept
{
    const auto index = static_cast<std::size_t>(code);
    if (index >= DiagnosticCodeCount)
    {
        return {};
    }
    return kCodeStrings[index];
}

std::optional<DiagnosticCode>
FindDiagnosticCode(std::string_view name) noexcept
{
    for (std::size_t index = 0; index < DiagnosticCodeCount; ++index)
    {
        if (kCodeStrings[index] == name)
        {
            return static_cast<DiagnosticCode>(index);
        }
    }
    return std::nullopt;
}

DiagnosticSeverity
DiagnosticDefaultSeverity(DiagnosticCode code) noexcept
{
    // Every code but one stops the read. `UnmappedJoint` is the exception
    // because a producer exporting props, markers, or a full-body rig beside
    // the humanoid is the normal case rather than a defect.
    if (code == DiagnosticCode::UnmappedJoint)
    {
        return DiagnosticSeverity::Warning;
    }
    return DiagnosticSeverity::Error;
}

bool
DiagnosticIsSyntax(DiagnosticCode code) noexcept
{
    return static_cast<std::size_t>(code) < SyntaxDiagnosticCodeCount;
}

bool
DiagnosticIsRecoverable(DiagnosticCode code) noexcept
{
    return code == DiagnosticCode::UnmappedJoint;
}

std::string_view
DiagnosticSeverityString(DiagnosticSeverity severity) noexcept
{
    switch (severity)
    {
    case DiagnosticSeverity::Info:
        return "info";
    case DiagnosticSeverity::Warning:
        return "warning";
    case DiagnosticSeverity::Error:
        return "error";
    }
    return "error";
}

Diagnostic
MakeDiagnostic(DiagnosticCode code, std::string detail)
{
    Diagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = DiagnosticDefaultSeverity(code);
    diagnostic.recoverable = DiagnosticIsRecoverable(code);
    diagnostic.detail = std::move(detail);
    return diagnostic;
}

std::string
FormatDiagnostic(const Diagnostic& diagnostic)
{
    std::string line;
    line += '[';
    line += DiagnosticCodeString(diagnostic.code);
    line += "] ";
    line += DiagnosticSeverityString(diagnostic.severity);
    if (diagnostic.recoverable)
    {
        line += " recoverable";
    }
    if (!diagnostic.source.empty())
    {
        line += " source=";
        line += diagnostic.source;
    }
    if (diagnostic.line)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), " line=%zu", *diagnostic.line);
        line += buffer;
    }
    if (!diagnostic.subject.empty())
    {
        line += " subject=";
        line += diagnostic.subject;
    }
    if (!diagnostic.detail.empty())
    {
        line += ": ";
        line += diagnostic.detail;
    }
    return line;
}

} // namespace openstrata::motion::bvh
