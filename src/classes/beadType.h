// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#pragma once

#include "classes/atomType.h"
#include "math/function1D.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// BeadType Definition
//
// Represents a coarse-grained "bead" pseudo-particle: a scattering centre that stands in for a
// group of real atoms (c.f. MuSSIC's bead model, docs/ai-port/MUSIC_NOTES.md section 1 "CG
// physics"). Unlike an AtomType, a BeadType has no single chemical element - its effective
// scattering length is the isotope-aware sum of its constituent AtomTypes' scattering lengths,
// and its physical extent is represented by an explicit radius + form factor rather than being a
// point scatterer (see docs/ai-port/GAP_ANALYSIS.md, "the core tension").
class BeadType
{
    public:
    explicit BeadType(std::string_view name = "");
    ~BeadType() = default;

    /*
     * Character
     */
    private:
    // Name of this bead type
    std::string name_;

    public:
    // Set name
    void setName(std::string_view name);
    // Return name
    std::string_view name() const;

    /*
     * Composition
     */
    public:
    // Constituent atom type entry: an AtomType and how many such atoms make up one bead
    struct AtomTypeCount
    {
        std::shared_ptr<AtomType> atomType;
        int count{1};
    };

    private:
    // Constituent atom types and their counts within one bead
    std::vector<AtomTypeCount> components_;

    public:
    // Add a constituent atom type and its count within the bead
    void addComponent(std::shared_ptr<AtomType> atomType, int count);
    // Return constituent atom types and counts
    const std::vector<AtomTypeCount> &components() const;

    /*
     * Scattering
     */
    private:
    // Effective (isotope-aware) coherent scattering length of the bead, fm:
    // b = sum_i(count_i * b_i), mirroring MuSSIC's n_s(r) = sum_i[N_i^s b_i]
    // (docs/ai-port/MUSIC_NOTES.md section 1), without the density-profile/form-factor part.
    double effectiveScatteringLength_{0.0};

    public:
    // Calculate, store, and return the effective scattering length from constituent atom types,
    // using the supplied per-atom-type bound coherent scattering length lookup (fm)
    double calculateEffectiveScatteringLength(const std::function<double(const std::shared_ptr<AtomType> &)> &boundCoherentFn);
    // Directly set the effective scattering length (fm), bypassing calculation (e.g. when
    // restoring from a restart file)
    void setEffectiveScatteringLength(double b);
    // Return the current effective scattering length (fm)
    double effectiveScatteringLength() const;

    /*
     * Physical Size / Form Factor
     */
    private:
    // Bead radius (Angstroms) - user-specified or otherwise derived; not computed by this class
    double radius_{0.0};
    // Form factor functional form applied to this bead type's scattering (Q-dependent damping)
    Functions1D::Form formFactor_{Functions1D::None};

    public:
    // Set bead radius
    void setRadius(double radius);
    // Return bead radius
    double radius() const;
    // Set form factor functional form
    void setFormFactor(Functions1D::Form form);
    // Return form factor functional form
    Functions1D::Form formFactor() const;
};
