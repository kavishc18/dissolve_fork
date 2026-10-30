// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#pragma once

#include "classes/beadType.h"
#include "templates/array2D.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

// BeadWeights
//
// Bead-type-pair neutron scattering weighting matrix, analogous to NeutronWeights
// (src/classes/neutronWeights.h) but materially simpler: bead effective scattering lengths are
// precomputed per BeadType (see BeadType::calculateEffectiveScatteringLength), so this class only
// needs to combine them with bead-type concentrations into the Faber-Ziman-style pair weight
// c_i*c_j*b_i*b_j*(2-delta_ij) (c.f. MuSSIC's weight1(i,j), docs/ai-port/MUSIC_NOTES.md section 1).
class BeadWeights
{
    public:
    BeadWeights() = default;
    ~BeadWeights() = default;

    /*
     * Construction
     */
    private:
    // Bead types and their fractional concentrations
    std::vector<std::pair<std::shared_ptr<BeadType>, double>> beadTypes_;

    public:
    // Clear contents
    void clear();
    // Add a bead type with its fractional concentration
    void addBeadType(std::shared_ptr<BeadType> beadType, double fraction);
    // Return number of bead types
    int nBeadTypes() const;
    // Return the i-th bead type
    const std::shared_ptr<BeadType> &beadType(int i) const;
    // Return the i-th bead type's fractional concentration
    double fraction(int i) const;
    // Return the names of all bead types, in order
    std::vector<std::string> beadTypeNames() const;
    // Return the concentration-weighted average number of atoms per bead (stands in for Soper's
    // N_A normalisation in MuSSIC's CG cross-section formula - see
    // docs/ai-port/MUSIC_NOTES.md section 1 "CG physics", avg_noa_bead in neutronsq_cg.F)
    double averageAtomsPerBead() const;

    /*
     * Weights
     */
    private:
    // Full pair weighting matrix: weight(i,j) = c_i * c_j * b_i * b_j * (2 - delta_ij)
    Array2D<double> weights_;

    public:
    // (Re)calculate the weighting matrix from current bead type / concentration data
    void calculateWeights();
    // Return full weighting for bead types i and j
    double weight(int i, int j) const;
    // Return full weights matrix
    const Array2D<double> &weights() const;
};
