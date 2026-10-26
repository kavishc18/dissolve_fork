// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#pragma once

#include "classes/beadPartialSet.h"
#include "math/function1D.h"
#include "math/windowFunction.h"
#include "module/module.h"

// Forward Declarations
class CGGRModule;

// CGSQ Module
//
// Fourier-transforms bead-bead g(r) (from a source CGGRModule) into unweighted bead-bead S(Q),
// analogous to SQModule but for coarse-grained beads (docs/ai-port/MIGRATION_DESIGN.md
// Increment 3).
//
// Deliberately does NOT apply any bead form factor here: per docs/ai-port/MUSIC_NOTES.md
// (section 1, "CG physics"), MuSSIC itself only applies the bead form factor FF(bead,Q) at the
// *weighted* (NeutronSQ-equivalent) stage, not to the unweighted S(Q) - the reference data this
// module validates against (CG-unweighted-*.sq) has no form factor baked in, so applying one
// here would be both scientifically wrong and would fail validation. The UniformSphere form
// factor added to Functions1D in this same slice is prep work for the future weighted module
// (Increment 4), not for this one - see DECISION_LOG.md.
class CGSQModule : public Module
{
    public:
    CGSQModule();
    ~CGSQModule() override = default;

    /*
     * Definition
     */
    private:
    // Source module for bead-bead g(r)
    const CGGRModule *sourceCGGR_{nullptr};
    // Minimum Q for calculated S(Q)
    double qMin_{0.01};
    // Maximum Q for calculated S(Q)
    double qMax_{30.0};
    // Step size in Q for S(Q) calculation
    double qDelta_{0.05};
    // Broadening function to apply to S(Q)
    Function1DWrapper qBroadening_;
    // Window function to use when Fourier-transforming g(r) to S(Q)
    WindowFunction::Form windowFunction_{WindowFunction::Form::None};

    public:
    // Return source CGGR module
    const CGGRModule *sourceCGGR() const;

    /*
     * Functions
     */
    public:
    // Calculate unweighted bead-bead S(Q) from unweighted bead-bead g(r)
    static bool calculateUnweightedSQ(const BeadPartialSet &unweightedgr, BeadPartialSet &unweightedsq, double qMin,
                                      double qDelta, double qMax, double rho, const WindowFunction &windowFunction,
                                      Function1DWrapper broadening);

    /*
     * Processing
     */
    private:
    // Run main processing
    Module::ExecutionResult process(ModuleContext &moduleContext) override;

    public:
    // Set target data
    void setTargets(const std::vector<std::unique_ptr<Configuration>> &configurations,
                    const std::map<ModuleTypes::ModuleType, std::vector<const Module *>> &moduleMap) override;
};
