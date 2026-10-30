// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "classes/beadWeights.h"

/*
 * Construction
 */

void BeadWeights::clear()
{
    beadTypes_.clear();
    weights_.clear();
}

void BeadWeights::addBeadType(std::shared_ptr<BeadType> beadType, double fraction)
{
    beadTypes_.emplace_back(std::move(beadType), fraction);
}

int BeadWeights::nBeadTypes() const { return beadTypes_.size(); }

const std::shared_ptr<BeadType> &BeadWeights::beadType(int i) const { return beadTypes_[i].first; }

double BeadWeights::fraction(int i) const { return beadTypes_[i].second; }

std::vector<std::string> BeadWeights::beadTypeNames() const
{
    std::vector<std::string> names;
    names.reserve(beadTypes_.size());
    for (auto &[beadType, fraction] : beadTypes_)
        names.emplace_back(beadType->name());
    return names;
}

double BeadWeights::averageAtomsPerBead() const
{
    auto avg = 0.0;
    for (auto &[beadType, fraction] : beadTypes_)
        avg += fraction * beadType->totalAtomCount();
    return avg;
}

/*
 * Weights
 */

void BeadWeights::calculateWeights()
{
    auto n = static_cast<int>(beadTypes_.size());
    weights_.initialise(n, n, true);

    for (auto i = 0; i < n; ++i)
    {
        auto &[beadI, ci] = beadTypes_[i];
        auto bi = beadI->effectiveScatteringLength();
        for (auto j = i; j < n; ++j)
        {
            auto &[beadJ, cj] = beadTypes_[j];
            auto bj = beadJ->effectiveScatteringLength();
            weights_[{i, j}] = ci * cj * bi * bj * (i == j ? 1.0 : 2.0);
        }
    }
}

double BeadWeights::weight(int i, int j) const { return weights_[{i, j}]; }

const Array2D<double> &BeadWeights::weights() const { return weights_; }
