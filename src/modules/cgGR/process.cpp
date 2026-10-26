// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "analyser/siteSelector.h"
#include "classes/beadPartialSet.h"
#include "classes/box.h"
#include "classes/configuration.h"
#include "classes/molecule.h"
#include "classes/site.h"
#include "classes/speciesSite.h"
#include "main/dissolve.h"
#include "module/context.h"
#include "modules/cgGR/cgGR.h"
#include "templates/algorithms.h"
#include <map>

// Run main processing
Module::ExecutionResult CGGRModule::process(ModuleContext &moduleContext)
{
    if (!targetConfiguration_)
    {
        Messenger::error("CGGR: No target Configuration set.\n");
        return ExecutionResult::Failed;
    }
    if (beadSites_.empty())
    {
        Messenger::error("CGGR: No bead sites defined.\n");
        return ExecutionResult::Failed;
    }

    auto *cfg = targetConfiguration_;
    const auto *box = cfg->box();

    // Select realised bead-site instances for this frame
    SiteSelector selector(cfg, beadSites_);
    const auto &sites = selector.sites();
    if (sites.empty())
    {
        Messenger::error("CGGR: No bead site instances found in Configuration '{}'.\n", cfg->name());
        return ExecutionResult::Failed;
    }

    // Group sites by the name of their defining SpeciesSite (= bead-type identity), preserving
    // first-seen order so the bead-type index ordering is stable across frames
    std::vector<std::string> beadTypeNames;
    std::map<std::string, int> typeIndexOf;
    std::vector<int> siteTypeIndex(sites.size());
    std::vector<const Site *> siteObjects(sites.size());
    for (std::size_t n = 0; n < sites.size(); ++n)
    {
        const auto &[site, index] = sites[n];
        siteObjects[n] = site;
        auto name = std::string(site->parent()->name());
        auto it = typeIndexOf.find(name);
        if (it == typeIndexOf.end())
        {
            auto newIndex = static_cast<int>(beadTypeNames.size());
            typeIndexOf.emplace(name, newIndex);
            beadTypeNames.push_back(name);
            siteTypeIndex[n] = newIndex;
        }
        else
            siteTypeIndex[n] = it->second;
    }

    std::vector<int> beadTypePopulations(beadTypeNames.size(), 0);
    for (auto typeIndex : siteTypeIndex)
        ++beadTypePopulations[typeIndex];

    // Determine r range (mirrors GRModule::process - half-cell range unless overridden)
    auto rdfRange = box->inscribedSphereRadius();
    if (requestedRange_)
        rdfRange = std::min(requestedRange_.value(), rdfRange);
    rdfRange = int(rdfRange / binWidth_) * binWidth_;

    // Retrieve or create the persistent BeadPartialSet for this Configuration
    auto &processingData = moduleContext.dissolve().processingModuleData();
    auto [beadPartials, status] =
        processingData.realiseIf<BeadPartialSet>(std::format("{}//OriginalGR", cfg->name()), name_);
    auto beadTypesChanged = beadPartials.beadTypeNames() != beadTypeNames;
    if (status == GenericItem::ItemStatus::Created || beadTypesChanged)
        beadPartials.setUp(beadTypeNames, rdfRange, binWidth_);

    // Has this frame already been accumulated? (mirrors GRModule's fingerprint-based up-to-date check,
    // but here the fingerprint marks the last *accumulated* frame, not a reset point - see class
    // comment on BeadPartialSet for why frames accumulate rather than reset-and-recompute)
    auto currentFingerprint = std::format("{}", cfg->contentsVersion());
    if (beadPartials.fingerprint() == currentFingerprint)
    {
        Messenger::print("CGGR: Bead partials for Configuration '{}' are already up-to-date for this frame.\n", cfg->name());
        return ExecutionResult::NotExecuted;
    }

    // Bin all bead-bead pairs for this frame directly into the running (cross-frame) histograms.
    // Deliberate scope cut (see cgGR.h / DECISION_LOG.md): simple double loop only, no cell-list
    // acceleration - bead counts are expected to be far smaller than atom counts.
    const auto nSites = siteObjects.size();
    for (std::size_t i = 0; i < nSites; ++i)
    {
        auto typeI = siteTypeIndex[i];
        const auto &ri = siteObjects[i]->origin();
        for (std::size_t j = i + 1; j < nSites; ++j)
        {
            auto typeJ = siteTypeIndex[j];
            auto distance = box->minimumDistance(ri, siteObjects[j]->origin());

            auto &full = (typeI <= typeJ) ? beadPartials.fullHistogram(typeI, typeJ) : beadPartials.fullHistogram(typeJ, typeI);
            full.bin(distance);

            if (siteObjects[i]->molecule() == siteObjects[j]->molecule())
            {
                auto &bound =
                    (typeI <= typeJ) ? beadPartials.boundHistogram(typeI, typeJ) : beadPartials.boundHistogram(typeJ, typeI);
                bound.bin(distance);
            }
        }
    }
    beadPartials.incrementFrameCount();

    // Derive unbound histograms as full - bound (mirrors GRModule's approach)
    auto nTypes = static_cast<int>(beadTypeNames.size());
    for (auto i = 0; i < nTypes; ++i)
        for (auto j = i; j < nTypes; ++j)
        {
            beadPartials.unboundHistogram(i, j) = beadPartials.fullHistogram(i, j);
            beadPartials.unboundHistogram(i, j).add(beadPartials.boundHistogram(i, j), -1);
        }

    // Normalise into g(r) using all frames accumulated so far
    beadPartials.formPartials(box->volume(), beadTypePopulations);

    std::vector<double> beadTypeFractions(nTypes);
    auto totalSites = static_cast<double>(nSites);
    for (auto i = 0; i < nTypes; ++i)
        beadTypeFractions[i] = beadTypePopulations[i] / totalSites;
    beadPartials.formTotals(beadTypeFractions);

    beadPartials.setFingerprint(currentFingerprint);

    Messenger::print("CGGR: Accumulated frame {} for Configuration '{}' ({} bead types, {} bead instances).\n",
                     beadPartials.nFramesAccumulated(), cfg->name(), nTypes, nSites);

    return ExecutionResult::Success;
}
