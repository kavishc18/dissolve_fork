// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "main/dissolve.h"
#include "module/context.h"
#include "modules/cgGR/cgGR.h"
#include "modules/cgNeutronSQ/cgNeutronSQ.h"
#include "modules/cgSQ/cgSQ.h"

// Set target data
void CGNeutronSQModule::setTargets(const std::vector<std::unique_ptr<Configuration>> &configurations,
                                   const std::map<ModuleTypes::ModuleType, std::vector<const Module *>> &moduleMap)
{
    auto it = moduleMap.find(ModuleTypes::CGSQ);
    if (it != moduleMap.end())
        sourceCGSQ_ = dynamic_cast<const CGSQModule *>(it->second.front());
}

// Run main processing
Module::ExecutionResult CGNeutronSQModule::process(ModuleContext &moduleContext)
{
    if (!sourceCGSQ_)
    {
        Messenger::error("CGNeutronSQ: A source CGSQ module must be provided.\n");
        return ExecutionResult::Failed;
    }
    if (beadWeights_.nBeadTypes() == 0)
    {
        Messenger::error("CGNeutronSQ: No bead weights set (call setBeadWeights() before processing).\n");
        return ExecutionResult::Failed;
    }

    Messenger::print("CGNeutronSQ: Source unweighted bead-bead S(Q) will be taken from module '{}'.\n", sourceCGSQ_->name());

    beadWeights_.calculateWeights();

    auto &processingData = moduleContext.dissolve().processingModuleData();

    if (!processingData.contains("UnweightedSQ", sourceCGSQ_->name()))
    {
        Messenger::error("CGNeutronSQ: Couldn't locate source unweighted bead-bead S(Q) from module '{}'.\n",
                         sourceCGSQ_->name());
        return ExecutionResult::Failed;
    }
    const auto &unweightedsq = processingData.value<BeadPartialSet>("UnweightedSQ", sourceCGSQ_->name());

    if (unweightedsq.beadTypeNames() != beadWeights_.beadTypeNames())
    {
        Messenger::error(
            "CGNeutronSQ: Bead type names/order in the source S(Q) don't match those in the configured BeadWeights.\n");
        return ExecutionResult::Failed;
    }

    auto [weightedsq, status] = processingData.realiseIf<BeadPartialSet>("WeightedSQ", name_);
    if (status == GenericItem::ItemStatus::Created)
        weightedsq.setUpPartials(unweightedsq.beadTypeNames());

    auto currentFingerprint = std::format("{}", unweightedsq.fingerprint());
    if (weightedsq.fingerprint() == currentFingerprint)
    {
        Messenger::print("CGNeutronSQ: Weighted bead-bead S(Q) is already up-to-date.\n");
        return ExecutionResult::NotExecuted;
    }

    if (!CGNeutronSQModule::calculateWeightedSQ(unweightedsq, weightedsq, beadWeights_))
        return ExecutionResult::Failed;

    weightedsq.setFingerprint(currentFingerprint);

    return ExecutionResult::Success;
}
