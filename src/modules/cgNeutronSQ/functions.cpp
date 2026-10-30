// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "modules/cgNeutronSQ/cgNeutronSQ.h"

// Calculate weighted bead-bead S(Q) (and the weighted total F_CG(Q)) from unweighted S(Q)
bool CGNeutronSQModule::calculateWeightedSQ(const BeadPartialSet &unweightedsq, BeadPartialSet &weightedsq,
                                            const BeadWeights &beadWeights)
{
    auto nTypes = unweightedsq.nBeadTypes();
    if (nTypes != beadWeights.nBeadTypes())
        return Messenger::error(
            "CGNeutronSQModule::calculateWeightedSQ - number of bead types in source S(Q) ({}) and BeadWeights ({}) "
            "don't match.\n",
            nTypes, beadWeights.nBeadTypes());

    auto avgNoaBead = beadWeights.averageAtomsPerBead();
    if (avgNoaBead <= 0.0)
        return Messenger::error("CGNeutronSQModule::calculateWeightedSQ - average atoms-per-bead is not positive.\n");

    const auto &qAxis = unweightedsq.partial(0, 0).xAxis();
    auto nQ = static_cast<int>(qAxis.size());

    // Precompute each bead type's form factor over the Q grid once
    std::vector<std::vector<double>> formFactor(nTypes, std::vector<double>(nQ));
    for (auto i = 0; i < nTypes; ++i)
    {
        const auto &bt = beadWeights.beadType(i);
        for (auto q = 0; q < nQ; ++q)
            formFactor[i][q] = bt->formFactorAt(qAxis[q]);
    }

    // Weighted bead-bead pair partials: apply BeadWeights::weight(i,j) (already c_i*c_j*b_i*b_j*
    // (2-delta_ij)-weighted) and the product of the two bead types' form factors, normalised by
    // the average number of atoms per bead (MuSSIC's avg_noa_bead - stands in for Soper's N_A)
    for (auto i = 0; i < nTypes; ++i)
        for (auto j = i; j < nTypes; ++j)
        {
            auto weight = beadWeights.weight(i, j);

            weightedsq.boundPartial(i, j).copyArrays(unweightedsq.boundPartial(i, j));
            weightedsq.unboundPartial(i, j).copyArrays(unweightedsq.unboundPartial(i, j));
            weightedsq.partial(i, j).copyArrays(unweightedsq.partial(i, j));

            for (auto q = 0; q < nQ; ++q)
            {
                auto factor = weight * formFactor[i][q] * formFactor[j][q] / avgNoaBead;
                weightedsq.boundPartial(i, j).value(q) *= factor;
                weightedsq.unboundPartial(i, j).value(q) *= factor;
                weightedsq.partial(i, j).value(q) = weightedsq.boundPartial(i, j).value(q) + weightedsq.unboundPartial(i, j).value(q);
            }
        }

    // Sum pair partials into totals - factor=1 since BeadWeights::weight() already includes the
    // c_i*c_j*(2-delta_ij) factor for every stored (i,j) entry (mirrors
    // NeutronSQModule::calculateWeightedSQ calling PartialSet::formTotals(false))
    weightedsq.formTotalsPreWeighted();

    // Add the per-bead-type self/single-bead scattering term to the grand total only (not to the
    // bound/unbound totals individually - matches MuSSIC's neutronsq_cg.F exactly: self_term is
    // only ever added to d_totalweightedsq)
    for (auto q = 0; q < nQ; ++q)
    {
        auto selfSum = 0.0;
        for (auto i = 0; i < nTypes; ++i)
            selfSum += beadWeights.fraction(i) * beadWeights.beadType(i)->selfScatteringWeight() * formFactor[i][q] *
                      formFactor[i][q] / avgNoaBead;
        weightedsq.total().value(q) += selfSum;
    }

    return true;
}
