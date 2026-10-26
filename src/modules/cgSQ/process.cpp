// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "classes/box.h"
#include "classes/configuration.h"
#include "main/dissolve.h"
#include "module/context.h"
#include "modules/cgGR/cgGR.h"
#include "modules/cgSQ/cgSQ.h"

// Set target data
void CGSQModule::setTargets(const std::vector<std::unique_ptr<Configuration>> &configurations,
                            const std::map<ModuleTypes::ModuleType, std::vector<const Module *>> &moduleMap)
{
    auto it = moduleMap.find(ModuleTypes::CGGR);
    if (it != moduleMap.end())
        sourceCGGR_ = dynamic_cast<const CGGRModule *>(it->second.front());
}

// Run main processing
Module::ExecutionResult CGSQModule::process(ModuleContext &moduleContext)
{
    if (!sourceCGGR_)
    {
        Messenger::error("CGSQ: A source CGGR module must be provided.\n");
        return ExecutionResult::Failed;
    }

    auto *cfg = sourceCGGR_->targetConfiguration();
    if (!cfg)
    {
        Messenger::error("CGSQ: Source CGGR module '{}' has no target Configuration set.\n", sourceCGGR_->name());
        return ExecutionResult::Failed;
    }

    Messenger::print("CGSQ: Calculating bead-bead S(Q) over {} < Q < {} Angstroms**-1 using step size of {} Angstroms**-1.\n",
                     qMin_, qMax_, qDelta_);
    Messenger::print("CGSQ: Source bead-bead g(r) will be taken from module '{}'.\n", sourceCGGR_->name());

    auto &processingData = moduleContext.dissolve().processingModuleData();

    if (!processingData.contains(std::format("{}//OriginalGR", cfg->name()), sourceCGGR_->name()))
    {
        Messenger::error("CGSQ: Couldn't locate source bead-bead g(r) from module '{}'.\n", sourceCGGR_->name());
        return ExecutionResult::Failed;
    }
    const auto &unweightedgr =
        processingData.value<BeadPartialSet>(std::format("{}//OriginalGR", cfg->name()), sourceCGGR_->name());

    auto rho = unweightedgr.totalBeadPopulation() / cfg->box()->volume();

    auto [unweightedsq, status] = processingData.realiseIf<BeadPartialSet>("UnweightedSQ", name_);
    if (status == GenericItem::ItemStatus::Created)
        unweightedsq.setUpPartials(unweightedgr.beadTypeNames());

    auto currentFingerprint = std::format("{}", unweightedgr.fingerprint());
    if (unweightedsq.fingerprint() == currentFingerprint)
    {
        Messenger::print("CGSQ: Unweighted bead-bead S(Q) is already up-to-date.\n");
        return ExecutionResult::NotExecuted;
    }

    if (!CGSQModule::calculateUnweightedSQ(unweightedgr, unweightedsq, qMin_, qDelta_, qMax_, rho,
                                           WindowFunction(windowFunction_), qBroadening_))
        return ExecutionResult::Failed;

    unweightedsq.setFingerprint(currentFingerprint);

    return ExecutionResult::Success;
}
