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
class CGAtomisticMappingTest : public ::testing::Test
{
    protected:
    DissolveSystemTest systemTest;
};

// Slice 5 (stretch) - on-the-fly atomistic->CG mapping, MuSSIC's
// "compare_CG_atomistic" run type. Validates against a dedicated MuSSIC
// reference run (tests/music-reference/cg_atomistic_mapping/) on a genuine
// multi-atom-per-bead synthetic atomistic trajectory (200 molecules x 9 real
// atoms, mapped into 2 beads of 3 and 6 atoms respectively).
//
// The headline finding of this slice: CGGRModule, CGSQModule and
// CGNeutronSQModule required ZERO code changes to support this case. They
// were already built on top of Dissolve's existing SpeciesSite mechanism
// (see docs/ai-port/GAP_ANALYSIS.md Option C), which natively computes the
// (unweighted) average position of an arbitrary number of constituent atoms
// every frame - exactly what "on-the-fly CG mapping" means. The only new
// thing needed for this slice was a multi-atom Site definition and a real
// atomistic trajectory to feed it (see
// tests/data/dissolve/input/cgAtomisticMapping.txt) - not new module code.
// See docs/ai-port/DECISION_LOG.md Slice 5 entry for the full writeup.
TEST_F(CGAtomisticMappingTest, DimerGasFromAtoms)
{
    ASSERT_NO_THROW_VERBOSE(systemTest.setUp("dissolve/input/cgAtomisticMapping.txt"));

    // Same bead composition/scattering lengths as Slice 4's dimer-gas test
    // (BA = 3xAT1, BB = 2xAT1 + 4xAT2, b=0.5/0.3 in Dissolve's internal
    // scattering-length convention) - see tests/music-reference/
    // cg_atomistic_mapping/input.dat.
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

    auto bb = std::make_shared<BeadType>("BB");
    bb->addComponent(at1, 2);
    bb->addComponent(at2, 4);
    bb->calculateEffectiveScatteringLength(lookup);
    bb->calculateSelfScatteringWeight(lookup);

    BeadWeights weights;
    weights.addBeadType(ba, 0.5);
    weights.addBeadType(bb, 0.5);
    weights.calculateWeights();

    auto *cgNeutronSQModule = systemTest.getModule<CGNeutronSQModule>("CGNeutronSQ01");
    cgNeutronSQModule->setBeadWeights(weights);

    ASSERT_TRUE(systemTest.dissolve().iterate(3));

    // Bead-bead g(r) (Slice 2 level) - on-the-fly mapped bead positions vs
    // MuSSIC's own compare_CG_atomistic-derived CG trajectory. Matches to
    // ~1e-15 (floating-point noise floor) - the same precision as the
    // pre-made single-atom-per-bead case in Slice 2, confirming the on-the-fly
    // multi-atom averaging is exact, not an approximation.
    EXPECT_TRUE(systemTest.checkData1D(
        "CGGR01//Bulk//OriginalGR//BA-BA//Full",
        {"cg-port/atomistic_mapping/CG-unweighted-BA-BA.gr", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 1.0e-9,
        Error::ErrorType::RMSEError));
    EXPECT_TRUE(systemTest.checkData1D(
        "CGGR01//Bulk//OriginalGR//BA-BB//Full",
        {"cg-port/atomistic_mapping/CG-unweighted-BA-BB.gr", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 1.0e-9,
        Error::ErrorType::RMSEError));
    EXPECT_TRUE(systemTest.checkData1D(
        "CGGR01//Bulk//OriginalGR//BB-BB//Full",
        {"cg-port/atomistic_mapping/CG-unweighted-BB-BB.gr", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 1.0e-9,
        Error::ErrorType::RMSEError));

    // Bead-bead unweighted S(Q) (Slice 3 level)
    EXPECT_TRUE(systemTest.checkData1D(
        "CGSQ01//UnweightedSQ//Total",
        {"cg-port/atomistic_mapping/CG-unweighted-total.sq", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 5.0e-3,
        Error::ErrorType::RMSEError));

    // Weighted total F_CG(Q) (Slice 4 level - the headline quantity)
    EXPECT_TRUE(systemTest.checkData1D(
        "CGNeutronSQ01//WeightedSQ//Total",
        {"cg-port/atomistic_mapping/H-CG-weighted-total.sq", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 4.0e-3,
        Error::ErrorType::RMSEError));
}

} // namespace UnitTest
