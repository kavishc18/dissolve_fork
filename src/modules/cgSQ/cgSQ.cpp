// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "modules/cgSQ/cgSQ.h"
#include "keywords/double.h"
#include "keywords/enumOptions.h"
#include "keywords/function1D.h"
#include "keywords/module.h"
#include "modules/cgGR/cgGR.h"

CGSQModule::CGSQModule() : Module(ModuleTypes::CGSQ)
{
    keywords_.addTarget<ModuleKeyword<const CGGRModule>>("SourceCGGR", "Source bead-bead g(r) to transform into S(Q)",
                                                         sourceCGGR_, ModuleTypes::CGGR);

    keywords_.setOrganisation("Options", "Range");
    keywords_.add<DoubleKeyword>("QMin", "Minimum Q for calculated S(Q)", qMin_, 0.0);
    keywords_.add<DoubleKeyword>("QMax", "Maximum Q for calculated S(Q)", qMax_, 0.0);
    keywords_.add<DoubleKeyword>("QDelta", "Step size in Q for S(Q) calculation", qDelta_, 1.0e-5);

    keywords_.setOrganisation("Options", "Broadening & Windowing");
    keywords_.add<Function1DKeyword>("QBroadening", "Instrument broadening function to apply when calculating S(Q)",
                                     qBroadening_);
    keywords_.add<EnumOptionsKeyword<WindowFunction::Form>>(
        "WindowFunction", "Window function to apply when Fourier-transforming g(r) to S(Q)", windowFunction_,
        WindowFunction::forms());
}

// Return source CGGR module
const CGGRModule *CGSQModule::sourceCGGR() const { return sourceCGGR_; }
