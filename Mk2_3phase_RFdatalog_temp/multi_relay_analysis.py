#!/usr/bin/env python3
"""
Multi-Relay System Analysis: Progressive Load Management with Batteries

This simulation demonstrates how multiple relays    # Top graph: Broken system with 0W thresholds
    create_multi_relay_subplot(ax1, time_hours, solar, house, net_before,
                              relay_states_broken, relays_broken, 
                              "BROKEN: All Relays Use 0W Import Threshold", False)
    
    # Bottom graph: Working system with progressive negative import thresholds  
    create_multi_relay_subplot(ax2, time_hours, solar, house, net_before, 
                              relay_states_good, relays_good,
                              "WORKS: Progressive Negative Import Thresholds", True)ferent negative import
thresholds create intelligent load management in battery systems. Shows
progressive load shedding as solar production declines through the evening.
"""

import numpy as np
import matplotlib.pyplot as plt

plt.rcParams['font.family'] = 'DejaVu Sans'

def simulate_multi_relay_system():
    """Create 2-relay system analysis for battery systems."""
    
    # Time from 5:30 PM to 7:00 PM (1.5 hours, evening decline)
    time_hours = np.linspace(17.5, 19, 90)  # 90 minutes, 1-minute resolution
    
    # Solar production: start higher to show both relays ON, then decline
    solar_production = 6000 * np.exp(-1.2 * (time_hours - 17.5))
    
    # Add some cloud variations
    cloud_dip1 = 1 - 0.3 * np.exp(-((time_hours - 18.1) / 0.05)**2)  # Cloud event
    cloud_dip2 = 1 - 0.2 * np.exp(-((time_hours - 18.4) / 0.04)**2)  # Another dip
    solar_production *= cloud_dip1 * cloud_dip2
    
    # House consumption: steady 500W (more realistic)
    house_consumption = np.full_like(time_hours, 500)
    
    # Net balance before any relay activation
    net_balance_before = solar_production - house_consumption
    
    # Create the multi-relay analysis
    create_multi_relay_graph(time_hours, solar_production, house_consumption, net_balance_before)

def simulate_multi_relay_progressive(net_balance):
    """
    Simulate 2 relays with progressive negative import thresholds.
    Heat pump (higher priority) and pool pump (lower priority).
    Water heater controlled by PV router triac, not external relay.
    Heat pump has 20min minimum ON/OFF times for compressor protection.
    """
    
    # Relay configurations: [power, negative_import_threshold, min_on_time, min_off_time]
    relays = [
        {"power": 2500, "threshold": -100, "name": "Heat Pump", "priority": 1, 
         "min_on_time": 20, "min_off_time": 20},  # 20min minimum times
        {"power": 1000, "threshold": -50, "name": "Pool Pump", "priority": 2,
         "min_on_time": 0, "min_off_time": 0}  # No minimum times for pool pump
    ]
    
    # Initialize relay states and timing tracking
    relay_states = {i: np.zeros(len(net_balance), dtype=bool) for i in range(len(relays))}
    last_state_change = {i: -999 for i in range(len(relays))}  # Time of last state change
    
    for i in range(len(net_balance)):
        current_surplus = net_balance[i]
        
        # Update each relay state
        for j, relay in enumerate(relays):
            was_on = relay_states[j][i-1] if i > 0 else False
            time_since_change = i - last_state_change[j]
            
            # Calculate surplus after other relays (excluding this one)
            other_relay_load = 0
            if i > 0:  # Only check previous states if not first iteration
                for k in range(len(relays)):
                    if k != j and relay_states[k][i-1]:
                        other_relay_load += relays[k]["power"]
            
            surplus_for_this_relay = current_surplus - other_relay_load
            
            # Turn ON logic: enough surplus for this relay + safety margin
            turn_on_threshold = relay["power"] + abs(relay["threshold"])
            can_turn_on = (not was_on and 
                          surplus_for_this_relay > turn_on_threshold and
                          time_since_change >= relay["min_off_time"])
            
            # Turn OFF logic: surplus after this relay drops below threshold
            surplus_after_this_relay = surplus_for_this_relay - relay["power"]
            can_turn_off = (was_on and 
                           surplus_after_this_relay < abs(relay["threshold"]) and
                           time_since_change >= relay["min_on_time"])
            
            # Apply state logic with minimum time constraints
            if can_turn_on:
                relay_states[j][i] = True
                last_state_change[j] = i
            elif can_turn_off:
                relay_states[j][i] = False
                last_state_change[j] = i
            else:
                relay_states[j][i] = was_on  # Keep previous state
    
    return relay_states, relays

def simulate_multi_relay_broken(net_balance):
    """
    Simulate the same 2 relays but with 0W import thresholds (broken).
    Heat pump still respects minimum times, but can't turn off due to 0W threshold.
    Pool pump stays on once turned on, leading to battery drain.
    """
    
    relays = [
        {"power": 2500, "name": "Heat Pump", "min_on_time": 20, "min_off_time": 20},
        {"power": 1000, "name": "Pool Pump", "min_on_time": 0, "min_off_time": 0}
    ]
    
    relay_states = {i: np.zeros(len(net_balance), dtype=bool) for i in range(len(relays))}
    last_state_change = {i: -999 for i in range(len(relays))}
    
    for i in range(len(net_balance)):
        current_surplus = net_balance[i]
        
        for j, relay in enumerate(relays):
            was_on = relay_states[j][i-1] if i > 0 else False
            time_since_change = i - last_state_change[j]
            
            # Turn ON when enough surplus (conservative threshold)
            can_turn_on = (not was_on and 
                          current_surplus > relay["power"] * 1.2 and
                          time_since_change >= relay.get("min_off_time", 0))
            
            if can_turn_on:
                relay_states[j][i] = True
                last_state_change[j] = i
            else:
                # With 0W threshold: never turn off (battery prevents import detection)
                # Heat pump still respects minimum ON time before it could turn off
                relay_states[j][i] = was_on
    
    return relay_states, relays

def create_multi_relay_graph(time_hours, solar, house, net_before):
    """Create comprehensive multi-relay analysis graph."""
    
    # Simulate both scenarios
    relay_states_good, relays_good = simulate_multi_relay_progressive(net_before)
    relay_states_broken, relays_broken = simulate_multi_relay_broken(net_before)
    
    # Create figure with two subplots
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(18, 12))
    fig.suptitle('2-Relay System: Heat Pump & Pool Pump with Battery Systems\n' +
                 'Evening Decline (17:30-19:00) - Logarithmic Scale for Threshold Visibility', 
                 fontsize=16, fontweight='bold', y=0.98)
    
    # Top graph: Broken system with 0W thresholds
    create_multi_relay_subplot(ax1, time_hours, solar, house, net_before,
                              relay_states_broken, relays_broken, 
                              "BROKEN: All Relays Use 0W Import Threshold", False)
    
    # Bottom graph: Working system with progressive negative import thresholds  
    create_multi_relay_subplot(ax2, time_hours, solar, house, net_before, 
                              relay_states_good, relays_good,
                              "WORKS: Progressive Negative Import Thresholds", True)
    
    # Add explanation
    explanation = (
        "MULTI-RELAY INSIGHT: Progressive thresholds create intelligent load management!\n"
        "• Working system: Relays turn off in priority order as surplus decreases (load shedding)\n"
        "• Broken system: All relays stay on once activated, causing massive battery drain\n"
        "• Battery compensation prevents proper load management with positive import thresholds"
    )
    
    fig.text(0.02, 0.02, explanation, fontsize=11, style='italic',
             bbox=dict(boxstyle="round,pad=0.5", facecolor="lightyellow", alpha=0.9))
    
    plt.tight_layout()
    plt.subplots_adjust(bottom=0.12, top=0.90)
    
    # Save the plot
    filename = 'multi_relay_battery_system.png'
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    print(f"Multi-relay system graph saved as: {filename}")
    
    # Print analysis
    print_multi_relay_analysis(relay_states_good, relay_states_broken, relays_good, time_hours)

def create_multi_relay_subplot(ax, time_hours, solar, house, net_before, 
                              relay_states, relays, title, is_working):
    """Create subplot showing multi-relay behavior."""
    
    # Calculate total relay load over time
    total_relay_load = np.zeros(len(time_hours))
    for i in range(len(relays)):
        total_relay_load += relay_states[i].astype(int) * relays[i]["power"]
    
    # Calculate derived values
    net_after_relays = net_before - total_relay_load
    battery_output = np.maximum(0, -net_after_relays)
    grid_power = net_after_relays + battery_output
    
    # Add measurement noise
    grid_power += np.random.normal(0, 3, len(time_hours))
    
    # Colors for different relays
    relay_colors = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#96CEB4']
    
    # Plot stacked relay loads
    bottom = np.zeros(len(time_hours))
    for i, relay in enumerate(relays):
        relay_load = relay_states[i].astype(int) * relay["power"]
        ax.fill_between(time_hours, bottom, bottom + relay_load, 
                       alpha=0.7, color=relay_colors[i], 
                       label=f'{relay["name"]} ({relay["power"]}W)')
        bottom += relay_load
    
    # Plot other power curves
    ax.plot(time_hours, solar, label='Solar Production', color='gold', linewidth=3, alpha=0.9)
    ax.plot(time_hours, house, label='House Load (500W)', color='red', linewidth=2, linestyle='--')
    ax.plot(time_hours, net_before, label='Net Balance (before relays)', color='blue', linewidth=3)
    ax.plot(time_hours, net_after_relays, label='Net Balance (after relays)', color='purple', linewidth=2, linestyle=':')
    ax.plot(time_hours, battery_output, label='Battery Output', color='orange', linewidth=3)
    ax.plot(time_hours, grid_power, label='Grid Power', color='black', linewidth=2)
    
    # Reference lines
    ax.axhline(0, color='gray', linewidth=1, alpha=0.7, label='Zero line')
    
    # Note: Threshold lines are now added in the formatting section above
    
    # Statistics
    total_switches = sum(np.sum(np.diff(relay_states[i].astype(int)) != 0) 
                        for i in range(len(relays)))
    max_total_load = np.max(total_relay_load)
    avg_battery = np.mean(battery_output)
    
    ax.set_title(f'{title}\n' +
                f'Total Switches: {total_switches} | Max Load: {max_total_load:.0f}W | Avg Battery: {avg_battery:.0f}W',
                fontsize=12, fontweight='bold')
    
    # Formatting with logarithmic scale for better visibility of small values
    ax.set_ylabel('Power (W) - Log Scale', fontweight='bold')
    ax.set_xlim(17.5, 19)
    
    # Use logarithmic scale to make small threshold values visible
    ax.set_yscale('symlog', linthresh=50)  # Reduced linear threshold for better small value visibility
    ax.set_ylim(-10, 6000)  # Tiny negative range, maximum positive range
    
    # Custom Y-axis ticks for logarithmic scale
    major_ticks = [0, 50, 100, 250, 500, 1000, 2000, 3000, 4000, 5000, 6000]
    ax.set_yticks(major_ticks)
    
    # Add horizontal reference lines for the POSITIVE surplus thresholds
    # (negative import thresholds = positive surplus requirements)
    if is_working:
        ax.axhline(50, color='cyan', linestyle='--', linewidth=2, alpha=0.8, 
                  label='Pool Pump ON at 50W surplus (-50W import threshold)')
        ax.axhline(100, color='red', linestyle='--', linewidth=2, alpha=0.8, 
                  label='Heat Pump ON at 100W surplus (-100W import threshold)')
    
    ax.grid(True, alpha=0.3)
    ax.grid(True, which='minor', alpha=0.15)  # Minor grid for better readability
    ax.legend(fontsize=9, loc='lower right', framealpha=0.9, ncol=2)
    
    # Time labels
    time_labels = ['17:30', '17:45', '18:00', '18:15', '18:30', '18:45', '19:00']
    ax.set_xticks([17.5, 17.75, 18, 18.25, 18.5, 18.75, 19])
    ax.set_xticklabels(time_labels)
    
    # Add annotations
    if is_working:
        ax.annotate('Progressive load shedding\nas surplus decreases', 
                   xy=(18.3, 1000), xytext=(17.7, 2500),
                   arrowprops=dict(arrowstyle='->', color='green', lw=2),
                   fontsize=11, color='green', fontweight='bold',
                   bbox=dict(boxstyle="round,pad=0.3", facecolor="lightgreen"))
    else:
        ax.annotate('All relays stay on\ncausing battery drain', 
                   xy=(18.5, 1500), xytext=(17.6, 2800),
                   arrowprops=dict(arrowstyle='->', color='red', lw=2),
                   fontsize=11, color='red', fontweight='bold',
                   bbox=dict(boxstyle="round,pad=0.3", facecolor="lightyellow"))

def print_multi_relay_analysis(relay_states_good, relay_states_broken, relays, time_hours):
    """Print detailed multi-relay analysis."""
    
    print("\n" + "="*80)
    print("MULTI-RELAY SYSTEM ANALYSIS (17:30-19:00)")
    print("="*80)
    
    # Analyze working system
    print(f"\nWORKING SYSTEM (Negative Import Thresholds + Minimum Times):")
    total_switches_good = 0
    for i, relay in enumerate(relays):
        switches = np.sum(np.diff(relay_states_good[i].astype(int)) != 0)
        on_time = np.sum(relay_states_good[i])
        total_switches_good += switches
        min_time_info = f", {relay['min_on_time']}min min ON/OFF" if relay['min_on_time'] > 0 else ""
        print(f"  • {relay['name']}: {switches} switches, {on_time} min ON ({relay['threshold']}W threshold{min_time_info})")
    
    print(f"\nBROKEN SYSTEM (0W Import Thresholds):")
    total_switches_broken = 0
    for i, relay in enumerate(relays):
        switches = np.sum(np.diff(relay_states_broken[i].astype(int)) != 0)
        on_time = np.sum(relay_states_broken[i])
        total_switches_broken += switches
        print(f"  • {relay['name']}: {switches} switches, {on_time} min ON (0W threshold)")
    
    # Calculate total energy implications
    total_energy_good = sum(np.sum(relay_states_good[i]) * relays[i]["power"] / 60 
                           for i in range(len(relays)))  # Wh
    total_energy_broken = sum(np.sum(relay_states_broken[i]) * relays[i]["power"] / 60 
                             for i in range(len(relays)))  # Wh
    
    print(f"\nENERGY COMPARISON:")
    print(f"  • Working system: {total_energy_good:.0f} Wh relay load")
    print(f"  • Broken system: {total_energy_broken:.0f} Wh relay load")
    print(f"  • Excess consumption: {total_energy_broken - total_energy_good:.0f} Wh ({(total_energy_broken/total_energy_good-1)*100:.0f}% more)")
    
    print(f"\nMULTI-RELAY FINDINGS:")
    print(f"1. Progressive thresholds enable intelligent load management")
    print(f"2. Heat pump respects 20min minimum ON/OFF times for compressor protection")
    print(f"3. Working system: {total_switches_good} total switches (proper cycling)")
    print(f"4. Broken system: {total_switches_broken} total switches (sticky relays)")
    print(f"5. Broken system consumes {(total_energy_broken/total_energy_good-1)*100:.0f}% more energy from battery")

def main():
    """Run the 2-relay system analysis."""
    print("2-Relay System Analysis: Heat Pump & Pool Pump")
    print("=" * 50)
    print("Analyzing 2-relay system during evening decline")
    print("Time: 17:30-19:00, Heat Pump (2.5kW) + Pool Pump (1kW)")
    print("Note: Water heater controlled by PV router triac")
    print("Heat pump: 20min minimum ON/OFF times for compressor protection")
    print()
    
    simulate_multi_relay_system()

if __name__ == "__main__":
    main()