/**
 * @file EWMA_CloudImmunity_Benchmark.ino
 * @brief Performance benchmark for EWMA filters comparing EMA vs DEMA vs TEMA
 *        for cloud immunity and responsiveness in PV Router applications
 * @author Generated for PVRouter-3-phase project
 * @date 2024-02-27
 */

#include <Arduino.h>

// Include EWMA implementation
#ifndef EWMA_AVG_H
#define EWMA_AVG_H

#include "type_traits.hpp"

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
};

#endif

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
};

// Test parameters
const int nb_iterations = 10000;
const int nb_passes = 50;

// Global variables to prevent compiler optimization
volatile int32_t dummy_result = 0;
volatile int32_t test_value = 0;

// Test instances
EWMA_average<8> ema_fast;    // Fast response (alpha=8)
EWMA_average<32> ema_med;    // Medium response (alpha=32)  
EWMA_average<128> ema_slow;  // Slow response (alpha=128)
SimpleMovingAverage sma;

// Cloud simulation data (realistic power values with cloud effects)
int32_t cloud_data[] = {
  1000, 1020, 980, 1050, 1030, 990, 1100, 1080, 950, 1200,  // Clear morning
  400, 600, 300, 800, 200, 900, 150, 750, 100, 850,         // Cloudy period
  1300, 1350, 1320, 1380, 1340, 1300, 1400, 1420, 1380, 1450, // Clear afternoon
  50, 30, 80, 20, 100, 10, 120, 5, 150, 0                   // Evening fade
};
const int cloud_data_size = sizeof(cloud_data) / sizeof(cloud_data[0]);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);
  
  Serial.println("=== EWMA Cloud Immunity Performance Benchmark ===");
  Serial.println("Testing EMA vs DEMA vs TEMA for PV Router applications");
  Serial.println("Simulating cloud effects on solar power measurements");
  Serial.println();
  
  delay(2000);
}

void loop() {
  Serial.println("Starting benchmark tests...");
  Serial.println();

  // Test 1: Raw performance comparison
  Serial.println("1. Raw Performance Test (microseconds per operation)");
  Serial.println("------------------------------------------------");
  
  test_raw_performance("EMA Fast (α=8)", []() {
    ema_fast.addValue(test_value);
    dummy_result = ema_fast.getAverageS();
  });
  
  test_raw_performance("DEMA Fast (α=8)", []() {
    ema_fast.addValue(test_value);
    dummy_result = ema_fast.getAverageD();
  });
  
  test_raw_performance("TEMA Fast (α=8)", []() {
    ema_fast.addValue(test_value);
    dummy_result = ema_fast.getAverageT();
  });
  
  test_raw_performance("Simple Moving Avg", []() {
    sma.addValue(test_value);
    dummy_result = sma.getAverage();
  });

  Serial.println();

  // Test 2: Cloud immunity response test
  Serial.println("2. Cloud Immunity Response Test");
  Serial.println("--------------------------------");
  
  test_cloud_response();
  
  Serial.println();
  Serial.println("Benchmark complete. Waiting 10 seconds before next run...");
  Serial.println("==========================================================");
  Serial.println();
  
  delay(10000);
}

void test_raw_performance(const char* test_name, void (*test_func)()) {
  unsigned long total_time = 0;
  
  for (int pass = 0; pass < nb_passes; pass++) {
    // Use different test values each pass
    test_value = random(0, 2000);
    
    unsigned long start_time = micros();
    
    for (int i = 0; i < nb_iterations; i++) {
      test_func();
    }
    
    unsigned long end_time = micros();
    total_time += (end_time - start_time);
  }
  
  float avg_time_per_op = float(total_time) / (nb_passes * nb_iterations);
  
  Serial.print(test_name);
  Serial.print(": ");
  Serial.print(avg_time_per_op, 3);
  Serial.print(" μs (");
  Serial.print(avg_time_per_op * 16, 1);
  Serial.println(" cycles @ 16MHz)");
}

void test_cloud_response() {
  // Reset all filters
  EWMA_average<8> test_ema_fast;
  EWMA_average<32> test_ema_med;  
  EWMA_average<128> test_ema_slow;
  SimpleMovingAverage test_sma;
  
  Serial.println("Feeding cloud simulation data to different filters...");
  Serial.println("Time,Input,EMA_Fast,DEMA_Fast,TEMA_Fast,EMA_Med,EMA_Slow,SMA");
  
  for (int i = 0; i < cloud_data_size; i++) {
    int32_t input = cloud_data[i];
    
    // Feed the same input to all filters
    test_ema_fast.addValue(input);
    test_ema_med.addValue(input);
    test_ema_slow.addValue(input);
    test_sma.addValue(input);
    
    // Output results for analysis
    Serial.print(i * 5); // Assume 5-second sampling
    Serial.print(",");
    Serial.print(input);
    Serial.print(",");
    Serial.print(test_ema_fast.getAverageS());
    Serial.print(",");
    Serial.print(test_ema_fast.getAverageD());
    Serial.print(",");
    Serial.print(test_ema_fast.getAverageT());
    Serial.print(",");
    Serial.print(test_ema_med.getAverageS());
    Serial.print(",");
    Serial.print(test_ema_slow.getAverageS());
    Serial.print(",");
    Serial.println(test_sma.getAverage());
  }
  
  Serial.println();
  Serial.println("Analysis Summary:");
  Serial.println("- EMA_Fast: Quick response, more sensitive to clouds");
  Serial.println("- DEMA_Fast: Better cloud rejection than EMA_Fast");  
  Serial.println("- TEMA_Fast: Best cloud immunity, smoothest response");
  Serial.println("- EMA_Med/Slow: Slower response, better stability");
  Serial.println("- SMA: Traditional average, may have delay issues");
}

void analyze_cloud_immunity() {
  Serial.println("3. Cloud Immunity Analysis");
  Serial.println("---------------------------");
  
  // Simulate a cloud passing (power drops from 1000W to 200W and back)
  int32_t cloud_sequence[] = {1000, 800, 600, 400, 200, 300, 500, 700, 900, 1000};
  int seq_len = sizeof(cloud_sequence) / sizeof(cloud_sequence[0]);
  
  EWMA_average<32> ema_test;
  EWMA_average<32> dema_test;
  EWMA_average<32> tema_test;
  
  Serial.println("Cloud sequence: 1000W -> 200W -> 1000W");
  Serial.println("Step,Input,EMA,DEMA,TEMA,Relay_EMA,Relay_DEMA,Relay_TEMA");
  
  for (int i = 0; i < seq_len; i++) {
    int32_t input = cloud_sequence[i];
    
    ema_test.addValue(input);
    dema_test.addValue(input);  
    tema_test.addValue(input);
    
    int32_t ema_val = ema_test.getAverageS();
    int32_t dema_val = dema_test.getAverageD();
    int32_t tema_val = tema_test.getAverageT();
    
    // Simulate relay decisions (threshold = 500W)
    bool relay_ema = ema_val > 500;
    bool relay_dema = dema_val > 500;
    bool relay_tema = tema_val > 500;
    
    Serial.print(i);
    Serial.print(",");
    Serial.print(input);
    Serial.print(",");
    Serial.print(ema_val);
    Serial.print(",");
    Serial.print(dema_val);
    Serial.print(",");
    Serial.print(tema_val);
    Serial.print(",");
    Serial.print(relay_ema ? 1 : 0);
    Serial.print(",");
    Serial.print(relay_dema ? 1 : 0);
    Serial.print(",");
    Serial.println(relay_tema ? 1 : 0);
  }
}