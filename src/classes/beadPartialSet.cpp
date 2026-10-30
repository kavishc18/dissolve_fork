// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "classes/beadPartialSet.h"
#include "classes/partialSet.h"
#include "templates/algorithms.h"
#include <algorithm>
#include <format>
#include <numeric>

/*
 * Bead Types
 */

void BeadPartialSet::setUpPartials(std::vector<std::string> beadTypeNames)
{
    beadTypeNames_ = std::move(beadTypeNames);
    auto nTypes = static_cast<int>(beadTypeNames_.size());

    partials_.initialise(nTypes, nTypes, true);
    boundPartials_.initialise(nTypes, nTypes, true);
    unboundPartials_.initialise(nTypes, nTypes, true);

    for (auto i = 0; i < nTypes; ++i)
        for (auto j = i; j < nTypes; ++j)
        {
            partials_[{i, j}].setTag(std::format("{}-{}//Full", beadTypeNames_[i], beadTypeNames_[j]));
            boundPartials_[{i, j}].setTag(std::format("{}-{}//Bound", beadTypeNames_[i], beadTypeNames_[j]));
            unboundPartials_[{i, j}].setTag(std::format("{}-{}//Unbound", beadTypeNames_[i], beadTypeNames_[j]));
        }

    total_.setTag("Total");
    boundTotal_.setTag("BoundTotal");
    unboundTotal_.setTag("UnboundTotal");
    total_.clear();
    boundTotal_.clear();
    unboundTotal_.clear();

    nFramesAccumulated_ = 0;
    fingerprint_ = "NO_FINGERPRINT";
}

void BeadPartialSet::setUp(std::vector<std::string> beadTypeNames, double rdfRange, double binWidth)
{
    auto nTypes = static_cast<int>(beadTypeNames.size());
    setUpPartials(std::move(beadTypeNames));

    fullHistograms_.initialise(nTypes, nTypes, true);
    boundHistograms_.initialise(nTypes, nTypes, true);
    unboundHistograms_.initialise(nTypes, nTypes, true);
    for (auto i = 0; i < nTypes; ++i)
        for (auto j = i; j < nTypes; ++j)
        {
            fullHistograms_[{i, j}].initialise(0.0, rdfRange, binWidth);
            boundHistograms_[{i, j}].initialise(0.0, rdfRange, binWidth);
            unboundHistograms_[{i, j}].initialise(0.0, rdfRange, binWidth);
        }

    nFramesAccumulated_ = 0;
    fingerprint_ = "NO_FINGERPRINT";
}

void BeadPartialSet::reset()
{
    auto nTypes = static_cast<int>(beadTypeNames_.size());
    for (auto i = 0; i < nTypes; ++i)
        for (auto j = i; j < nTypes; ++j)
        {
            fullHistograms_[{i, j}].zeroBins();
            boundHistograms_[{i, j}].zeroBins();
            unboundHistograms_[{i, j}].zeroBins();
            std::ranges::fill(partials_[{i, j}].values(), 0.0);
            std::ranges::fill(boundPartials_[{i, j}].values(), 0.0);
            std::ranges::fill(unboundPartials_[{i, j}].values(), 0.0);
        }
    std::ranges::fill(total_.values(), 0.0);
    std::ranges::fill(boundTotal_.values(), 0.0);
    std::ranges::fill(unboundTotal_.values(), 0.0);

    nFramesAccumulated_ = 0;
    fingerprint_ = "NO_FINGERPRINT";
}

int BeadPartialSet::nBeadTypes() const { return static_cast<int>(beadTypeNames_.size()); }

const std::vector<std::string> &BeadPartialSet::beadTypeNames() const { return beadTypeNames_; }

std::optional<int> BeadPartialSet::indexOf(std::string_view beadTypeName) const
{
    auto it = std::find(beadTypeNames_.begin(), beadTypeNames_.end(), beadTypeName);
    if (it == beadTypeNames_.end())
        return {};
    return static_cast<int>(std::distance(beadTypeNames_.begin(), it));
}

void BeadPartialSet::setFingerprint(std::string_view fingerprint) { fingerprint_ = fingerprint; }

std::string_view BeadPartialSet::fingerprint() const { return fingerprint_; }

void BeadPartialSet::incrementFrameCount() { ++nFramesAccumulated_; }

int BeadPartialSet::nFramesAccumulated() const { return nFramesAccumulated_; }

/*
 * Histograms
 */

Histogram1D &BeadPartialSet::fullHistogram(int i, int j) { return fullHistograms_[{i, j}]; }

Histogram1D &BeadPartialSet::boundHistogram(int i, int j) { return boundHistograms_[{i, j}]; }

Histogram1D &BeadPartialSet::unboundHistogram(int i, int j) { return unboundHistograms_[{i, j}]; }

/*
 * Partials
 */

Data1D &BeadPartialSet::partial(int i, int j) { return partials_[{i, j}]; }
const Data1D &BeadPartialSet::partial(int i, int j) const { return partials_[{i, j}]; }

Data1D &BeadPartialSet::unboundPartial(int i, int j) { return unboundPartials_[{i, j}]; }
const Data1D &BeadPartialSet::unboundPartial(int i, int j) const { return unboundPartials_[{i, j}]; }

Data1D &BeadPartialSet::boundPartial(int i, int j) { return boundPartials_[{i, j}]; }
const Data1D &BeadPartialSet::boundPartial(int i, int j) const { return boundPartials_[{i, j}]; }

Data1D &BeadPartialSet::total() { return total_; }
const Data1D &BeadPartialSet::total() const { return total_; }

Data1D &BeadPartialSet::boundTotal() { return boundTotal_; }
const Data1D &BeadPartialSet::boundTotal() const { return boundTotal_; }

Data1D &BeadPartialSet::unboundTotal() { return unboundTotal_; }
const Data1D &BeadPartialSet::unboundTotal() const { return unboundTotal_; }

/*
 * Manipulation
 */

// Form partials (g(r)) from accumulated histogram data
void BeadPartialSet::formPartials(double boxVolume, const std::vector<int> &beadTypePopulations)
{
    beadTypePopulations_ = beadTypePopulations;

    auto nTypes = nBeadTypes();
    auto nFrames = std::max(nFramesAccumulated_, 1);
    for (auto i = 0; i < nTypes; ++i)
        for (auto j = i; j < nTypes; ++j)
        {
            // multiplier folds in both the standard self/cross (2-delta_ij) factor and the
            // division by the number of frames accumulated into the raw histogram counts - see
            // the class-level comment on why bead histograms accumulate across frames directly
            // rather than being averaged as independently-normalised snapshots.
            auto multiplier = (i == j ? 2.0 : 1.0) / nFrames;
            PartialSet::calculateRDF(partials_[{i, j}], fullHistograms_[{i, j}], boxVolume, beadTypePopulations[i],
                                     beadTypePopulations[j], multiplier);
            PartialSet::calculateRDF(boundPartials_[{i, j}], boundHistograms_[{i, j}], boxVolume, beadTypePopulations[i],
                                     beadTypePopulations[j], multiplier);
            PartialSet::calculateRDF(unboundPartials_[{i, j}], unboundHistograms_[{i, j}], boxVolume, beadTypePopulations[i],
                                     beadTypePopulations[j], multiplier);
        }
}

// Sum partials into totals, weighted by supplied per-bead-type concentration fractions
void BeadPartialSet::formTotals(const std::vector<double> &beadTypeFractions)
{
    auto nTypes = nBeadTypes();
    if (nTypes == 0)
    {
        total_.clear();
        boundTotal_.clear();
        unboundTotal_.clear();
        return;
    }

    total_.initialise(partials_[{0, 0}]);
    boundTotal_.initialise(partials_[{0, 0}]);
    unboundTotal_.initialise(partials_[{0, 0}]);
    std::ranges::fill(total_.values(), 0.0);
    std::ranges::fill(boundTotal_.values(), 0.0);
    std::ranges::fill(unboundTotal_.values(), 0.0);

    for (auto i = 0; i < nTypes; ++i)
        for (auto j = i; j < nTypes; ++j)
        {
            auto factor = beadTypeFractions[i] * beadTypeFractions[j] * (i == j ? 1.0 : 2.0);
            auto &full = partials_[{i, j}].values();
            auto &bound = boundPartials_[{i, j}].values();
            auto &unbound = unboundPartials_[{i, j}].values();
            for (auto n = 0; n < total_.nValues(); ++n)
            {
                total_.value(n) += factor * full[n];
                boundTotal_.value(n) += factor * bound[n];
                unboundTotal_.value(n) += factor * unbound[n];
            }
        }
}

// Sum partials into totals directly (factor=1) - see header comment
void BeadPartialSet::formTotalsPreWeighted()
{
    auto nTypes = nBeadTypes();
    if (nTypes == 0)
    {
        total_.clear();
        boundTotal_.clear();
        unboundTotal_.clear();
        return;
    }

    total_.initialise(partials_[{0, 0}]);
    boundTotal_.initialise(partials_[{0, 0}]);
    unboundTotal_.initialise(partials_[{0, 0}]);
    std::ranges::fill(total_.values(), 0.0);
    std::ranges::fill(boundTotal_.values(), 0.0);
    std::ranges::fill(unboundTotal_.values(), 0.0);

    for (auto i = 0; i < nTypes; ++i)
        for (auto j = i; j < nTypes; ++j)
        {
            auto &full = partials_[{i, j}].values();
            auto &bound = boundPartials_[{i, j}].values();
            auto &unbound = unboundPartials_[{i, j}].values();
            for (auto n = 0; n < total_.nValues(); ++n)
            {
                total_.value(n) += full[n];
                boundTotal_.value(n) += bound[n];
                unboundTotal_.value(n) += unbound[n];
            }
        }
}

const std::vector<int> &BeadPartialSet::beadTypePopulations() const { return beadTypePopulations_; }

int BeadPartialSet::totalBeadPopulation() const
{
    return std::accumulate(beadTypePopulations_.begin(), beadTypePopulations_.end(), 0);
}

/*
 * Searchers
 */

OptionalReferenceWrapper<const Data1D> BeadPartialSet::searchData1D(std::string_view tag) const
{
    auto fullIt = std::find_if(partials_.begin(), partials_.end(), [tag](const auto &data) { return data.tag() == tag; });
    if (fullIt != partials_.end())
        return *fullIt;
    auto boundIt =
        std::find_if(boundPartials_.begin(), boundPartials_.end(), [tag](const auto &data) { return data.tag() == tag; });
    if (boundIt != boundPartials_.end())
        return *boundIt;
    auto unboundIt =
        std::find_if(unboundPartials_.begin(), unboundPartials_.end(), [tag](const auto &data) { return data.tag() == tag; });
    if (unboundIt != unboundPartials_.end())
        return *unboundIt;
    if (total_.tag() == tag)
        return total_;
    if (boundTotal_.tag() == tag)
        return boundTotal_;
    if (unboundTotal_.tag() == tag)
        return unboundTotal_;
    return {};
}
