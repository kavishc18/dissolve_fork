// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#pragma once

#include "math/data1D.h"
#include "math/histogram1D.h"
#include "templates/array2D.h"
#include "templates/optionalRef.h"
#include <optional>
#include <string>
#include <vector>

// Set of bead-bead partials (g(r)-style), analogous to PartialSet but keyed by coarse-grained
// bead-type name instead of AtomTypeMix. See docs/ai-port/GAP_ANALYSIS.md ("the core tension")
// and docs/ai-port/MIGRATION_DESIGN.md (Increment 2) for why this is a parallel class rather than
// a reuse of PartialSet: bead types have no backing AtomType/element, so PartialSet's hard
// dependency on AtomTypeMix doesn't fit.
//
// Unlike PartialSet (which recomputes a fresh single-frame snapshot each time and relies on a
// separate historical-averaging step for multi-frame statistics), BeadPartialSet accumulates raw
// histogram bin counts across every frame it is given before normalising once - this mirrors
// MuSSIC's own normalisation approach (divide by total frame count once at the end,
// form_partial.F) rather than Dissolve's GRModule scheme of averaging N independently-normalised
// snapshots. This is a deliberate, logged difference, not an oversight (DECISION_LOG.md).
class BeadPartialSet
{
    public:
    BeadPartialSet() = default;
    ~BeadPartialSet() = default;

    /*
     * Bead Types
     */
    private:
    // Ordered list of distinct bead-type names used to index all matrices below
    std::vector<std::string> beadTypeNames_;
    // Fingerprint for these partials (e.g. reflecting Configuration contentsVersion at last accumulation)
    std::string fingerprint_{"NO_FINGERPRINT"};
    // Number of frames accumulated into the current histogram data
    int nFramesAccumulated_{0};

    public:
    // Set up partial/histogram arrays for the supplied (ordered) list of bead-type names
    void setUp(std::vector<std::string> beadTypeNames, double rdfRange, double binWidth);
    // Set up partial arrays only (no histograms) - for partial sets derived from another
    // BeadPartialSet by transformation (e.g. g(r) -> S(Q)) rather than by direct binning
    void setUpPartials(std::vector<std::string> beadTypeNames);
    // Reset histograms and accumulated frame count, retaining bead-type list
    void reset();
    // Return number of bead types used to generate matrices
    int nBeadTypes() const;
    // Return bead-type names
    const std::vector<std::string> &beadTypeNames() const;
    // Return index of named bead type, if present
    std::optional<int> indexOf(std::string_view beadTypeName) const;
    // Set new fingerprint
    void setFingerprint(std::string_view fingerprint);
    // Return fingerprint of partials
    std::string_view fingerprint() const;
    // Increment the accumulated frame counter (call once per frame binned)
    void incrementFrameCount();
    // Return number of frames accumulated so far
    int nFramesAccumulated() const;

    /*
     * Histograms
     */
    private:
    // Histograms for full (all pairs) bead-bead distances
    Array2D<Histogram1D> fullHistograms_;
    // Histograms for bound (intramolecular, same-molecule) bead-bead distances
    Array2D<Histogram1D> boundHistograms_;
    // Histograms for unbound (intermolecular, different-molecule) bead-bead distances
    Array2D<Histogram1D> unboundHistograms_;

    public:
    // Return full histogram specified
    Histogram1D &fullHistogram(int i, int j);
    // Return bound histogram specified
    Histogram1D &boundHistogram(int i, int j);
    // Return unbound histogram specified
    Histogram1D &unboundHistogram(int i, int j);

    /*
     * Partials
     */
    private:
    // Full bead-bead partial g(r)
    Array2D<Data1D> partials_;
    // Bound (intramolecular) bead-bead partial g(r)
    Array2D<Data1D> boundPartials_;
    // Unbound (intermolecular) bead-bead partial g(r)
    Array2D<Data1D> unboundPartials_;
    // Total functions
    Data1D total_, boundTotal_, unboundTotal_;

    public:
    // Return full bead-bead partial specified
    Data1D &partial(int i, int j);
    const Data1D &partial(int i, int j) const;
    // Return bead-bead partial for unbound (intermolecular) pairs
    Data1D &unboundPartial(int i, int j);
    const Data1D &unboundPartial(int i, int j) const;
    // Return bead-bead partial for bound (intramolecular) pairs
    Data1D &boundPartial(int i, int j);
    const Data1D &boundPartial(int i, int j) const;
    // Return total function
    Data1D &total();
    const Data1D &total() const;
    // Return total bound function
    Data1D &boundTotal();
    const Data1D &boundTotal() const;
    // Return total unbound function
    Data1D &unboundTotal();
    const Data1D &unboundTotal() const;

    /*
     * Manipulation
     */
    public:
    // Form partials (g(r)) from accumulated histogram data, normalising by the number of frames
    // accumulated so far and the given per-bead-type population (number of bead instances of that
    // type in the Configuration)
    void formPartials(double boxVolume, const std::vector<int> &beadTypePopulations);
    // Sum partials into totals, weighted by the supplied per-bead-type concentration fractions
    void formTotals(const std::vector<double> &beadTypeFractions);
    // Sum partials into totals directly (factor=1 for every stored i<=j entry), for use when the
    // partials already have concentration/weighting factors baked in (e.g. CGNeutronSQModule,
    // which pre-applies BeadWeights::weight(i,j) - itself already c_i*c_j*(2-delta_ij)-weighted -
    // to each partial before calling this). Mirrors PartialSet::formTotals(false) exactly.
    void formTotalsPreWeighted();
    // Return per-bead-type populations recorded at the last formPartials() call
    const std::vector<int> &beadTypePopulations() const;
    // Return total bead population (sum over all types) recorded at the last formPartials() call
    int totalBeadPopulation() const;

    private:
    // Per-bead-type populations, recorded by the last formPartials() call
    std::vector<int> beadTypePopulations_;

    /*
     * Searchers
     */
    public:
    // Return Data1D with specified tag, if it exists (supports the GenericList "//"-path lookup
    // convention used by DissolveSystemTest::checkData1D - see src/items/searchers.cpp)
    OptionalReferenceWrapper<const Data1D> searchData1D(std::string_view tag) const;
};
