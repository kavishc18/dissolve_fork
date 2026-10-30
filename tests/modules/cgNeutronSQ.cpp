// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "classes/atomType.h"
#include "classes/beadWeights.h"
#include "modules/cgNeutronSQ/cgNeutronSQ.h"
#include "tests/testData.h"
#include <gtest/gtest.h>
#include <map>
#include <memory>

namespace UnitTest
{
class CGNeutronSQModuleTest : public ::testing::Test
{
    protected:
    DissolveSystemTest systemTest;
};

// Validates against a dedicated MuSSIC reference run with form_factor: no_FF, broadening: none,
// and windowfn: none (tests/music-reference/cg_dimer_gas_noff_clean/ - generated specifically for
// this slice, see docs/ai-port/DECISION_LOG.md) - this is the headline validation target for the
// whole port: MuSSIC's weighted total F_CG(Q), the one CG output confirmed end-to-end functional
// (docs/ai-port/MUSIC_NOTES.md D1 only affects the weighted-total *.gr files, not *.sq).
//
// Uses no_FF (point beads, no form factor) and no broadening/windowing deliberately, to isolate
// the actual weighting formula - self/single-bead term + cross-bead term + avg_noa_bead
// normalisation, all verified by hand against MuSSIC's neutronsq_cg.F source directly - from the
// separate, already-identified Gaussian-parameterisation and Lorch0-convention mismatches (see
// cgSQ.h and DECISION_LOG.md). Reconciling those is deferred, explicit future work, not part of
// this slice's validation.
TEST_F(CGNeutronSQModuleTest, DimerGasNoFF)
{
    ASSERT_NO_THROW_VERBOSE(systemTest.setUp("dissolve/input/cgNeutronSQ-dimerGas.txt"));

    // Bead composition and scattering lengths from tests/music-reference/cg_dimer_gas_noff_clean/
    // input.dat: AT1 b=5.0 fm, AT2 b=3.0 fm; BA = 3xAT1, BB = 2xAT1 + 4xAT2. Scattering lengths
    // are given in Dissolve's internal convention (raw fm x 0.1), matching how NeutronWeights
    // itself applies this factor (src/classes/neutronWeights.cpp) - confirmed empirically here by
    // reproducing MuSSIC's own self-scattering output column exactly only with this factor
    // applied (see DECISION_LOG.md Slice 4 entry).
    auto at1 = std::make_shared<AtomType>();
    at1->setName("AT1");
    auto at2 = std::make_shared<AtomType>();
    at2->setName("AT2");
    const std::map<AtomType *, double> boundCoherent = {{at1.get(), 0.5}, {at2.get(), 0.3}};
    auto lookup = [&](const std::shared_ptr<AtomType> &atomType) { return boundCoherent.at(atomType.get()); };

    auto ba = std::make_shared<BeadType>("BA");
    ba->addComponent(at1, 3);
    ba->calculateEffectiveScatteringLength(lookup);
    ba->calculateSelfScatteringWeight(lookup);
    EXPECT_NEAR(ba->effectiveScatteringLength(), 1.5, 1.0e-10);
    EXPECT_NEAR(ba->selfScatteringWeight(), 1.5, 1.0e-10); // 1.5^2 - 3*0.5^2 = 2.25 - 0.75 = 1.5

    auto bb = std::make_shared<BeadType>("BB");
    bb->addComponent(at1, 2);
    bb->addComponent(at2, 4);
    bb->calculateEffectiveScatteringLength(lookup);
    bb->calculateSelfScatteringWeight(lookup);
    EXPECT_NEAR(bb->effectiveScatteringLength(), 2.2, 1.0e-10);
    EXPECT_NEAR(bb->selfScatteringWeight(), 3.98, 1.0e-10); // 2.2^2 - (2*0.5^2+4*0.3^2) = 4.84-0.86

    BeadWeights weights;
    weights.addBeadType(ba, 0.5);
    weights.addBeadType(bb, 0.5);
    weights.calculateWeights();

    // Hand-computed: avg_noa_bead = 0.5*3 + 0.5*6 = 4.5; self term = (0.5*1.5 + 0.5*3.98)/4.5
    EXPECT_NEAR(weights.averageAtomsPerBead(), 4.5, 1.0e-10);

    auto *cgNeutronSQModule = systemTest.getModule<CGNeutronSQModule>("CGNeutronSQ01");
    cgNeutronSQModule->setBeadWeights(weights);

    ASSERT_TRUE(systemTest.dissolve().iterate(3));

    // Weighted total F_CG(Q) ranges ~0.43-1.26 over this Q range; RMSE below is well under 1% of
    // that range - same empirical-calibration approach as CGSQModuleTest (see DECISION_LOG.md).
    EXPECT_TRUE(systemTest.checkData1D(
        "CGNeutronSQ01//WeightedSQ//Total",
        {"cg-port/noff_clean/H-CG-weighted-total.sq", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 5.0e-3,
        Error::ErrorType::RMSEError));
}

} // namespace UnitTest
