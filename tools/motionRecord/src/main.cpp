// SPDX-License-Identifier: Apache-2.0
//
// motion_record: a recorded trace becomes a motion stage (WORKSPACE.md §1.2).
// It arrived from usd-vrm-plugins as motion_capture and takes this
// repository's command name.
//
// It is the composition point, not the algorithm: `motionRecording` owns the
// intake and the recorder, `motionSampling` the buffer, `motionUsd` everything
// that touches a stage, and this file wires them together on a fixed tick and
// reports what happened.
//
// The loop below is the whole of "live capture" as this project defines it:
//
//     sender.Advance(now - deliveryLag)   // what has arrived by now
//     recorder.Record(source.Sample(now)) // what the consumer sees now
//
// A real adapter replaces the first line with a decoded packet and nothing
// else changes — which is why the replay is a faithful test and not a mock.
#include "Options.h"

#include "motionRecording/CaptureTrace.h"
#include "motionRecording/LiveCaptureSource.h"
#include "motionRecording/MotionRecorder.h"
#include "motionRecording/ReplaySender.h"
#include "motionSampling/MotionSource.h"
#include "motionUsd/ClipWriter.h"

#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace
{

std::string
Number(double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.6g", value);
    return buffer;
}

std::string
Count(unsigned long long value)
{
    return std::to_string(value);
}

const char*
MissingJointName(openstrata::motion::MissingJointPolicy policy)
{
    return policy == openstrata::motion::MissingJointPolicy::HoldLast ? "hold" : "unbound";
}

const char*
RootMotionName(openstrata::motion::RootMotionIntake intake)
{
    switch (intake)
    {
    case openstrata::motion::RootMotionIntake::Ignore:
        return "ignore";
    case openstrata::motion::RootMotionIntake::Passthrough:
        return "passthrough";
    case openstrata::motion::RootMotionIntake::DeriveVelocity:
        break;
    }
    return "derive";
}

void
PrintReport(const openstrata::motion::LiveCaptureSource& source,
            const openstrata::motion::RecordReport& report, std::size_t observedJoints)
{
    const openstrata::motion::LiveCaptureStats& stats = source.GetStats();
    std::printf("intake:     %s accepted, %s out-of-order, %s stale, %s empty\n",
                Count(stats.framesAccepted).c_str(),
                Count(stats.framesRejectedOutOfOrder).c_str(),
                Count(stats.framesRejectedStale).c_str(),
                Count(stats.framesRejectedEmpty).c_str());
    std::printf("joints:     %zu of %zu observed; %s gated by confidence, "
                "%s held, %s left unbound\n",
                observedJoints,
                openstrata::motion::HumanJointCount,
                Count(stats.jointsGatedByConfidence).c_str(),
                Count(stats.jointsHeld).c_str(),
                Count(stats.jointsUnbound).c_str());
    std::printf("root:       %s samples observed, %s velocities derived\n",
                Count(stats.rootSamplesObserved).c_str(),
                Count(stats.rootVelocitiesDerived).c_str());
    std::printf("evaluation: %zu ticks -> %zu sampled, %zu held, "
                "%zu extrapolated, %zu unavailable\n",
                report.ticks, report.sampled, report.held, report.extrapolated, report.unavailable);
    std::printf("peak lag:   %s s\n", Number(report.peakLagSeconds).c_str());
}

} // namespace

int
main(int argc, char** argv)
{
    const std::vector<std::string> arguments(argv + 1, argv + argc);

    motionRecordTool::Options options;
    bool showHelp = false;
    std::string error;
    if (!motionRecordTool::ParseOptions(arguments, &options, &showHelp, &error))
    {
        std::cerr << "motion_record: " << error << "\n\n" << motionRecordTool::GetUsage();
        return 2;
    }
    if (showHelp)
    {
        std::fputs(motionRecordTool::GetUsage(), stdout);
        return 0;
    }

    openstrata::motion::MotionClip trace;
    openstrata::motion::CaptureTraceError traceError;
    if (!openstrata::motion::ReadCaptureTraceFile(options.tracePath, &trace, &traceError))
    {
        std::cerr << "motion_record: " << options.tracePath;
        if (traceError.line != 0)
        {
            std::cerr << ":" << traceError.line;
        }
        std::cerr << ": " << traceError.message << "\n";
        return 1;
    }

    if (!options.normalizePath.empty())
    {
        if (!openstrata::motion::WriteCaptureTraceFile(options.normalizePath, trace))
        {
            std::cerr << "motion_record: could not write " << options.normalizePath << "\n";
            return 1;
        }
        if (!options.quiet)
        {
            std::cerr << "motion_record: normalized " << trace.samples.size() << " frame(s) into "
                      << options.normalizePath << "\n";
        }
        return 0;
    }

    const double rate =
        options.evaluationRate > 0.0 ? options.evaluationRate : trace.nominalFrameRate;

    openstrata::motion::LiveCaptureSource source(options.capture);
    source.SetSourceMetadata(trace.source);

    openstrata::motion::ReplaySender sender(trace, &source);
    openstrata::motion::MotionRecorder recorder(rate);

    // The session runs from the first recorded frame to the last, plus enough
    // ticks to consume the delivery lag -- otherwise the tail of the trace is
    // delivered and never evaluated.
    //
    // The tolerance matters at both ends. Without it, a span that computes to
    // 59.999999 ticks truncates to 59 and drops the final frame; with a `<=`
    // loop instead of `<`, a lag-free replay would emit one tick past the end
    // of the trace and report a phantom extrapolation.
    const double duration = trace.endTime - trace.startTime;
    const double span = duration + options.deliveryLag;
    const double tolerance = openstrata::motion::PoseSampleTimeTolerance;
    const auto tickCount = static_cast<std::size_t>(span * rate + tolerance * rate) + 1;

    // Pin the capture clock to the consumer's: a trace may be stamped in any
    // epoch (a real session's timestamps start wherever the device's clock
    // was), and the authored clip has to start at zero.
    source.SetClockOffset(trace.startTime);

    for (std::size_t tick = 0; tick < tickCount; ++tick)
    {
        const double now = static_cast<double>(tick) / rate;
        sender.Advance(trace.startTime + now - options.deliveryLag);
        if (source.IsEmpty())
        {
            // Nothing has arrived yet. Recording an unavailable tick would open
            // the clip with a gap instead of starting it when the session
            // actually started producing.
            continue;
        }
        recorder.Record(source.Sample(now));
    }

    const openstrata::motion::RecordReport report = recorder.GetReport();
    const openstrata::motion::MotionClip recorded = recorder.Take();

    if (recorded.samples.empty())
    {
        std::cerr << "motion_record: the replay produced no evaluated "
                     "frames\n";
        return 1;
    }

    if (!options.quiet && report.unavailable != 0)
    {
        std::cerr << "motion_record: warning: " << report.unavailable
                  << " tick(s) could not be answered by the source\n";
    }
    if (!options.quiet && source.GetStats().framesRejectedOutOfOrder != 0)
    {
        std::cerr << "motion_record: warning: " << source.GetStats().framesRejectedOutOfOrder
                  << " frame(s) arrived out of order; the source's clock is "
                     "not monotonic\n";
    }

    if (options.report)
    {
        PrintReport(source, report, source.GetObservedJoints().count());
    }

    if (options.dryRun)
    {
        return 0;
    }

    std::map<std::string, std::string> provenance;
    provenance["kind"] = "liveCapture";
    provenance["provider"] = recorded.source.provider;
    provenance["protocol"] = recorded.source.protocol;
    provenance["sourceId"] = recorded.source.sourceId;
    provenance["trace"] = options.tracePath;
    provenance["evaluationRate"] = Number(rate);
    provenance["deliveryLag"] = Number(options.deliveryLag);
    provenance["confidenceFloor"] = Number(options.capture.confidenceFloor);
    provenance["missingJoints"] = MissingJointName(options.capture.missingJoints);
    provenance["rootMotion"] = RootMotionName(options.capture.rootMotion);
    provenance["smoothingCutoffHz"] = Number(options.capture.smoothingCutoffHz);
    provenance["framesEvaluated"] = Count(report.ticks);
    provenance["framesSampled"] = Count(report.sampled);
    provenance["framesHeld"] = Count(report.held);
    provenance["framesExtrapolated"] = Count(report.extrapolated);
    provenance["peakLagSeconds"] = Number(report.peakLagSeconds);

    // A capture reports rotations relative to the canonical rest, never a rest
    // of its own, so the stage takes motionUsd's capture rest: identity, with
    // the hips at the session's first root position (USD_MAPPING.md §3).
    openstrata::motion::MotionStageOptions stageOptions;
    stageOptions.sourceFormat = "capture";
    stageOptions.rootMotionSource = "root position";
    stageOptions.provenance = provenance;

    openstrata::motion::MotionStageReport stageReport;
    if (!openstrata::motion::WriteMotionStage(options.outputPath, recorded, stageOptions,
                                              &stageReport, &error))
    {
        std::cerr << "motion_record: " << error << "\n";
        return 1;
    }

    // What the stage cannot hold yet is said, not dropped in silence: a look-at
    // target still has no place in the mapping. Channels do, since USD-O4, so
    // they are counted below rather than warned about.
    if (!options.quiet && stageReport.unauthoredLookAtTargets != 0)
    {
        std::cerr << "motion_record: warning: " << stageReport.unauthoredLookAtTargets
                  << " sample(s) carried a look-at target that was not authored\n";
    }

    if (!options.quiet)
    {
        std::cerr << "motion_record: wrote " << recorded.samples.size() << " frame(s) over "
                  << source.GetObservedJoints().count() << " observed joint(s) and "
                  << stageReport.channels.size() << " channel(s) to " << options.outputPath
                  << "\n";
    }
    return 0;
}
