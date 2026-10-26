// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "math/ft.h"
#include "modules/cgSQ/cgSQ.h"

// Calculate unweighted bead-bead S(Q) from unweighted bead-bead g(r)
bool CGSQModule::calculateUnweightedSQ(const BeadPartialSet &unweightedgr, BeadPartialSet &unweightedsq, double qMin,
                                       double qDelta, double qMax, double rho, const WindowFunction &windowFunction,
                                       Function1DWrapper broadening)
{
    if (unweightedgr.nBeadTypes() != unweightedsq.nBeadTypes())
        return Messenger::error("CGSQModule::calculateUnweightedSQ - sizes of supplied partial sets are different.\n");

    auto nTypes = unweightedgr.nBeadTypes();
    for (auto i = 0; i < nTypes; ++i)
        for (auto j = i; j < nTypes; ++j)
        {
            // Full partial: subtract 1.0 to give (g(r)-1) before transforming
            unweightedsq.partial(i, j).copyArrays(unweightedgr.partial(i, j));
            unweightedsq.partial(i, j) -= 1.0;
            Fourier::sineFT(unweightedsq.partial(i, j), 4.0 * PI * rho, qMin, qDelta, qMax, windowFunction, broadening);

            // Bound partial: not referenced to the bulk density, so no subtraction of 1.0
            unweightedsq.boundPartial(i, j).copyArrays(unweightedgr.boundPartial(i, j));
            Fourier::sineFT(unweightedsq.boundPartial(i, j), 4.0 * PI * rho, qMin, qDelta, qMax, windowFunction, broadening);

            // Unbound partial
            unweightedsq.unboundPartial(i, j).copyArrays(unweightedgr.unboundPartial(i, j));
            unweightedsq.unboundPartial(i, j) -= 1.0;
            Fourier::sineFT(unweightedsq.unboundPartial(i, j), 4.0 * PI * rho, qMin, qDelta, qMax, windowFunction, broadening);
        }

    // Sum into total, weighted by bead-type concentration fractions (mirrors BeadPartialSet::formTotals)
    std::vector<double> fractions(nTypes);
    auto total = unweightedgr.totalBeadPopulation();
    for (auto i = 0; i < nTypes; ++i)
        fractions[i] = unweightedgr.beadTypePopulations()[i] / static_cast<double>(total);
    unweightedsq.formTotals(fractions);

    return true;
}
