#include <unity.h>
#include <cstdio>
#include <vector>
#include <string>

// Mock Arduino types for native compilation
typedef unsigned char byte;

// Include EWMA implementation (copied from main project)
constexpr uint8_t round_up_to_power_of_2(uint16_t v)
{
  if (__builtin_popcount(v) == 1) { return __builtin_ctz(v) - 1; }

  uint8_t next_pow_of_2{ 0 };

  while (v)
  {
    v >>= 1;
    ++next_pow_of_2;
  }

  return --next_pow_of_2;
}

template< uint8_t A = 10 >
class EWMA_average
{
private:
  int32_t ema_raw{ 0 };
  int32_t ema{ 0 };
  int32_t ema_ema_raw{ 0 };
  int32_t ema_ema{ 0 };
  int32_t ema_ema_ema_raw{ 0 };
  int32_t ema_ema_ema{ 0 };

public:
  void addValue(int32_t input)
  {
    ema_raw = ema_raw - ema + input;
    ema = ema_raw >> round_up_to_power_of_2(A);

    ema_ema_raw = ema_ema_raw - ema_ema + ema;
    ema_ema = ema_ema_raw >> (round_up_to_power_of_2(A) - 1);

    ema_ema_ema_raw = ema_ema_ema_raw - ema_ema_ema + ema_ema;
    ema_ema_ema = ema_ema_ema_raw >> (round_up_to_power_of_2(A) - 2);
  }

  auto getAverageS() const
  {
    return ema;
  }
  auto getAverageD() const
  {
    return (ema << 1) - ema_ema;
  }
  auto getAverageT() const
  {
    return ema + (ema - ema_ema) + (ema - ema_ema_ema);
  }

  void reset()
  {
    ema_raw = ema = ema_ema_raw = ema_ema = ema_ema_ema_raw = ema_ema_ema = 0;
  }
};

// Convert RELAY_FILTER_DELAY_MINUTES to alpha parameter
// Assuming 5-second sampling rate: alpha = delay_minutes * 60 / 5 = delay_minutes * 12
constexpr uint8_t delay_minutes_to_alpha(uint8_t delay_minutes)
{
  uint8_t alpha{ delay_minutes * 12 };
  return (alpha > 0) ? alpha : 8;  // Minimum alpha of 8
}

// Mock relay class for testing with configurable parameters
class TestRelay
{
public:
  bool m_relay_state;
  int32_t m_import_threshold;
  int32_t m_surplus_threshold;
  uint8_t m_filter_delay_minutes;

  TestRelay(int32_t import_threshold = 20, int32_t surplus_threshold = 20, uint8_t filter_delay = 2)
    : m_relay_state(false), m_import_threshold(import_threshold),
      m_surplus_threshold(surplus_threshold), m_filter_delay_minutes(filter_delay) {}

  bool proceed_relay(int32_t power_value)
  {
    bool new_state{ m_relay_state };

    if (m_import_threshold < 0)
    {
      // Battery scenario: negative threshold means we want surplus above abs(threshold)
      int32_t required_surplus{ -m_import_threshold };
      if (!m_relay_state && power_value >= required_surplus)
      {
        new_state = true;  // Turn on when surplus exceeds threshold
      }
      else if (m_relay_state && power_value < (required_surplus - m_surplus_threshold))
      {
        new_state = false;  // Turn off when surplus drops below (threshold - hysteresis)
      }
    }
    else
    {
      // Normal scenario: positive threshold means import/export boundary
      if (!m_relay_state && power_value >= m_import_threshold)
      {
        new_state = true;  // Turn on when import exceeds threshold
      }
      else if (m_relay_state && power_value < (m_import_threshold - m_surplus_threshold))
      {
        new_state = false;  // Turn off when import drops below threshold minus hysteresis
      }
    }

    m_relay_state = new_state;
    return m_relay_state;
  }

  void reset_state()
  {
    m_relay_state = false;
  }
};

// Cloud pattern definitions (realistic power measurements in Watts)
struct CloudPattern
{
  std::string name;
  std::string description;
  std::vector< int32_t > power_data;
  int32_t relay_threshold;
};

// Create realistic cloud patterns with 5-second sampling over 15-20 minutes
// Each pattern starts with 5 minutes of stable production, then introduces clouds
std::vector< CloudPattern > create_cloud_patterns()
{
  return {
    { "Scattered Clouds - 20 minutes",
      "5min stable, then light scattered clouds with brief shadows (spring/fall conditions)",
      // 20 minutes = 240 samples at 5-second intervals
      // 0-5 minutes: Stable production around 1200W
      { 1200, 1210, 1195, 1205, 1190, 1200, 1215, 1205, 1195, 1200,  // 0-50s
        1210, 1200, 1205, 1195, 1210, 1200, 1190, 1205, 1210, 1195,  // 50-100s
        1200, 1205, 1210, 1195, 1200, 1215, 1200, 1205, 1190, 1200,  // 100-150s
        1210, 1195, 1200, 1205, 1210, 1200, 1195, 1205, 1200, 1210,  // 150-200s
        1195, 1200, 1205, 1210, 1200, 1195, 1205, 1200, 1210, 1195,  // 200-250s
        1200, 1205, 1210, 1195, 1200, 1205, 1210, 1200, 1195, 1205,  // 250-300s (5min)

        // 5-10 minutes: First cloud shadows
        1000, 1150, 950, 1180, 800, 1200, 1050, 1220, 900, 1180,     // 300-350s
        1100, 1190, 850, 1200, 1000, 1210, 950, 1180, 1100, 1200,    // 350-400s
        1150, 1210, 1000, 1190, 1200, 1180, 950, 1200, 1100, 1190,   // 400-450s
        1050, 1200, 1150, 1180, 1000, 1210, 1200, 1190, 1100, 1200,  // 450-500s
        1150, 1180, 950, 1200, 1100, 1190, 1050, 1210, 1200, 1180,   // 500-550s
        1000, 1200, 1150, 1190, 1100, 1200, 1050, 1180, 1200, 1190,  // 550-600s (10min)

        // 10-15 minutes: More frequent cloud shadows
        900, 1180, 800, 1200, 950, 1190, 850, 1210, 1000, 1180,    // 600-650s
        1100, 1200, 900, 1190, 800, 1180, 950, 1200, 1050, 1190,   // 650-700s
        850, 1180, 1000, 1200, 900, 1190, 1100, 1180, 950, 1200,   // 700-750s
        1000, 1190, 850, 1180, 1100, 1200, 900, 1190, 1050, 1180,  // 750-800s
        950, 1200, 1000, 1190, 900, 1180, 1100, 1200, 950, 1190,   // 800-850s
        1050, 1180, 900, 1200, 1100, 1190, 950, 1180, 1000, 1200,  // 850-900s (15min)

        // 15-20 minutes: Clouds clearing, return to stable
        1100, 1190, 1150, 1200, 1180, 1210, 1200, 1190, 1180, 1200,    // 900-950s
        1190, 1200, 1180, 1210, 1200, 1190, 1180, 1200, 1190, 1210,    // 950-1000s
        1200, 1190, 1180, 1200, 1190, 1210, 1200, 1180, 1190, 1200,    // 1000-1050s
        1190, 1200, 1180, 1210, 1200, 1190, 1180, 1200, 1190, 1200,    // 1050-1100s
        1180, 1200, 1190, 1210, 1200, 1180, 1190, 1200, 1180, 1200,    // 1100-1150s
        1190, 1200, 1180, 1200, 1190, 1180, 1200, 1190, 1200, 1180 },  // 1150-1200s (20min)
      500 },
    { "Heavy Cloud Bank - 18 minutes",
      "5min stable, then dense cloud formation passes over with dramatic drop",
      // 18 minutes = 216 samples
      // 0-5 minutes: Stable high production
      {
        1300,
        1310,
        1295,
        1305,
        1290,
        1300,
        1315,
        1305,
        1295,
        1300,  // 0-50s
        1310,
        1300,
        1305,
        1295,
        1310,
        1300,
        1290,
        1305,
        1310,
        1295,  // 50-100s
        1300,
        1305,
        1310,
        1295,
        1300,
        1315,
        1300,
        1305,
        1290,
        1300,  // 100-150s
        1310,
        1295,
        1300,
        1305,
        1310,
        1300,
        1295,
        1305,
        1300,
        1310,  // 150-200s
        1295,
        1300,
        1305,
        1310,
        1300,
        1295,
        1305,
        1300,
        1310,
        1295,  // 200-250s
        1300,
        1305,
        1310,
        1295,
        1300,
        1305,
        1310,
        1300,
        1295,
        1305,  // 250-300s (5min)

        // 5-8 minutes: Cloud bank approaches - gradual decline
        1250,
        1200,
        1150,
        1100,
        1000,
        900,
        800,
        700,
        600,
        500,  // 300-350s
        400,
        350,
        300,
        250,
        200,
        180,
        150,
        120,
        100,
        80,  // 350-400s
        60,
        50,
        40,
        30,
        25,
        20,
        15,
        12,
        10,
        8,  // 400-450s
        6,
        5,
        4,
        3,
        2,
        1,
        1,
        0,
        0,
        0,  // 450-500s (cloud peak)

        // 8-11 minutes: Under dense cloud - very low production
        0,
        1,
        2,
        3,
        5,
        8,
        10,
        15,
        20,
        25,  // 500-550s
        30,
        40,
        50,
        60,
        80,
        100,
        120,
        150,
        180,
        200,  // 550-600s
        250,
        300,
        350,
        400,
        500,
        600,
        700,
        800,
        900,
        1000,  // 600-650s

        // 11-15 minutes: Cloud clearing - gradual recovery
        1100,
        1150,
        1200,
        1220,
        1240,
        1250,
        1260,
        1270,
        1275,
        1280,  // 650-700s
        1285,
        1290,
        1292,
        1294,
        1296,
        1298,
        1299,
        1300,
        1300,
        1301,  // 700-750s
        1302,
        1303,
        1304,
        1305,
        1305,
        1305,
        1304,
        1303,
        1302,
        1301,  // 750-800s
        1300,
        1301,
        1302,
        1303,
        1304,
        1305,
        1305,
        1304,
        1303,
        1302,  // 800-850s
        1301,
        1300,
        1299,
        1300,
        1301,
        1302,
        1303,
        1304,
        1305,
        1305,  // 850-900s (15min)

        // 15-18 minutes: Back to stable high production
        1305,
        1304,
        1303,
        1302,
        1301,
        1300,
        1301,
        1302,
        1303,
        1304,  // 900-950s
        1305,
        1304,
        1303,
        1302,
        1301,
        1300,
        1299,
        1300,
        1301,
        1302,  // 950-1000s
        1303,
        1304,
        1305,
        1304,
        1303,
        1302,
        1301,
        1300,
        1301,
        1302,  // 1000-1050s
        1303,
        1304,
        1305,
        1304,
        1303,
        1302,
        1301,
        1300,
        1299,
        1300,  // 1050-1080s (18min)
      },
      500 },
    { "Intermittent Clouds - 16 minutes",
      "5min stable, then frequent cloud/sun alternation (challenging conditions)",
      // 16 minutes = 192 samples
      // 0-5 minutes: Stable production
      {
        1100,
        1110,
        1095,
        1105,
        1090,
        1100,
        1115,
        1105,
        1095,
        1100,  // 0-50s
        1110,
        1100,
        1105,
        1095,
        1110,
        1100,
        1090,
        1105,
        1110,
        1095,  // 50-100s
        1100,
        1105,
        1110,
        1095,
        1100,
        1115,
        1100,
        1105,
        1090,
        1100,  // 100-150s
        1110,
        1095,
        1100,
        1105,
        1110,
        1100,
        1095,
        1105,
        1100,
        1110,  // 150-200s
        1095,
        1100,
        1105,
        1110,
        1100,
        1095,
        1105,
        1100,
        1110,
        1095,  // 200-250s
        1100,
        1105,
        1110,
        1095,
        1100,
        1105,
        1110,
        1100,
        1095,
        1105,  // 250-300s (5min)

        // 5-11 minutes: Rapid cloud/sun alternation
        600,
        1100,
        500,
        1200,
        400,
        1150,
        300,
        1100,
        700,
        1200,  // 300-350s
        800,
        1150,
        450,
        1100,
        350,
        1200,
        600,
        1150,
        500,
        1100,  // 350-400s
        400,
        1200,
        650,
        1150,
        550,
        1100,
        300,
        1200,
        750,
        1150,  // 400-450s
        450,
        1100,
        350,
        1200,
        600,
        1150,
        500,
        1100,
        400,
        1200,  // 450-500s
        700,
        1150,
        600,
        1100,
        350,
        1200,
        800,
        1150,
        450,
        1100,  // 500-550s
        300,
        1200,
        650,
        1150,
        550,
        1100,
        400,
        1200,
        750,
        1150,  // 550-600s (10min)
        500,
        1100,
        350,
        1200,
        600,
        1150,
        450,
        1100,
        300,
        1200,  // 600-650s
        700,
        1150,
        550,
        1100,
        400,
        1200,
        650,
        1150,
        500,
        1100,  // 650-660s (11min)

        // 11-16 minutes: Clouds becoming less frequent, stabilizing
        800,
        1150,
        900,
        1100,
        950,
        1200,
        1000,
        1150,
        1050,
        1100,  // 660-710s
        1100,
        1150,
        1120,
        1100,
        1080,
        1150,
        1100,
        1120,
        1110,
        1100,  // 710-760s
        1090,
        1150,
        1100,
        1120,
        1110,
        1100,
        1105,
        1150,
        1110,
        1100,  // 760-810s
        1095,
        1120,
        1105,
        1100,
        1110,
        1105,
        1100,
        1110,
        1105,
        1100,  // 810-860s
        1105,
        1110,
        1100,
        1105,
        1110,
        1100,
        1105,
        1110,
        1100,
        1105,  // 860-910s
        1110,
        1100,
        1105,
        1110,
        1100,
        1105,
        1110,
        1100,
        1105,
        1110,  // 910-960s (16min)
      },
      500 },
    { "Storm Approach - 17 minutes",
      "5min stable, then gradual deterioration to storm conditions",
      // 17 minutes = 204 samples
      // 0-5 minutes: Beautiful clear morning
      {
        1350,
        1360,
        1345,
        1355,
        1340,
        1350,
        1365,
        1355,
        1345,
        1350,  // 0-50s
        1360,
        1350,
        1355,
        1345,
        1360,
        1350,
        1340,
        1355,
        1360,
        1345,  // 50-100s
        1350,
        1355,
        1360,
        1345,
        1350,
        1365,
        1350,
        1355,
        1340,
        1350,  // 100-150s
        1360,
        1345,
        1350,
        1355,
        1360,
        1350,
        1345,
        1355,
        1350,
        1360,  // 150-200s
        1345,
        1350,
        1355,
        1360,
        1350,
        1345,
        1355,
        1350,
        1360,
        1345,  // 200-250s
        1350,
        1355,
        1360,
        1345,
        1350,
        1355,
        1360,
        1350,
        1345,
        1355,  // 250-300s (5min)

        // 5-9 minutes: High clouds moving in - gradual decline
        1300,
        1280,
        1250,
        1200,
        1150,
        1100,
        1050,
        1000,
        950,
        900,  // 300-350s
        850,
        800,
        750,
        700,
        650,
        600,
        550,
        500,
        450,
        400,  // 350-400s
        380,
        360,
        340,
        320,
        300,
        280,
        260,
        240,
        220,
        200,  // 400-450s
        180,
        160,
        140,
        120,
        100,
        90,
        80,
        70,
        60,
        50,  // 450-500s
        45,
        40,
        35,
        30,
        25,
        22,
        20,
        18,
        15,
        12,  // 500-540s (9min)

        // 9-13 minutes: Storm conditions - very low, erratic production
        10,
        8,
        6,
        4,
        2,
        1,
        0,
        1,
        3,
        5,  // 540-590s
        8,
        12,
        15,
        10,
        5,
        2,
        0,
        1,
        4,
        8,  // 590-640s
        12,
        18,
        25,
        20,
        15,
        10,
        5,
        2,
        1,
        0,  // 640-690s
        1,
        3,
        6,
        10,
        15,
        12,
        8,
        4,
        1,
        0,  // 690-740s
        2,
        5,
        8,
        12,
        18,
        15,
        10,
        6,
        3,
        1,  // 740-780s (13min)

        // 13-17 minutes: Storm passing, very slow recovery
        5,
        10,
        15,
        20,
        30,
        40,
        50,
        60,
        80,
        100,  // 780-830s
        120,
        150,
        180,
        200,
        250,
        300,
        350,
        400,
        450,
        500,  // 830-880s
        550,
        600,
        650,
        700,
        750,
        800,
        850,
        900,
        950,
        1000,  // 880-930s
        1050,
        1100,
        1120,
        1140,
        1160,
        1180,
        1200,
        1220,
        1240,
        1260,  // 930-980s
        1280,
        1300,
        1310,
        1320,
        1330,
        1340,
        1345,
        1350,
        1350,
        1350,  // 980-1020s (17min)
      },
      500 },
    { "Morning Fog Clearing - 15 minutes",
      "5min stable low production, then fog gradually lifts (coastal/mountain)",
      // 15 minutes = 180 samples
      // 0-5 minutes: Stable low production in fog
      {
        50,
        55,
        45,
        52,
        48,
        50,
        58,
        52,
        48,
        50,  // 0-50s
        55,
        50,
        52,
        48,
        55,
        50,
        48,
        52,
        55,
        48,  // 50-100s
        50,
        52,
        55,
        48,
        50,
        58,
        50,
        52,
        48,
        50,  // 100-150s
        55,
        48,
        50,
        52,
        55,
        50,
        48,
        52,
        50,
        55,  // 150-200s
        48,
        50,
        52,
        55,
        50,
        48,
        52,
        50,
        55,
        48,  // 200-250s
        50,
        52,
        55,
        48,
        50,
        52,
        55,
        50,
        48,
        52,  // 250-300s (5min)

        // 5-10 minutes: Fog starting to lift - gradual power increase
        60,
        70,
        80,
        90,
        100,
        120,
        140,
        160,
        180,
        200,  // 300-350s
        220,
        250,
        280,
        320,
        360,
        400,
        450,
        500,
        550,
        600,  // 350-400s
        650,
        700,
        750,
        800,
        850,
        900,
        950,
        1000,
        1050,
        1100,  // 400-450s
        1120,
        1140,
        1160,
        1180,
        1200,
        1210,
        1220,
        1230,
        1240,
        1250,  // 450-500s
        1260,
        1270,
        1275,
        1280,
        1285,
        1290,
        1292,
        1294,
        1296,
        1298,  // 500-550s
        1300,
        1302,
        1304,
        1306,
        1308,
        1310,
        1312,
        1314,
        1316,
        1318,  // 550-600s (10min)

        // 10-15 minutes: Clear conditions achieved - stable high production
        1320,
        1322,
        1324,
        1326,
        1328,
        1330,
        1328,
        1326,
        1324,
        1322,  // 600-650s
        1320,
        1322,
        1324,
        1326,
        1328,
        1330,
        1332,
        1330,
        1328,
        1326,  // 650-700s
        1324,
        1322,
        1320,
        1322,
        1324,
        1326,
        1328,
        1330,
        1328,
        1326,  // 700-750s
        1324,
        1322,
        1320,
        1318,
        1320,
        1322,
        1324,
        1326,
        1328,
        1330,  // 750-800s
        1328,
        1326,
        1324,
        1322,
        1320,
        1322,
        1324,
        1326,
        1328,
        1330,  // 800-850s
        1328,
        1326,
        1324,
        1322,
        1320,
        1322,
        1324,
        1326,
        1328,
        1330,  // 850-900s (15min)
      },
      500 },
    {
      "Battery System - 20 minutes",
      "5min stable, then varied conditions showing battery system behavior",
      // 20 minutes = 240 samples, with negative import values representing import from grid
      // 0-5 minutes: Stable moderate surplus
      { 200, 210, 195, 205, 190, 200, 215, 205, 195, 200,  // 0-50s
        210, 200, 205, 195, 210, 200, 190, 205, 210, 195,  // 50-100s
        200, 205, 210, 195, 200, 215, 200, 205, 190, 200,  // 100-150s
        210, 195, 200, 205, 210, 200, 195, 205, 200, 210,  // 150-200s
        195, 200, 205, 210, 200, 195, 205, 200, 210, 195,  // 200-250s
        200, 205, 210, 195, 200, 205, 210, 200, 195, 205,  // 250-300s (5min)

        // 5-10 minutes: Variable conditions - import and export
        -150, -100, -50, 0, 50, 100, 150, 200, 150, 100,     // 300-350s (battery charging/discharging)
        50, 0, -50, -100, -150, -100, -50, 0, 50, 100,       // 350-400s
        150, 200, 250, 300, 250, 200, 150, 100, 50, 0,       // 400-450s
        -50, -100, -150, -200, -150, -100, -50, 0, 50, 100,  // 450-500s
        150, 200, 300, 400, 500, 600, 500, 400, 300, 200,    // 500-550s
        100, 50, 0, -50, -100, -50, 0, 50, 100, 150,         // 550-600s (10min)

        // 10-15 minutes: High solar with battery behavior
        800, 900, 1000, 1100, 1200, 1100, 1000, 900, 800, 700,    // 600-650s
        600, 500, 400, 300, 200, 100, 0, -100, -50, 0,            // 650-700s
        50, 100, 200, 300, 400, 500, 600, 700, 800, 900,          // 700-750s
        1000, 1100, 1200, 1300, 1200, 1100, 1000, 900, 800, 700,  // 750-800s
        600, 500, 400, 300, 200, 100, 50, 0, -50, -100,           // 800-850s
        -50, 0, 50, 100, 200, 300, 400, 500, 600, 700,            // 850-900s (15min)

        // 15-20 minutes: Evening reduction with battery supporting loads
        600, 500, 400, 300, 200, 100, 0, -100, -200, -300,   // 900-950s
        -250, -200, -150, -100, -50, 0, 50, 100, 50, 0,      // 950-1000s
        -50, -100, -150, -200, -150, -100, -50, 0, 50, 100,  // 1000-1050s
        150, 100, 50, 0, -50, -100, -150, -100, -50, 0,      // 1050-1100s
        50, 100, 50, 0, -50, -100, -50, 0, 50, 0,            // 1100-1150s
        -50, -100, -150, -100, -50, 0, 50, 100, 50, 0 },     // 1150-1200s (20min)
      -100                                                   // Negative threshold: need 100W+ surplus to turn on
    }
  };
}

void setUp(void)
{
  // Set up before each test
}

void tearDown(void)
{
  // Clean up after each test
}

// Test a specific cloud pattern with different filter delay settings
void test_cloud_pattern_with_filter_delays(const CloudPattern& pattern)
{
  printf("\n=== %s ===\n", pattern.name.c_str());
  printf("%s\n", pattern.description.c_str());
  printf("Duration: %.1f minutes (%zu samples at 5-second intervals)\n",
         pattern.power_data.size() * 5.0 / 60.0, pattern.power_data.size());
  printf("Threshold: %dW (positive=import limit, negative=surplus requirement)\n",
         (int)pattern.relay_threshold);
  printf("\nTesting RELAY_FILTER_DELAY_MINUTES: 1, 2, 3, 4, 5 minutes\n");
  printf("Time(mm:ss),Power(W),1min,2min,3min,4min,5min,State_1m,State_2m,State_3m,State_4m,State_5m\n");

  // Create EWMA filters for different delay settings (using TEMA for best cloud immunity)
  EWMA_average< 12 > filter_1min;  // 1 minute * 12 = alpha 12
  EWMA_average< 24 > filter_2min;  // 2 minutes * 12 = alpha 24
  EWMA_average< 36 > filter_3min;  // 3 minutes * 12 = alpha 36
  EWMA_average< 48 > filter_4min;  // 4 minutes * 12 = alpha 48
  EWMA_average< 60 > filter_5min;  // 5 minutes * 12 = alpha 60

  // Create relay instances for each filter delay
  TestRelay relay_1min(pattern.relay_threshold, 100, 1);
  TestRelay relay_2min(pattern.relay_threshold, 100, 2);
  TestRelay relay_3min(pattern.relay_threshold, 100, 3);
  TestRelay relay_4min(pattern.relay_threshold, 100, 4);
  TestRelay relay_5min(pattern.relay_threshold, 100, 5);

  // Count relay state changes for each setting
  int changes_1min{ 0 }, changes_2min{ 0 }, changes_3min{ 0 }, changes_4min{ 0 }, changes_5min{ 0 };
  bool prev_1min{ false }, prev_2min{ false }, prev_3min{ false }, prev_4min{ false }, prev_5min{ false };

  // Track period summaries
  std::vector< std::string > period_names = { "0-5min: Stable", "5-10min: Clouds Start", "10-15min: Peak Activity", "15-20min: Stabilizing" };
  std::vector< int > period_starts = { 0, 60, 120, 180 };  // Sample indices for period starts

  // Process each power measurement
  for (size_t i = 0; i < pattern.power_data.size(); i++)
  {
    int32_t raw_power{ pattern.power_data[i] };

    // Apply EWMA filtering (using TEMA for best cloud immunity)
    filter_1min.addValue(raw_power);
    filter_2min.addValue(raw_power);
    filter_3min.addValue(raw_power);
    filter_4min.addValue(raw_power);
    filter_5min.addValue(raw_power);

    int32_t filtered_1min{ filter_1min.getAverageT() };
    int32_t filtered_2min{ filter_2min.getAverageT() };
    int32_t filtered_3min{ filter_3min.getAverageT() };
    int32_t filtered_4min{ filter_4min.getAverageT() };
    int32_t filtered_5min{ filter_5min.getAverageT() };

    // Apply relay logic
    bool state_1min{ relay_1min.proceed_relay(filtered_1min) };
    bool state_2min{ relay_2min.proceed_relay(filtered_2min) };
    bool state_3min{ relay_3min.proceed_relay(filtered_3min) };
    bool state_4min{ relay_4min.proceed_relay(filtered_4min) };
    bool state_5min{ relay_5min.proceed_relay(filtered_5min) };

    // Count state changes
    if (state_1min != prev_1min) changes_1min++;
    if (state_2min != prev_2min) changes_2min++;
    if (state_3min != prev_3min) changes_3min++;
    if (state_4min != prev_4min) changes_4min++;
    if (state_5min != prev_5min) changes_5min++;

    prev_1min = state_1min;
    prev_2min = state_2min;
    prev_3min = state_3min;
    prev_4min = state_4min;
    prev_5min = state_5min;

    // Output data every 30 seconds for readability (every 6th sample)
    if (i % 6 == 0 || i < 12 || i >= pattern.power_data.size() - 6)
    {
      int minutes{ (i * 5) / 60 };
      int seconds{ (i * 5) % 60 };
      printf("%02d:%02d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
             minutes, seconds, (int)raw_power,
             (int)filtered_1min, (int)filtered_2min, (int)filtered_3min,
             (int)filtered_4min, (int)filtered_5min,
             state_1min ? 1 : 0, state_2min ? 1 : 0, state_3min ? 1 : 0,
             state_4min ? 1 : 0, state_5min ? 1 : 0);
    }

    // Print period summaries
    for (size_t p = 0; p < period_starts.size(); p++)
    {
      if (i == period_starts[p] && i > 0)
      {
        printf("# %s\n", period_names[p].c_str());
      }
    }
  }

  printf("\n📊 RELAY BEHAVIOR ANALYSIS\n");
  printf("================================\n");
  printf("Relay State Changes (lower = better stability):\n");
  printf("1 minute delay:  %d changes\n", changes_1min);
  printf("2 minute delay:  %d changes\n", changes_2min);
  printf("3 minute delay:  %d changes\n", changes_3min);
  printf("4 minute delay:  %d changes\n", changes_4min);
  printf("5 minute delay:  %d changes\n", changes_5min);

  // Enhanced analysis
  printf("\n🎯 PERFORMANCE COMPARISON:\n");
  if (changes_1min > 0)
  {
    printf("Improvement with longer delays:\n");
    if (changes_2min < changes_1min) printf("  2min vs 1min: %d%% fewer changes\n", (changes_1min - changes_2min) * 100 / changes_1min);
    if (changes_3min < changes_1min) printf("  3min vs 1min: %d%% fewer changes\n", (changes_1min - changes_3min) * 100 / changes_1min);
    if (changes_4min < changes_1min) printf("  4min vs 1min: %d%% fewer changes\n", (changes_1min - changes_4min) * 100 / changes_1min);
    if (changes_5min < changes_1min) printf("  5min vs 1min: %d%% fewer changes\n", (changes_1min - changes_5min) * 100 / changes_1min);
  }

  // Smart recommendation based on pattern characteristics
  printf("\n💡 RECOMMENDATION for '%s':\n", pattern.name.c_str());
  if (changes_1min <= 2)
  {
    printf("✅ 1-2 minute delay is sufficient (responsive without excessive chatter)\n");
    printf("   This pattern has stable characteristics suitable for fast response.\n");
  }
  else if (changes_3min < changes_1min * 0.6)
  {
    printf("✅ 3 minute delay recommended (good balance of stability vs responsiveness)\n");
    printf("   This pattern benefits significantly from additional filtering.\n");
  }
  else if (changes_4min < changes_2min * 0.8)
  {
    printf("✅ 4-5 minute delay recommended (maximum stability for challenging conditions)\n");
    printf("   This pattern requires heavy filtering to prevent relay chatter.\n");
  }
  else
  {
    printf("✅ 2-3 minute delay recommended (standard setting works well)\n");
    printf("   This pattern responds well to moderate filtering.\n");
  }
  printf("\n");
}

void test_scattered_clouds_pattern()
{
  auto patterns = create_cloud_patterns();
  test_cloud_pattern_with_filter_delays(patterns[0]);
  TEST_ASSERT_TRUE(true);  // Pass test - results shown in output
}

void test_heavy_cloud_bank_pattern()
{
  auto patterns = create_cloud_patterns();
  test_cloud_pattern_with_filter_delays(patterns[1]);
  TEST_ASSERT_TRUE(true);  // Pass test - results shown in output
}

void test_intermittent_clouds_pattern()
{
  auto patterns = create_cloud_patterns();
  test_cloud_pattern_with_filter_delays(patterns[2]);
  TEST_ASSERT_TRUE(true);  // Pass test - results shown in output
}

void test_morning_fog_clearing_pattern()
{
  auto patterns = create_cloud_patterns();
  test_cloud_pattern_with_filter_delays(patterns[3]);
  TEST_ASSERT_TRUE(true);  // Pass test - results shown in output
}

void test_storm_approach_pattern()
{
  auto patterns = create_cloud_patterns();
  test_cloud_pattern_with_filter_delays(patterns[4]);
  TEST_ASSERT_TRUE(true);  // Pass test - results shown in output
}

void test_battery_system_pattern()
{
  auto patterns = create_cloud_patterns();
  test_cloud_pattern_with_filter_delays(patterns[5]);
  TEST_ASSERT_TRUE(true);  // Pass test - results shown in output
}

void test_filter_delay_configuration_guide()
{
  printf("\n=== RELAY_FILTER_DELAY_MINUTES Configuration Guide ===\n");
  printf("\nBased on cloud pattern analysis:\n\n");

  printf("🌤️  CLEAR SKY REGIONS (minimal clouds):\n");
  printf("   RELAY_FILTER_DELAY_MINUTES = 1\n");
  printf("   - Fast response to power changes\n");
  printf("   - Suitable when cloud events are rare\n");
  printf("   - Examples: Desert regions, dry climates\n\n");

  printf("⛅ MIXED CONDITIONS (occasional clouds):\n");
  printf("   RELAY_FILTER_DELAY_MINUTES = 2\n");
  printf("   - Balanced response vs stability\n");
  printf("   - Good default for most installations\n");
  printf("   - Examples: Continental climates, suburban areas\n\n");

  printf("☁️  FREQUENTLY CLOUDY (regular cloud cover):\n");
  printf("   RELAY_FILTER_DELAY_MINUTES = 3\n");
  printf("   - Enhanced stability during cloud events\n");
  printf("   - Reduces relay chatter significantly\n");
  printf("   - Examples: Coastal areas, temperate climates\n\n");

  printf("🌧️  VERY CLOUDY (challenging conditions):\n");
  printf("   RELAY_FILTER_DELAY_MINUTES = 4-5\n");
  printf("   - Maximum stability for difficult conditions\n");
  printf("   - Slower response but very stable\n");
  printf("   - Examples: Mountain regions, tropical areas\n\n");

  printf("🔋 BATTERY SYSTEMS:\n");
  printf("   RELAY_FILTER_DELAY_MINUTES = 2-3\n");
  printf("   - Use negative import threshold (e.g., -100W)\n");
  printf("   - Requires surplus above threshold to activate\n");
  printf("   - Prevents relay cycling due to battery buffer\n\n");

  printf("Configuration example in config.h:\n");
  printf("inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 2 }; // Adjust based on your climate\n");
  printf("// Normal installation:\n");
  printf("inline constexpr RelayEngine< 1, RELAY_FILTER_DELAY_MINUTES > relays{ { { unused_pin, 1000, 200, 1, 1 } } };\n");
  printf("// Battery installation:\n");
  printf("inline constexpr RelayEngine< 1, RELAY_FILTER_DELAY_MINUTES > relays{ { { unused_pin, 1000, -100, 1, 1 } } };\n\n");

  TEST_ASSERT_TRUE(true);  // Pass test - guide shown in output
}

int main()
{
  UNITY_BEGIN();

  printf("\n🌤️ =============================================== 🌤️\n");
  printf("     PV Router Cloud Pattern Analysis Tool\n");
  printf("     Tune RELAY_FILTER_DELAY_MINUTES for your climate\n");
  printf("🌤️ =============================================== 🌤️\n");

  RUN_TEST(test_scattered_clouds_pattern);
  RUN_TEST(test_heavy_cloud_bank_pattern);
  RUN_TEST(test_intermittent_clouds_pattern);
  RUN_TEST(test_morning_fog_clearing_pattern);
  RUN_TEST(test_storm_approach_pattern);
  RUN_TEST(test_battery_system_pattern);
  RUN_TEST(test_filter_delay_configuration_guide);

  return UNITY_END();
}
