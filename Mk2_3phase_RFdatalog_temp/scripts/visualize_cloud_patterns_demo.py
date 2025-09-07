#!/usr/bin/env python3
"""
PV Router Cloud Pattern Visualization Tool (Demo Version)
Generates beautiful graphs showing TEMA filtering behavior with simulated cloud patterns.
"""

import matplotlib.pyplot as plt
import numpy as np
import sys
import os

def generate_sample_cloud_data():
    """Generate sample cloud pattern data for demonstration."""
    
    # Time array (20 minutes, 5-second intervals)
    times_seconds = np.arange(0, 1200, 5)  # 0 to 1200 seconds (20 minutes)
    times_minutes = times_seconds / 60
    n_samples = len(times_minutes)  # Should be 240 samples
    
    # Scenario 1: Light cloud cover
    np.random.seed(42)  # For reproducible results
    base_power = 2000
    
    # Create power array with exact number of samples
    power1 = np.ones(n_samples) * base_power
    
    # Add noise to baseline
    power1 += np.random.normal(0, 20, n_samples)
    
    # Cloud event at 5-8 minutes (samples 60-96)
    cloud_start = 60
    cloud_end = 96
    for i in range(cloud_start, min(cloud_end, n_samples)):
        # Gradual power reduction during cloud
        reduction_factor = 0.4 + 0.4 * np.sin((i - cloud_start) * np.pi / (cloud_end - cloud_start))
        power1[i] *= reduction_factor
        power1[i] += np.random.normal(0, 50)
    
    # Scenario 2: Heavy clouds with rapid changes
    power2 = np.ones(n_samples) * base_power
    power2 += np.random.normal(0, 20, n_samples)
    
    # Rapid fluctuations 5-15 minutes (samples 60-180)
    for i in range(60, min(180, n_samples)):
        if (i // 10) % 2:  # Every 10 samples (50 seconds), alternate cloud/sun
            power2[i] *= 0.3  # Heavy cloud
            power2[i] += np.random.normal(0, 50)
        else:
            power2[i] *= 0.9  # Partial sun
            power2[i] += np.random.normal(0, 30)
    
    return {
        'times': times_minutes,
        'scenario1': {
            'name': 'Light Cloud Cover',
            'power': np.maximum(power1, 0),  # Ensure no negative power
        },
        'scenario2': {
            'name': 'Heavy Cloud + Rapid Changes', 
            'power': np.maximum(power2, 0),
        }
    }

def calculate_tema(power_data, delay_minutes):
    """Calculate TEMA (Triple EMA) filtering for given power data and delay setting.
    
    This matches the actual diverter implementation which uses getAverageT() for relay control.
    TEMA provides better cloud immunity than single EMA.
    
    IMPORTANT: The C++ code uses different alpha values for each EMA level:
    - EMA: uses full smoothing period (A)
    - EMA_EMA: uses half period (A-1) -> 2x faster response
    - EMA_EMA_EMA: uses quarter period (A-2) -> 4x faster response
    """
    # Convert delay to alpha parameter used in the diverter code
    # A = delay_minutes * 12 (for 5-second sampling)
    A = int(delay_minutes * 12)
    
    # Initialize arrays for EMA, EMA_EMA, and EMA_EMA_EMA
    ema_raw = np.zeros_like(power_data, dtype=np.int64)
    ema = np.zeros_like(power_data, dtype=np.int32)
    ema_ema_raw = np.zeros_like(power_data, dtype=np.int64)
    ema_ema = np.zeros_like(power_data, dtype=np.int32)
    ema_ema_ema_raw = np.zeros_like(power_data, dtype=np.int64)
    ema_ema_ema = np.zeros_like(power_data, dtype=np.int32)
    
    # Helper function to find nearest power of 2 (matching C++ implementation)
    def round_up_to_power_of_2(v):
        import math
        if v & (v - 1) == 0:  # Already power of 2
            return int(math.log2(v)) - 1
        return int(math.log2(v))
    
    # Calculate shift values - this is the key difference from our previous version!
    shift_A = round_up_to_power_of_2(A)        # Full smoothing period
    shift_A_half = shift_A - 1                 # Half period (2x faster)
    shift_A_quarter = shift_A - 2              # Quarter period (4x faster)
    
    # Process each sample (matching the C++ addValue implementation exactly)
    for i in range(len(power_data)):
        input_val = int(power_data[i])
        
        # EMA calculation (original smoothing period)
        ema_raw[i] = (ema_raw[i-1] if i > 0 else 0) - (ema[i-1] if i > 0 else 0) + input_val
        ema[i] = int(ema_raw[i] >> shift_A)
        
        # EMA of EMA calculation (half period - 2x faster response)
        ema_ema_raw[i] = (ema_ema_raw[i-1] if i > 0 else 0) - (ema_ema[i-1] if i > 0 else 0) + ema[i]
        ema_ema[i] = int(ema_ema_raw[i] >> shift_A_half)
        
        # EMA of EMA of EMA calculation (quarter period - 4x faster response)
        ema_ema_ema_raw[i] = (ema_ema_ema_raw[i-1] if i > 0 else 0) - (ema_ema_ema[i-1] if i > 0 else 0) + ema_ema[i]
        ema_ema_ema[i] = int(ema_ema_ema_raw[i] >> shift_A_quarter)
    
    # Calculate TEMA using the exact C++ formula: 3 * (ema - ema_ema) + ema_ema_ema
    tema = 3 * (ema - ema_ema) + ema_ema_ema
    
    return tema.astype(np.float64)

def calculate_relay_states(tema_values, threshold=1000):
    """Calculate relay states based on TEMA values and threshold."""
    relay_states = np.zeros_like(tema_values)
    current_state = 0
    hysteresis = 100  # 100W hysteresis
    
    for i, power in enumerate(tema_values):
        if current_state == 0:  # Relay OFF
            if power >= threshold + hysteresis:
                current_state = 1
        else:  # Relay ON
            if power <= threshold - hysteresis:
                current_state = 0
        
        relay_states[i] = current_state
    
    return relay_states

def create_cloud_visualization():
    """Create comprehensive cloud pattern visualization."""
    
    # Generate sample data
    data = generate_sample_cloud_data()
    
    # Set up the plot
    plt.style.use('seaborn-v0_8' if 'seaborn-v0_8' in plt.style.available else 'default')
    
    fig, axes = plt.subplots(2, 1, figsize=(15, 12))
    fig.suptitle('PV Router Cloud Pattern Analysis\nTEMA Filter Optimization for Relay Control', 
                 fontsize=16, fontweight='bold', y=0.98)
    
    colors = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#96CEB4', '#FECA57']
    delay_values = [1, 2, 3, 4, 5]
    
    scenarios = [
        (data['scenario1'], 'scenario1'),
        (data['scenario2'], 'scenario2')
    ]
    
    for scenario_idx, (scenario_data, scenario_key) in enumerate(scenarios):
        ax = axes[scenario_idx]
        ax2 = ax.twinx()
        
        times = data['times']
        power = scenario_data['power']
        
        # Plot raw power (reference)
        ax.plot(times, power, 'lightgray', linewidth=1.5, alpha=0.7, 
               label='Raw Power', zorder=1)
        
        # Plot TEMA filtered data and relay states for each delay
        for i, delay in enumerate(delay_values):
            tema = calculate_tema(power, delay)
            relay_states = calculate_relay_states(tema)
            
            # Plot TEMA line
            ax.plot(times, tema, color=colors[i], linewidth=2.5, 
                   label=f'TEMA {delay}min', alpha=0.9, zorder=3)
            
            # Plot relay states as filled areas
            relay_y = relay_states * 100  # Scale for visibility
            ax2.fill_between(times, 0, relay_y, color=colors[i], 
                           alpha=0.15, step='post', 
                           label=f'Relay {delay}min')
        
        # Add threshold line
        ax.axhline(y=1000, color='red', linestyle='--', alpha=0.7, 
                  linewidth=2, label='Relay Threshold (1000W)')
        
        # Formatting
        ax.set_xlabel('Time (minutes)')
        ax.set_ylabel('Power (W)', color='black')
        ax.set_title(f"Scenario {scenario_idx + 1}: {scenario_data['name']}", 
                    fontsize=14, fontweight='bold', pad=20)
        
        # Relay state axis
        ax2.set_ylabel('Relay State', color='gray')
        ax2.set_ylim(0, 120)
        ax2.set_yticks([0, 100])
        ax2.set_yticklabels(['OFF', 'ON'])
        
        # Combined legend
        lines1, labels1 = ax.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        ax.legend(lines1 + lines2, labels1 + labels2, 
                 loc='upper right', bbox_to_anchor=(1, 1), fontsize=10)
        
        # Grid and formatting
        ax.grid(True, alpha=0.3)
        ax.set_axisbelow(True)
        ax.set_xlim(0, 20)
        ax.set_ylim(0, max(power) * 1.1)
        
        # Highlight cloud events
        if scenario_idx == 0:  # Light clouds
            ax.axvspan(5, 8, alpha=0.1, color='orange', label='Cloud Event')
        else:  # Heavy clouds
            ax.axvspan(5, 15, alpha=0.1, color='orange', label='Cloud Events')
    
    plt.tight_layout()
    plt.subplots_adjust(top=0.92)  # Make room for suptitle
    
    # Save the plot
    output_file = 'cloud_pattern_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight', 
                facecolor='white', edgecolor='none')
    
    return output_file

def create_switching_analysis():
    """Create relay switching frequency analysis."""
    
    data = generate_sample_cloud_data()
    
    fig, ax = plt.subplots(1, 1, figsize=(12, 8))
    
    scenarios = ['Light Clouds', 'Heavy Clouds']
    delay_values = [1, 2, 3, 4, 5]
    colors = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#96CEB4', '#FECA57']
    
    switching_counts = []
    
    for scenario_idx, scenario_key in enumerate(['scenario1', 'scenario2']):
        scenario_data = data[scenario_key]
        power = scenario_data['power']
        
        scenario_switches = []
        for delay in delay_values:
            tema = calculate_tema(power, delay)
            relay_states = calculate_relay_states(tema)
            
            # Count state changes
            switches = np.sum(np.diff(relay_states) != 0)
            scenario_switches.append(switches)
        
        switching_counts.append(scenario_switches)
    
    # Create grouped bar chart
    x = np.arange(len(scenarios))
    width = 0.15
    
    for i, delay in enumerate(delay_values):
        offset = (i - 2) * width
        values = [switching_counts[j][i] for j in range(len(scenarios))]
        ax.bar(x + offset, values, width, label=f'{delay} min', 
               color=colors[i], alpha=0.8)
    
    ax.set_xlabel('Cloud Scenarios')
    ax.set_ylabel('Number of Relay Switches')
    ax.set_title('Relay Switching Frequency Analysis\n(Lower is Better - More Stable)', 
                fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(scenarios)
    ax.legend(title='RELAY_FILTER_DELAY_MINUTES')
    ax.grid(True, alpha=0.3, axis='y')
    
    # Add recommendation box
    textstr = ('Recommendations:\n'
               '• Clear skies: 1-2 min\n'
               '• Mixed clouds: 2-3 min\n'  
               '• Heavy clouds: 3-4 min\n'
               '• Very unstable: 4-5 min')
    props = dict(boxstyle='round', facecolor='lightblue', alpha=0.8)
    ax.text(0.02, 0.98, textstr, transform=ax.transAxes, fontsize=10,
            verticalalignment='top', bbox=props)
    
    plt.tight_layout()
    plt.subplots_adjust(top=0.88)  # Make room for title
    
    summary_file = 'relay_switching_summary.png'
    plt.savefig(summary_file, dpi=300, bbox_inches='tight', 
                facecolor='white', edgecolor='none')
    
    return summary_file

def main():
    """Main visualization function."""
    print("=" * 52)
    print("   PV Router Cloud Pattern Visualization Tool")
    print("   Generating beautiful TEMA analysis graphs")
    print("=" * 52)
    print()
    
    # Check if we're in the right directory
    if not os.path.exists('platformio.ini'):
        print("❌ Error: Please run this script from the Mk2_3phase_RFdatalog_temp directory")
        print("   cd Mk2_3phase_RFdatalog_temp && python3 scripts/visualize_cloud_patterns.py")
        sys.exit(1)
    
    # Check dependencies
    try:
        import matplotlib
        import numpy
        print("Dependencies found")
    except ImportError:
        print("Installing matplotlib and numpy...")
        import subprocess
        subprocess.run([sys.executable, '-m', 'pip', 'install', 'matplotlib', 'numpy'])
        print("Dependencies installed")
    
    print("Generating cloud pattern simulations...")
    print("   • Scenario 1: Light cloud cover (single cloud event)")
    print("   • Scenario 2: Heavy clouds with rapid changes")
    print()
    
    try:
        # Create main analysis plot
        print("Creating main analysis graphs...")
        main_plot = create_cloud_visualization()
        print(f"Main analysis plot: {main_plot}")
        
        # Create switching analysis
        print("Creating switching frequency analysis...")
        summary_plot = create_switching_analysis()
        print(f"Summary plot: {summary_plot}")
        
        print()
        print("VISUALIZATION COMPLETE!")
        print("=" * 50)
        print(f"Main Analysis: {main_plot}")
        print(f"Summary Chart: {summary_plot}")
        print()
        print("How to read the graphs:")
        print("• Gray line = Raw power measurements")
        print("• Colored lines = TEMA filtered values (different delays)")
        print("• Shaded areas = Relay ON/OFF states")
        print("• Red dashed line = Relay activation threshold (1000W)")
        print("• Orange shaded = Cloud event periods")
        print()
        print("Tuning Guide:")
        print("• Choose the delay that gives smooth curves with minimal relay switching")
        print("• Higher delays = more stability but slower response")
        print("• Lower delays = faster response but more relay chatter")
        print()
        print("Apply your chosen setting in config.h:")
        print("   inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ X };")
        
    except Exception as e:
        print(f"❌ Error creating visualizations: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()