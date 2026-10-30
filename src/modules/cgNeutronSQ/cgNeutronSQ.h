// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#pragma once

#include "classes/beadPartialSet.h"
#include "classes/beadWeights.h"
#include "module/module.h"

// Forward Declarations
class CGSQModule;

// CGNeutronSQ Module
//
// Applies neutron scattering weights to a source CGSQModule's unweighted bead-bead S(Q),
// producing the weighted total F_CG(Q) - the headline validation target for the whole
// AI-assisted MUSIC->Dissolve coarse-grained port (docs/ai-port/MIGRATION_DESIGN.md
// Increment 4). Analogous to NeutronSQModule, but:
//
// - Uses BeadWeights (bead-pair cross term, c_i*c_j*b_i*b_j*(2-delta_ij)) instead of
//   NeutronWeights, plus an extra per-bead-type "self/single-bead scattering" term
//   (BeadType::selfScatteringWeight()) that atomistic NeutronSQ has no equivalent of - it
//   accounts for scattering between the real atoms notionally inside a single bead, which
//   isn't captured by any bead-bead pair partial. See docs/ai-port/DECISION_LOG.md Slice 4
//   entry for the full derivation, verified directly against MuSSIC's neutronsq_cg.F source.
// - Applies each bead type's Q-dependent form factor (BeadType::formFactorAt()) to every
//   term - this is where UniformSphere (added in Slice 3) and Gaussian actually get used;
//   CGSQModule deliberately does not apply any form factor, since MuSSIC itself only applies
//   it here, at the weighted stage.
//
// Bead type definitions (composition, scattering lengths, radius, form factor) are not yet
// wired through the input-file keyword system (same scope cut as BeadType/BeadWeights in
// Slice 1) - set programmatically via setBeadWeights() after construction, before the module
// is first processed. Wiring this through real input-file keywords is deferred to a later pass.
class CGNeutronSQModule : public Module
{
    public:
    CGNeutronSQModule();
    ~CGNeutronSQModule() override = default;

    /*
     * Definition
     */
    private:
    // Source module for unweighted bead-bead S(Q)
    const CGSQModule *sourceCGSQ_{nullptr};
    // Bead-type weights (composition, scattering lengths, concentrations) - set programmatically,
    // not yet via keywords (see class comment)
    BeadWeights beadWeights_;

    public:
    // Return source CGSQ module
    const CGSQModule *sourceCGSQ() const;
    // Set bead-type weights to use for weighting (must be called before first processing)
    void setBeadWeights(BeadWeights weights);

    /*
     * Functions
     */
    public:
    // Calculate weighted bead-bead S(Q) (and the weighted total F_CG(Q)) from unweighted S(Q)
    static bool calculateWeightedSQ(const BeadPartialSet &unweightedsq, BeadPartialSet &weightedsq,
                                    const BeadWeights &beadWeights);

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
