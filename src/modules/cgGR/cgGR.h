// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#pragma once

#include "classes/beadPartialSet.h"
#include "module/module.h"
#include <optional>
#include <vector>

// Forward Declarations
class Configuration;
class SpeciesSite;

// CGGR Module
//
// Computes bead-bead radial distribution functions (full/bound/unbound) for coarse-grained
// "bead" pseudo-particles, analogous to GRModule but keyed by bead-type name instead of
// AtomType (see docs/ai-port/GAP_ANALYSIS.md and docs/ai-port/MIGRATION_DESIGN.md Increment 2).
//
// Bead positions are obtained from the existing SpeciesSite/SiteSelector machinery (one
// SpeciesSite per bead instance); bead-type identity is simply the defining SpeciesSite's own
// name, so e.g. every site named "CE" across any Species is treated as one instance of bead
// type "CE". This deliberately reuses SpeciesSiteVectorKeyword rather than inventing a new
// keyword type (open decision in MIGRATION_DESIGN.md, resolved this way for Increment 2).
//
// A deliberate scope cut from GRModule (logged in DECISION_LOG.md): this module uses a simple
// double loop only (no cell-list acceleration) since bead counts are expected to be far smaller
// than atom counts after coarse-graining, and supports a single target Configuration.
class CGGRModule : public Module
{
    public:
    CGGRModule();
    ~CGGRModule() override = default;

    /*
     * Definition
     */
    private:
    // Target configuration
    Configuration *targetConfiguration_{nullptr};
    // Bead site definitions (one SpeciesSite per bead instance; grouped into bead types by name)
    std::vector<const SpeciesSite *> beadSites_;
    // Bin width (spacing in r) to use
    double binWidth_{0.025};
    // Maximum r to calculate g(r) out to, unless left unset (then half-cell range is used)
    std::optional<double> requestedRange_;

    public:
    // Return target configuration
    Configuration *targetConfiguration() const;

    /*
     * Processing
     */
    private:
    // Run main processing
    Module::ExecutionResult process(ModuleContext &moduleContext) override;
};
