// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Team Dissolve and contributors

#include "classes/beadType.h"

BeadType::BeadType(std::string_view name) : name_(name) {}

/*
 * Character
 */

void BeadType::setName(std::string_view name) { name_ = name; }

std::string_view BeadType::name() const { return name_; }

/*
 * Composition
 */

void BeadType::addComponent(std::shared_ptr<AtomType> atomType, int count)
{
    components_.push_back({std::move(atomType), count});
}

const std::vector<BeadType::AtomTypeCount> &BeadType::components() const { return components_; }

/*
 * Scattering
 */

double
BeadType::calculateEffectiveScatteringLength(const std::function<double(const std::shared_ptr<AtomType> &)> &boundCoherentFn)
{
    effectiveScatteringLength_ = 0.0;
    for (auto &component : components_)
        effectiveScatteringLength_ += component.count * boundCoherentFn(component.atomType);
    return effectiveScatteringLength_;
}

void BeadType::setEffectiveScatteringLength(double b) { effectiveScatteringLength_ = b; }

double BeadType::effectiveScatteringLength() const { return effectiveScatteringLength_; }

double BeadType::calculateSelfScatteringWeight(const std::function<double(const std::shared_ptr<AtomType> &)> &boundCoherentFn)
{
    auto sumB = 0.0, sumBSquared = 0.0;
    for (auto &component : components_)
    {
        auto b = boundCoherentFn(component.atomType);
        sumB += component.count * b;
        sumBSquared += component.count * b * b;
    }
    selfScatteringWeight_ = sumB * sumB - sumBSquared;
    return selfScatteringWeight_;
}

void BeadType::setSelfScatteringWeight(double w) { selfScatteringWeight_ = w; }

double BeadType::selfScatteringWeight() const { return selfScatteringWeight_; }

int BeadType::totalAtomCount() const
{
    auto total = 0;
    for (auto &component : components_)
        total += component.count;
    return total;
}

/*
 * Physical Size / Form Factor
 */

void BeadType::setRadius(double radius) { radius_ = radius; }

double BeadType::radius() const { return radius_; }

void BeadType::setFormFactor(Functions1D::Form form) { formFactor_ = form; }

Functions1D::Form BeadType::formFactor() const { return formFactor_; }

double BeadType::formFactorAt(double q) const
{
    if (formFactor_ == Functions1D::Form::None)
        return 1.0;
    return Function1DWrapper(formFactor_, {radius_}).y(q);
}
