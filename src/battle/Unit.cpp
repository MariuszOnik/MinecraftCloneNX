#include "battle/Unit.hpp"

#include <cmath>

namespace voxelgame::battle {

namespace {
constexpr float kHalfPi = 1.57079632679489661923F;
}

float FacingYaw(const Facing facing) noexcept {
    // Model yaw such that "front" points along the facing direction. North (-Z)
    // is 0; matches how the renderer rotates the humanoid.
    switch (facing) {
        case Facing::North:
            return 0.0F;
        case Facing::East:
            return kHalfPi;
        case Facing::South:
            return 2.0F * kHalfPi;
        case Facing::West:
            return 3.0F * kHalfPi;
    }
    return 0.0F;
}

Facing FacingTowards(const int fromX, const int fromZ, const int toX, const int toZ) noexcept {
    const int dx = toX - fromX;
    const int dz = toZ - fromZ;
    if (std::abs(dx) >= std::abs(dz)) {
        return dx >= 0 ? Facing::East : Facing::West;
    }
    return dz >= 0 ? Facing::South : Facing::North;
}

UnitHandle UnitRegistry::Spawn(const Unit& prototype) {
    int index;
    if (!freeList_.empty()) {
        index = freeList_.back();
        freeList_.pop_back();
    } else {
        index = static_cast<int>(slots_.size());
        slots_.emplace_back();
    }

    Slot& slot = slots_[static_cast<std::size_t>(index)];
    slot.unit = prototype;
    slot.alive = true;
    ++aliveCount_;
    return UnitHandle{index, slot.generation};
}

void UnitRegistry::Remove(const UnitHandle handle) {
    if (!IndexValid(handle)) {
        return;
    }
    Slot& slot = slots_[static_cast<std::size_t>(handle.index)];
    slot.alive = false;
    ++slot.generation;  // invalidates every outstanding handle to this slot
    --aliveCount_;
    freeList_.push_back(handle.index);
}

bool UnitRegistry::IndexValid(const UnitHandle handle) const noexcept {
    return handle.index >= 0 && static_cast<std::size_t>(handle.index) < slots_.size() &&
           slots_[static_cast<std::size_t>(handle.index)].generation == handle.generation;
}

bool UnitRegistry::Alive(const UnitHandle handle) const noexcept {
    return IndexValid(handle) && slots_[static_cast<std::size_t>(handle.index)].alive;
}

Unit* UnitRegistry::Get(const UnitHandle handle) noexcept {
    return Alive(handle) ? &slots_[static_cast<std::size_t>(handle.index)].unit : nullptr;
}

const Unit* UnitRegistry::Get(const UnitHandle handle) const noexcept {
    return Alive(handle) ? &slots_[static_cast<std::size_t>(handle.index)].unit : nullptr;
}

Unit* UnitRegistry::AtIndex(const int index) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= slots_.size() ||
        !slots_[static_cast<std::size_t>(index)].alive) {
        return nullptr;
    }
    return &slots_[static_cast<std::size_t>(index)].unit;
}

const Unit* UnitRegistry::AtIndex(const int index) const noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= slots_.size() ||
        !slots_[static_cast<std::size_t>(index)].alive) {
        return nullptr;
    }
    return &slots_[static_cast<std::size_t>(index)].unit;
}

UnitHandle UnitRegistry::HandleAt(const int index) const noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= slots_.size() ||
        !slots_[static_cast<std::size_t>(index)].alive) {
        return UnitHandle{};
    }
    return UnitHandle{index, slots_[static_cast<std::size_t>(index)].generation};
}

std::size_t UnitRegistry::TeamCount(const int team) const noexcept {
    std::size_t count = 0;
    for (const Slot& slot : slots_) {
        if (slot.alive && slot.unit.team == team) {
            ++count;
        }
    }
    return count;
}

void UnitRegistry::ForEach(const std::function<void(UnitHandle, Unit&)>& fn) {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].alive) {
            fn(UnitHandle{static_cast<int>(i), slots_[i].generation}, slots_[i].unit);
        }
    }
}

void UnitRegistry::ForEach(const std::function<void(UnitHandle, const Unit&)>& fn) const {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].alive) {
            fn(UnitHandle{static_cast<int>(i), slots_[i].generation}, slots_[i].unit);
        }
    }
}

}  // namespace voxelgame::battle
