/**
 * @file rotation_groups.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The rotation groups: which loads take turns, checked and resolved at compile time
 * @version 0.1
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2026-2026
 *
 * @details rotationGroups (config.h) lists, group by group, the loads that take turns at the top
 *          of the priority list: one group per appliance, such as the elements of a 3-phase water
 *          heater. The groups are written with target codes (config_types.h); this file turns
 *          them into load numbers, checks them for validation.h, and builds Rotation::Table: the
 *          priority places each group holds. Rotation only reorders the loads of a group among
 *          their places, so those places never change and are known at compile time.
 *
 *          No Arduino here: the native tests build it on the PC.
 */

#ifndef ROTATION_GROUPS_H
#define ROTATION_GROUPS_H

#include <stdint.h>

#include "config_types.h"
#include "load_map.h"

namespace Rotation
{
/** @brief Number of bits set in @p mask. */
constexpr uint8_t countLoads(uint32_t mask)
{
  uint8_t count{ 0 };
  for (; mask; mask &= mask - 1)
  {
    ++count;
  }
  return count;
}

/**
 * @brief The loads a target code stands for, by load number.
 *
 * @details LOAD(n) is load n of the load map, LOCAL_LOAD(n) the same if it is local,
 *          REMOTE_LOAD(n) the n-th remote load in map order, a plain number the local load on
 *          that pin, ALL_LOCAL_LOADS(), ALL_REMOTE_LOADS() and ALL_LOADS() their loads.
 *
 * @param code a target code, as written in rotationGroups
 * @param loadMap the load map (physicalLoadPin)
 * @return bit n set for each load n the code stands for; 0 if it stands for no load (a relay,
 *         a load number beyond the map, a pin with no local load on it)
 */
template< uint8_t NL >
constexpr uint32_t loadsOf(const uint8_t code, const uint8_t (&loadMap)[NL])
{
  static_assert(NL <= 32, "the load numbers of the rotation groups fit 32 bits");

  if (code >= TargetCode::LOAD && code < TargetCode::LOAD + TargetCode::MAX_LOADS)
  {
    const uint8_t loadNum = code - TargetCode::LOAD;
    return loadNum < NL ? (1UL << loadNum) : 0;
  }
  if (code >= TargetCode::LOCAL_LOAD && code < TargetCode::LOCAL_LOAD + TargetCode::MAX_LOCAL_LOADS)
  {
    const uint8_t loadNum = code - TargetCode::LOCAL_LOAD;
    return loadNum < NL && Load::isLocal(loadMap[loadNum]) ? (1UL << loadNum) : 0;
  }
  if (code >= TargetCode::RELAY && code < TargetCode::RELAY + TargetCode::MAX_RELAYS)
  {
    return 0;
  }

  uint32_t loads{ 0 };
  if (code == TargetCode::ALL_LOCAL_LOADS || code == TargetCode::ALL_REMOTE_LOADS || code == TargetCode::ALL_LOADS)
  {
    for (uint8_t i = 0; i != NL; ++i)
    {
      const bool local = Load::isLocal(loadMap[i]);
      if (code == TargetCode::ALL_LOADS || local == (code == TargetCode::ALL_LOCAL_LOADS))
      {
        loads |= 1UL << i;
      }
    }
    return loads;
  }
  if (code >= TargetCode::ALL_LOCAL_LOADS)
  {
    return 0;  // ALL_RELAYS(), ALL_LOADS_AND_RELAYS()
  }

  if (code >= REMOTE_PIN_BASE)
  {
    uint8_t ordinal = code - REMOTE_PIN_BASE;
    for (uint8_t i = 0; i != NL; ++i)
    {
      if (!Load::isLocal(loadMap[i]) && ordinal-- == 0)
      {
        return 1UL << i;
      }
    }
    return 0;
  }

  for (uint8_t i = 0; i != NL; ++i)
  {
    if (Load::isLocal(loadMap[i]) && Load::pinOf(loadMap[i]) == code)
    {
      return 1UL << i;
    }
  }
  return 0;
}

/** @brief The loads of group @p g, all its codes together. */
template< uint8_t NG, uint8_t MaxPins, uint8_t NL >
constexpr uint32_t groupLoads(const RotationGroups< NG, MaxPins >& groups, const uint8_t g, const uint8_t (&loadMap)[NL])
{
  uint32_t loads{ 0 };
  const auto& targets = groups.getGroup(g);
  for (uint8_t i = 0; i != targets.count; ++i)
  {
    loads |= loadsOf(targets.pins[i], loadMap);
  }
  return loads;
}

/** @brief Every code of every group stands for at least one load. */
template< uint8_t NG, uint8_t MaxPins, uint8_t NL >
constexpr bool targetsAreLoads(const RotationGroups< NG, MaxPins >& groups, const uint8_t (&loadMap)[NL])
{
  for (uint8_t g = 0; g != groups.size(); ++g)
  {
    const auto& targets = groups.getGroup(g);
    for (uint8_t i = 0; i != targets.count; ++i)
    {
      if (!loadsOf(targets.pins[i], loadMap))
      {
        return false;
      }
    }
  }
  return true;
}

/** @brief Every group holds at least 2 loads. */
template< uint8_t NG, uint8_t MaxPins, uint8_t NL >
constexpr bool groupsHaveTwoLoads(const RotationGroups< NG, MaxPins >& groups, const uint8_t (&loadMap)[NL])
{
  for (uint8_t g = 0; g != groups.size(); ++g)
  {
    if (countLoads(groupLoads(groups, g, loadMap)) < 2)
    {
      return false;
    }
  }
  return true;
}

/** @brief No load is named twice, within a group or across groups. */
template< uint8_t NG, uint8_t MaxPins, uint8_t NL >
constexpr bool noLoadTwice(const RotationGroups< NG, MaxPins >& groups, const uint8_t (&loadMap)[NL])
{
  uint32_t seen{ 0 };
  for (uint8_t g = 0; g != groups.size(); ++g)
  {
    const auto& targets = groups.getGroup(g);
    for (uint8_t i = 0; i != targets.count; ++i)
    {
      const uint32_t loads = loadsOf(targets.pins[i], loadMap);
      if (seen & loads)
      {
        return false;
      }
      seen |= loads;
    }
  }
  return true;
}

/** @brief In each group, every load is local, or every load is on the same remote unit. */
template< uint8_t NG, uint8_t MaxPins, uint8_t NL >
constexpr bool groupsAreConsistent(const RotationGroups< NG, MaxPins >& groups, const uint8_t (&loadMap)[NL])
{
  for (uint8_t g = 0; g != groups.size(); ++g)
  {
    const uint32_t loads = groupLoads(groups, g, loadMap);
    bool first{ true };
    uint8_t unit{ 0 };
    for (uint8_t i = 0; i != NL; ++i)
    {
      if (!(loads & (1UL << i)))
      {
        continue;
      }
      if (first)
      {
        unit = Load::unitOf(loadMap[i]);
        first = false;
      }
      else if (Load::unitOf(loadMap[i]) != unit)
      {
        return false;
      }
    }
  }
  return true;
}

/**
 * @brief The rotation groups, resolved: the priority places each group holds.
 *
 * @details A group's places are the positions of the priority list that hold its loads at
 *          startup, in ascending order. rotate() moves the load in a group's last place to its
 *          first place and the others one place down, as LoadPriorities::rotate() does for the
 *          whole list; loads in no group never move.
 *
 * @tparam NG Number of groups.
 * @tparam NL Number of loads.
 */
template< uint8_t NG, uint8_t NL >
class Table
{
public:
  /**
   * @param groups the rotation groups, as in config.h
   * @param loadMap the load map (physicalLoadPin)
   * @param priorities the load numbers in priority order at startup (loadPrioritiesAtStartup)
   */
  template< uint8_t MaxPins >
  constexpr Table(const RotationGroups< NG, MaxPins >& groups, const uint8_t (&loadMap)[NL], const uint8_t (&priorities)[NL])
    : places_{}, ends_{}, loads_{}
  {
    uint8_t k{ 0 };
    for (uint8_t g = 0; g != NG; ++g)
    {
      loads_[g] = groupLoads(groups, g, loadMap);
      for (uint8_t p = 0; p != NL; ++p)
      {
        if (priorities[p] < 32 && (loads_[g] & (1UL << priorities[p])) && k < NL)
        {
          places_[k++] = p;
        }
      }
      ends_[g] = k;
    }
  }

  /** @brief Number of groups. */
  constexpr uint8_t size() const
  {
    return NG;
  }

  /** @brief The loads of group @p g: bit n set for load n. */
  constexpr uint32_t loads(const uint8_t g) const
  {
    return loads_[g];
  }

  /** @brief Rotates each group by one among its places; each entry keeps its ON bit. */
  template< uint8_t N >
  __attribute__((always_inline)) inline void rotate(uint8_t (&prio)[N]) const
  {
    uint8_t start{ 0 };
    for (uint8_t g = 0; g != NG; ++g)
    {
      const uint8_t end{ ends_[g] };
      if (end - start >= 2)
      {
        const uint8_t last{ prio[places_[end - 1]] };
        for (uint8_t i = end - 1; i != start; --i)
        {
          prio[places_[i]] = prio[places_[i - 1]];
        }
        prio[places_[start]] = last;
      }
      start = end;
    }
  }

private:
  uint8_t places_[NL]; /**< the places of group 0, then of group 1... */
  uint8_t ends_[NG];   /**< the end of each group in places_ */
  uint32_t loads_[NG]; /**< the loads of each group, by load number */
};

/** @brief No rotation group: nothing to hold, nothing to rotate. */
template< uint8_t NL >
class Table< 0, NL >
{
public:
  template< uint8_t MaxPins >
  constexpr Table(const RotationGroups< 0, MaxPins >& /* groups */, const uint8_t (& /* loadMap */)[NL],
                  const uint8_t (& /* priorities */)[NL])
  {
  }

  /** @brief Number of groups: none. */
  constexpr uint8_t size() const
  {
    return 0;
  }

  /** @brief No loads: never called, there for the loops over size(). */
  constexpr uint32_t loads(uint8_t) const
  {
    return 0;
  }

  /** @brief Nothing to rotate. */
  template< uint8_t N >
  void rotate(uint8_t (& /* prio */)[N]) const
  {
  }
};

/** @brief Deduction guide: the groups and the loads from the arguments. */
template< uint8_t NG, uint8_t MaxPins, uint8_t NL >
Table(const RotationGroups< NG, MaxPins >&, const uint8_t (&)[NL], const uint8_t (&)[NL]) -> Table< NG, NL >;
}  // namespace Rotation

#endif /* ROTATION_GROUPS_H */
