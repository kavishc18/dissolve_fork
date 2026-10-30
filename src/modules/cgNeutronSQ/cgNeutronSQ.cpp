// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "modules/cgNeutronSQ/cgNeutronSQ.h"
#include "keywords/module.h"
#include "modules/cgSQ/cgSQ.h"

CGNeutronSQModule::CGNeutronSQModule() : Module(ModuleTypes::CGNeutronSQ)
{
    keywords_.addTarget<ModuleKeyword<const CGSQModule>>(
        "SourceCGSQ", "Source unweighted bead-bead S(Q) to apply neutron weights to", sourceCGSQ_, ModuleTypes::CGSQ);
}

// Return source CGSQ module
const CGSQModule *CGNeutronSQModule::sourceCGSQ() const { return sourceCGSQ_; }

// Set bead-type weights to use for weighting
void CGNeutronSQModule::setBeadWeights(BeadWeights weights) { beadWeights_ = std::move(weights); }
