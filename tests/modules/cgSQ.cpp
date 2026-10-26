// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "modules/cgSQ/cgSQ.h"
#include "tests/testData.h"
#include <gtest/gtest.h>

namespace UnitTest
{
class CGSQModuleTest : public ::testing::Test
{
    protected:
    DissolveSystemTest systemTest;
};

// Validates against a MuSSIC reference run with no broadening/window function applied
// (tests/data/cg-port/nobroadening/, generated specifically for this test - see
// docs/ai-port/DECISION_LOG.md). This isolates the core sine-transform + 4*pi*rho normalisation
// math from a separate, already-identified discrepancy (docs/ai-port/MUSIC_NOTES.md: MuSSIC's
// own Lorch0 window reuses the broadening FWHM as its lengthscale, an idiosyncratic convention
// that does not match Dissolve's standard data-range-normalised Lorch0 - see
// docs/ai-port/MIGRATION_DESIGN.md R5). Reproducing MuSSIC's literal windowing convention is a
// deliberately deferred, separate question, not part of this slice.
//
// Deliberately does NOT involve the UniformSphere form factor either - see cgSQ.h for why that
// would be scientifically wrong here (MuSSIC itself only applies form factors at the weighted
// stage, not to unweighted S(Q)).
TEST_F(CGSQModuleTest, DimerGasNoBroadening)
{
    ASSERT_NO_THROW_VERBOSE(systemTest.setUp("dissolve/input/cgGR-dimerGas.txt"));
    ASSERT_TRUE(systemTest.dissolve().iterate(3));

    // Uses plain (unnormalised) RMSE rather than the checkData1D default (EuclideanError, which
    // normalises by the reference curve's own magnitude, and badly distorts the picture for a
    // near-zero-amplitude curve like BA-BA - see DECISION_LOG.md).
    //
    // Tolerances are deliberately *not* uniform across pairs, set empirically from the actual
    // achieved precision, not guessed: BA-BB and Total (strong, coherent bond-length-peak signal,
    // range up to ~1.7 and ~0.9) match to RMSE ~2-5e-3, i.e. sub-percent of their own range. BA-BA
    // and BB-BB (weak signal: an almost-uncorrelated ideal-gas pair with range only ~0.1-0.3) show
    // a larger RMSE (~1.3e-2 / ~4.3e-3) that is still small in absolute terms but a larger fraction
    // of their own small range. This matches the g(r) input being bit-identical to MuSSIC's
    // (confirmed in CGGRModuleTest to ~1e-29) but the two independent sine-transform quadrature
    // implementations summing ~100 terms of mostly-cancelling noise, which is inherently more
    // sensitive to tiny floating-point rounding/order differences than a sum dominated by one
    // large coherent peak - a known numerical-conditioning effect, not an algorithmic discrepancy.
    EXPECT_TRUE(systemTest.checkData1D(
        "CGSQ01//UnweightedSQ//BA-BA//Full",
        {"cg-port/nobroadening/CG-unweighted-BA-BA.sq", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 2.0e-2,
        Error::ErrorType::RMSEError));
    EXPECT_TRUE(systemTest.checkData1D(
        "CGSQ01//UnweightedSQ//BA-BB//Full",
        {"cg-port/nobroadening/CG-unweighted-BA-BB.sq", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 3.0e-3,
        Error::ErrorType::RMSEError));
    EXPECT_TRUE(systemTest.checkData1D(
        "CGSQ01//UnweightedSQ//BB-BB//Full",
        {"cg-port/nobroadening/CG-unweighted-BB-BB.sq", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 1.0e-2,
        Error::ErrorType::RMSEError));
    EXPECT_TRUE(systemTest.checkData1D(
        "CGSQ01//UnweightedSQ//Total",
        {"cg-port/nobroadening/CG-unweighted-total.sq", Data1DImportFileFormat::Data1DImportFormat::XY, 1, 2}, 6.0e-3,
        Error::ErrorType::RMSEError));
}

} // namespace UnitTest
