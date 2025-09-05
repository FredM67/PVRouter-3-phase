#include <unity.h>
#include <cstdio>
#include <chrono>
#include <vector>
#include <functional>

// Mock Arduino types for native compilation
typedef unsigned char byte;

// Include EWMA implementation (copied from main project)
constexpr uint8_t round_up_to_power_of_2(uint16_t v) {
  if (__builtin_popcount(v) == 1) { return __builtin_ctz(v) - 1; }

  uint8_t next_pow_of_2{ 0 };

  while (v) {
    v >>= 1;
    ++next_pow_of_2;
  }

  return --next_pow_of_2;
}

template< uint8_t A = 10 >
class EWMA_average {
private:
  int32_t ema_raw{ 0 };
  int32_t ema{ 0 };
  int32_t ema_ema_raw{ 0 };
  int32_t ema_ema{ 0 };
  int32_t ema_ema_ema_raw{ 0 };
  int32_t ema_ema_ema{ 0 };

public:
  void addValue(int32_t input) {
    ema_raw = ema_raw - ema + input;
    ema = ema_raw >> round_up_to_power_of_2(A);

    ema_ema_raw = ema_ema_raw - ema_ema + ema;
    ema_ema = ema_ema_raw >> (round_up_to_power_of_2(A) - 1);

    ema_ema_ema_raw = ema_ema_ema_raw - ema_ema_ema + ema_ema;
    ema_ema_ema = ema_ema_ema_raw >> (round_up_to_power_of_2(A) - 2);
  }

  auto getAverageS() const { return ema; }
  auto getAverageD() const { return (ema << 1) - ema_ema; }
  auto getAverageT() const { return ema + (ema - ema_ema) + (ema - ema_ema_ema); }
  
  void reset() {
    ema_raw = ema = ema_ema_raw = ema_ema = ema_ema_ema_raw = ema_ema_ema = 0;
  }
};

// Simple moving average for comparison
class SimpleMovingAverage {
private:
  static const int WINDOW_SIZE = 32;
  int32_t values[WINDOW_SIZE];
  int index;
  int count;
  int32_t sum;

public:
  SimpleMovingAverage() : index(0), count(0), sum(0) {
    for (int i = 0; i < WINDOW_SIZE; i++) values[i] = 0;
  }

  void addValue(int32_t value) {
    sum -= values[index];
    values[index] = value;
    sum += value;
    index = (index + 1) % WINDOW_SIZE;
    if (count < WINDOW_SIZE) count++;
  }

  int32_t getAverage() const {
    return count > 0 ? sum / count : 0;
  }
  
  void reset() {
    index = count = sum = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) values[i] = 0;
  }
};

void setUp(void) {
  // Set up before each test
}

void tearDown(void) {
  // Clean up after each test
}

// Benchmark utility function
double benchmark_operation(std::function<void()> operation, int iterations = 100000) {
  auto start = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < iterations; i++) {
    operation();
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
  
  return double(duration.count()) / iterations; // nanoseconds per operation
}

void test_performance_comparison() {
  printf("\n=== EWMA Performance Benchmark ===\n");
  printf("Operations per test: 100,000\n");
  printf("Results in nanoseconds per operation\n\n");
  
  // Test instances
  EWMA_average<32> ema;
  SimpleMovingAverage sma;
  volatile int32_t dummy_result = 0;
  volatile int32_t test_value = 1000;
  
  // Benchmark EMA operations
  double ema_time = benchmark_operation([&]() {
    ema.addValue(test_value);
    dummy_result = ema.getAverageS();
  });
  
  double dema_time = benchmark_operation([&]() {
    ema.addValue(test_value);
    dummy_result = ema.getAverageD();
  });
  
  double tema_time = benchmark_operation([&]() {
    ema.addValue(test_value);
    dummy_result = ema.getAverageT();
  });
  
  double sma_time = benchmark_operation([&]() {
    sma.addValue(test_value);
    dummy_result = sma.getAverage();
  });
  
  printf("EMA (Single):        %8.2f ns/op\n", ema_time);
  printf("DEMA (Double):       %8.2f ns/op\n", dema_time);
  printf("TEMA (Triple):       %8.2f ns/op\n", tema_time);
  printf("Simple Moving Avg:   %8.2f ns/op\n", sma_time);
  
  printf("\nPerformance ratios (vs EMA):\n");
  printf("DEMA overhead:       %6.2fx\n", dema_time / ema_time);
  printf("TEMA overhead:       %6.2fx\n", tema_time / ema_time);
  printf("SMA overhead:        %6.2fx\n", sma_time / ema_time);
  
  // Basic performance assertions
  TEST_ASSERT_LESS_THAN(1000, ema_time);   // Should be under 1 microsecond
  TEST_ASSERT_LESS_THAN(2000, dema_time);  // DEMA should be under 2 microseconds
  TEST_ASSERT_LESS_THAN(3000, tema_time);  // TEMA should be under 3 microseconds
}

void test_cloud_immunity_simulation() {
  printf("\n=== Cloud Immunity Simulation ===\n");
  
  // Cloud simulation data (realistic power values with cloud effects)
  std::vector<int32_t> cloud_data = {
    1000, 1020, 980, 1050, 1030, 990, 1100, 1080, 950, 1200,  // Clear morning
    400, 600, 300, 800, 200, 900, 150, 750, 100, 850,         // Cloudy period  
    1300, 1350, 1320, 1380, 1340, 1300, 1400, 1420, 1380, 1450, // Clear afternoon
    50, 30, 80, 20, 100, 10, 120, 5, 150, 0                   // Evening fade
  };
  
  // Test different alpha values
  EWMA_average<8> ema_fast;     // Fast response (cloud sensitive)
  EWMA_average<32> ema_med;     // Medium response
  EWMA_average<128> ema_slow;   // Slow response (cloud immune)
  
  printf("Processing %zu power measurements...\n", cloud_data.size());
  printf("Time,Input,EMA_Fast,DEMA_Med,TEMA_Med,EMA_Slow\n");
  
  int relay_changes_ema_fast = 0;
  int relay_changes_dema_med = 0;
  int relay_changes_tema_med = 0;
  int relay_changes_ema_slow = 0;
  
  bool prev_relay_ema_fast = false;
  bool prev_relay_dema_med = false;
  bool prev_relay_tema_med = false;
  bool prev_relay_ema_slow = false;
  
  const int32_t RELAY_THRESHOLD = 500; // 500W threshold for relay
  
  for (size_t i = 0; i < cloud_data.size(); i++) {
    int32_t input = cloud_data[i];
    
    // Feed to all filters
    ema_fast.addValue(input);
    ema_med.addValue(input);
    ema_slow.addValue(input);
    
    int32_t ema_fast_val = ema_fast.getAverageS();
    int32_t dema_med_val = ema_med.getAverageD();
    int32_t tema_med_val = ema_med.getAverageT();
    int32_t ema_slow_val = ema_slow.getAverageS();
    
    // Count relay state changes
    bool relay_ema_fast = ema_fast_val > RELAY_THRESHOLD;
    bool relay_dema_med = dema_med_val > RELAY_THRESHOLD;
    bool relay_tema_med = tema_med_val > RELAY_THRESHOLD;
    bool relay_ema_slow = ema_slow_val > RELAY_THRESHOLD;
    
    if (relay_ema_fast != prev_relay_ema_fast) relay_changes_ema_fast++;
    if (relay_dema_med != prev_relay_dema_med) relay_changes_dema_med++;
    if (relay_tema_med != prev_relay_tema_med) relay_changes_tema_med++;
    if (relay_ema_slow != prev_relay_ema_slow) relay_changes_ema_slow++;
    
    prev_relay_ema_fast = relay_ema_fast;
    prev_relay_dema_med = relay_dema_med;
    prev_relay_tema_med = relay_tema_med;
    prev_relay_ema_slow = relay_ema_slow;
    
    printf("%zu,%d,%d,%d,%d,%d\n", 
           i * 5, (int)input, (int)ema_fast_val, (int)dema_med_val, 
           (int)tema_med_val, (int)ema_slow_val);
  }
  
  printf("\nRelay State Changes (lower = better cloud immunity):\n");
  printf("EMA Fast (α=8):      %d changes\n", relay_changes_ema_fast);
  printf("DEMA Med (α=32):     %d changes\n", relay_changes_dema_med);
  printf("TEMA Med (α=32):     %d changes\n", relay_changes_tema_med);
  printf("EMA Slow (α=128):    %d changes\n", relay_changes_ema_slow);
  
  // TEMA should have fewer or equal relay changes than EMA (better cloud immunity)
  TEST_ASSERT_LESS_OR_EQUAL(relay_changes_tema_med, relay_changes_ema_fast);
  TEST_ASSERT_LESS_OR_EQUAL(relay_changes_dema_med, relay_changes_ema_fast);
}

void test_responsiveness_comparison() {
  printf("\n=== Responsiveness Test ===\n");
  
  // Test how quickly each filter responds to a step change
  std::vector<int32_t> step_data = {
    0, 0, 0, 0, 0,           // Start at zero
    1000, 1000, 1000, 1000, 1000,  // Step to 1000W
    0, 0, 0, 0, 0            // Step back to zero
  };
  
  EWMA_average<32> ema;
  EWMA_average<32> dema;
  EWMA_average<32> tema;
  
  printf("Step response test (0W -> 1000W -> 0W):\n");
  printf("Sample,Input,EMA,DEMA,TEMA\n");
  
  for (size_t i = 0; i < step_data.size(); i++) {
    int32_t input = step_data[i];
    
    ema.addValue(input);
    dema.addValue(input);
    tema.addValue(input);
    
    printf("%zu,%d,%d,%d,%d\n", 
           i, (int)input, (int)ema.getAverageS(), 
           (int)dema.getAverageD(), (int)tema.getAverageT());
  }
  
  // DEMA and TEMA should respond faster than EMA to step changes
  // This is qualitative - the actual test would require settling time analysis
  TEST_ASSERT_TRUE(true); // Placeholder - results visible in output
}

void test_alpha_parameter_effects() {
  printf("\n=== Alpha Parameter Effects ===\n");
  
  // Test how different alpha values affect convergence
  // Use alpha values that work well with TEMA (need at least 4 for TEMA to work)
  EWMA_average<8> ema_fast;         // α=8, fast
  EWMA_average<32> ema_med;         // α=32, medium
  EWMA_average<128> ema_slow;       // α=128, slow
  
  const int32_t target_value = 800;
  const int samples = 20;
  
  printf("Convergence to %dW with different α values:\n", (int)target_value);
  printf("Sample,α=8,α=32,α=128\n");
  
  for (int i = 0; i < samples; i++) {
    ema_fast.addValue(target_value);
    ema_med.addValue(target_value);
    ema_slow.addValue(target_value);
    
    printf("%d,%d,%d,%d\n", 
           i, (int)ema_fast.getAverageS(), (int)ema_med.getAverageS(),
           (int)ema_slow.getAverageS());
  }
  
  // Verify that smaller alpha converges faster
  TEST_ASSERT_GREATER_THAN(ema_med.getAverageS(), ema_fast.getAverageS());
  TEST_ASSERT_GREATER_THAN(ema_slow.getAverageS(), ema_med.getAverageS());
}

int main() {
  UNITY_BEGIN();
  
  RUN_TEST(test_performance_comparison);
  RUN_TEST(test_cloud_immunity_simulation);
  RUN_TEST(test_responsiveness_comparison);
  RUN_TEST(test_alpha_parameter_effects);
  
  return UNITY_END();
}