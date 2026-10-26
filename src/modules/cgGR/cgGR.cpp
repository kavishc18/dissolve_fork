// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "modules/cgGR/cgGR.h"
#include "keywords/configuration.h"
#include "keywords/double.h"
#include "keywords/optionalDouble.h"
#include "keywords/speciesSiteVector.h"

CGGRModule::CGGRModule() : Module(ModuleTypes::CGGR)
{
    keywords_.addTarget<ConfigurationKeyword>("Configuration", "Set target configuration for the module",
                                              targetConfiguration_);

    keywords_.setOrganisation("Options", "Bead Sites");
    keywords_.add<SpeciesSiteVectorKeyword>(
        "BeadSites", "Set the site(s) representing coarse-grained bead instances (grouped into bead types by site name)",
        beadSites_);

    keywords_.setOrganisation("Options", "Range");
    keywords_.add<DoubleKeyword>("BinWidth", "Bin width (spacing in r) to use", binWidth_, 1.0e-3);
    keywords_.add<OptionalDoubleKeyword>("Range", "Maximum r to calculate g(r) out to", requestedRange_, 0.0, std::nullopt, 1.0,
                                         "Use Half Cell Range");
}

// Return target configuration
Configuration *CGGRModule::targetConfiguration() const { return targetConfiguration_; }
