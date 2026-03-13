#!/bin/bash

# PV Router Cloud Pattern Analysis Script
# Runs comprehensive tests and provides personalized tuning recommendations

echo "🌤️ ================================================ 🌤️"
echo "   PV Router RELAY_FILTER_DELAY_MINUTES Tuner"
echo "   Scientific approach to optimize cloud immunity"
echo "🌤️ ================================================ 🌤️"
echo

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    echo "❌ Error: Please run this script from the Mk2_3phase_RFdatalog_temp directory"
    echo "   cd Mk2_3phase_RFdatalog_temp && ./scripts/tune_cloud_immunity.sh"
    exit 1
fi

echo "🧪 Running cloud pattern analysis tests..."
echo "   This will test different RELAY_FILTER_DELAY_MINUTES values (1-5 minutes)"
echo "   against realistic cloud scenarios to find your optimal setting."
echo

# Run the tests and capture output
TEST_OUTPUT=$(pio test -e native --filter="test_cloud_patterns" -v 2>/dev/null)

# Extract key results for user
echo "📊 ANALYSIS RESULTS"
echo "==================="
echo

# Parse test results for recommendations
echo "$TEST_OUTPUT" | grep -A 20 "Relay State Changes Summary" | head -10
echo

echo "🎯 PERSONALIZED RECOMMENDATIONS"
echo "==============================="
echo

# Extract recommendations from test output
echo "$TEST_OUTPUT" | grep "Recommendation for" -A 1 | sed 's/Recommendation for/📍 For/' | sed 's/✓/  →/'
echo

echo "⚙️  CONFIGURATION EXAMPLES"
echo "========================="
echo
echo "Copy one of these configurations to your config.h file:"
echo

echo "// For clear sky regions (desert, dry climates):"
echo "inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 1 };"
echo

echo "// For mixed conditions (recommended default):"
echo "inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 2 };"
echo

echo "// For cloudy regions (coastal, temperate):"
echo "inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 3 };"
echo

echo "// For very cloudy conditions (mountain, tropical):"
echo "inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 4 };"
echo

echo "🔋 BATTERY SYSTEM NOTES"
echo "======================="
echo "If you have a grid-tied battery system, also consider using negative"
echo "import thresholds to prevent relay cycling due to battery buffering:"
echo
echo "// Normal installation (turns OFF when importing > 200W):"
echo "inline constexpr RelayEngine< 1, RELAY_FILTER_DELAY_MINUTES > relays{"
echo "  { { unused_pin, 1000, 200, 1, 1 } }"
echo "};"
echo
echo "// Battery installation (turns OFF when surplus < 100W):"
echo "inline constexpr RelayEngine< 1, RELAY_FILTER_DELAY_MINUTES > relays{"
echo "  { { unused_pin, 1000, -100, 1, 1 } }"
echo "};"
echo

echo "📖 For complete details, see: docs/Cloud_Pattern_Tuning_Guide.md"
echo
echo "🎨 VISUALIZATION OPTION"
echo "======================="
echo "For beautiful graphs showing power curves, TEMA filtering, and relay states:"
echo
echo "📊 Demo Visualization (recommended):"
echo "./scripts/visualize_cloud_patterns_demo.py"
echo
echo "📈 Live Data Visualization (advanced):"
echo "./scripts/visualize_cloud_patterns.py"
echo
echo "✅ Analysis complete! Apply the recommended setting to your config.h"
echo "   and test in real conditions to validate performance."
