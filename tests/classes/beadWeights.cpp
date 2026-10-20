// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "classes/beadWeights.h"
#include <gtest/gtest.h>

namespace UnitTest
{

// Hand-computed weighting matrix for two bead types at 50:50 concentration, using the same
// effective scattering lengths as the dodecane CE/C3 beads in BeadTypeTest
// (EffectiveScatteringLengthDodecane): b_CE = 19.938 fm, b_C3 = -2.496 fm. Expected values below
// are independently hand-derived (not re-computed via the production formula) to actually catch
// an implementation bug rather than just restate the formula twice:
//   weight(CE,CE) = 0.5*0.5*19.938*19.938*1 = 99.380961
//   weight(C3,C3) = 0.5*0.5*(-2.496)*(-2.496)*1 = 1.557504
//   weight(CE,C3) = 0.5*0.5*19.938*(-2.496)*2 = -24.882624
TEST(BeadWeightsTest, TwoBeadTypesEqualConcentration)
{
    auto ce = std::make_shared<BeadType>("CE");
    ce->setEffectiveScatteringLength(19.938);
    auto c3 = std::make_shared<BeadType>("C3");
    c3->setEffectiveScatteringLength(-2.496);

    BeadWeights weights;
    weights.addBeadType(ce, 0.5);
    weights.addBeadType(c3, 0.5);
    weights.calculateWeights();

    EXPECT_EQ(weights.nBeadTypes(), 2);
    EXPECT_NEAR(weights.weight(0, 0), 99.380961, 1.0e-6);
    EXPECT_NEAR(weights.weight(1, 1), 1.557504, 1.0e-6);
    EXPECT_NEAR(weights.weight(0, 1), -24.882624, 1.0e-6);
    // Matrix must be symmetric
    EXPECT_NEAR(weights.weight(1, 0), weights.weight(0, 1), 1.0e-12);
}

TEST(BeadWeightsTest, ClearResetsState)
{
    BeadWeights weights;
    weights.addBeadType(std::make_shared<BeadType>("X"), 1.0);
    weights.calculateWeights();
    EXPECT_EQ(weights.nBeadTypes(), 1);

    weights.clear();
    EXPECT_EQ(weights.nBeadTypes(), 0);
}

} // namespace UnitTest
