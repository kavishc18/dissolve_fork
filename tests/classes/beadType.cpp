// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "classes/beadType.h"
#include "classes/atomType.h"
#include <gtest/gtest.h>
#include <map>
#include <memory>

namespace UnitTest
{

// Hand-computed against MuSSIC's dodecane example (docs/ai-port/MUSIC_NOTES.md section 2,
// usability_tests/dodecane/{input.dat,model.dat}): bead type CE = 1x CT3 + 2x CT2, bead type
// C3 = 3x CT2 + 6x HC, with per-atom-type coherent scattering lengths (fm) CT3 = 6.646,
// CT2 = 6.646, HC = -3.739. No Fourier transform or module involved - pure arithmetic, so this
// is the fastest possible correctness signal for the bead-weighting groundwork (Slice 1).
TEST(BeadTypeTest, EffectiveScatteringLengthDodecane)
{
    auto ct3 = std::make_shared<AtomType>();
    ct3->setName("CT3");
    auto ct2 = std::make_shared<AtomType>();
    ct2->setName("CT2");
    auto hc = std::make_shared<AtomType>();
    hc->setName("HC");

    const std::map<AtomType *, double> boundCoherent = {{ct3.get(), 6.646}, {ct2.get(), 6.646}, {hc.get(), -3.739}};
    auto lookup = [&](const std::shared_ptr<AtomType> &atomType) { return boundCoherent.at(atomType.get()); };

    BeadType ce("CE");
    ce.addComponent(ct3, 1);
    ce.addComponent(ct2, 2);
    EXPECT_NEAR(ce.calculateEffectiveScatteringLength(lookup), 19.938, 1.0e-9);
    EXPECT_NEAR(ce.effectiveScatteringLength(), 19.938, 1.0e-9);

    BeadType c3("C3");
    c3.addComponent(ct2, 3);
    c3.addComponent(hc, 6);
    EXPECT_NEAR(c3.calculateEffectiveScatteringLength(lookup), -2.496, 1.0e-9);
    EXPECT_NEAR(c3.effectiveScatteringLength(), -2.496, 1.0e-9);
}

TEST(BeadTypeTest, NameRadiusAndFormFactor)
{
    BeadType bt("TestBead");
    EXPECT_EQ(bt.name(), "TestBead");

    bt.setRadius(2.5);
    EXPECT_DOUBLE_EQ(bt.radius(), 2.5);

    EXPECT_EQ(bt.formFactor(), Functions1D::None);
    bt.setFormFactor(Functions1D::Gaussian);
    EXPECT_EQ(bt.formFactor(), Functions1D::Gaussian);
}

TEST(BeadTypeTest, DirectScatteringLengthSet)
{
    BeadType bt("TestBead");
    bt.setEffectiveScatteringLength(5.0);
    EXPECT_DOUBLE_EQ(bt.effectiveScatteringLength(), 5.0);
}

} // namespace UnitTest
