
#include <Arduino.h>

#include "movingAvg.h"

const int nb_of_interation_per_pass = 500;
const int nb_of_pass = 10;

unsigned long initial_time = 0;
unsigned long final_time = 0;

float duration_dummy_loop = 0;
float duration = 0;
float duration_sum = 0;


// All variables used for calculations are declared globally and volatile to minimize
// any possible compiler optimisation when performing the same operation multiple times.

volatile int dummy = 0;

volatile float float_1 = 0;
volatile float float_2 = 0;
volatile float float_3 = 0;

long long_1 = 0;
volatile long long_2 = 0;
volatile long long_3 = 0;

volatile int int_1 = 0;
volatile int int_2 = 0;
volatile int int_3 = 0;

volatile byte byte_1 = 0;
volatile byte byte_2 = 0;
volatile byte byte_3 = 0;

volatile boolean bool_1 = 0;
volatile boolean bool_2 = 0;
volatile boolean bool_3 = 0;

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
public:
  void addValue(int32_t input) {
    w = w - x + input;
    x = w >> round_up_to_power_of_2(A);
  }

  auto getAverage() const {
    return x;
  }

private:
  int32_t w{ 0 };
  int32_t x{ 0 };
};

movingAvg< int32_t, 10, 12 > sliding_Average;
EWMA_average<120> ewma_average;

void setup() {

  Serial.begin(115200);
  Serial.println("Setup ***");
}

void loop() {

  for (int j = 0; j < nb_of_pass; j++) {

    // STEP 1: We first calculate the time taken to run a dummy FOR loop to measure the overhead cause by the execution of the loop.
    initial_time = micros();
    for (int i = 0; i < nb_of_interation_per_pass; i++) {
      dummy++;  // A dummy instruction is introduced here. If not, the compiler is smart enough to just skip the loop entirely...
    }
    final_time = micros();
    duration_dummy_loop = float(final_time - initial_time) / nb_of_interation_per_pass;  // The average duration of a dummy loop is calculated
    dummy = 0;

    // STEP 2 (optional): Pick some relevant random numbers to test the command under random conditions. Make sure to pick numbers appropriate for your command (e.g. no negative number for the command "sqrt()")
    randomSeed(micros() * analogRead(0));
    long_1 = random(-36000, 36000);

    // STEP 3: Calculation of the time taken to run the dummy FOR loop and the command to test.
    initial_time = micros();
    for (int i = 0; i < nb_of_interation_per_pass; i++) {
      dummy++;  // The dummy instruction is also performed here so that we can remove the effect of the dummy FOR loop accurately.
                // **************** PUT YOUR COMMAND TO TEST HERE ********************
      long_1 = sin(i) * 1000;

      sliding_Average.addValue(long_1);
      ewma_average.addValue(long_1);

      // **************** PUT YOUR COMMAND TO TEST HERE ********************
    }
    final_time = micros();

    // STEP 4: Calculation of the time taken to run only the target command.
    duration = float(final_time - initial_time) / nb_of_interation_per_pass - duration_dummy_loop;
    duration_sum += duration;
    dummy = 0;

    Serial.print(sliding_Average.getAverage());
    Serial.print(" - ");
    Serial.print(ewma_average.getAverage());
    Serial.print(" - ");
    Serial.print(j);
    Serial.print(". ");
    print_result(duration);

    sliding_Average.clear();
  }

  Serial.println();
  Serial.println("********* FINAL AVERAGED VALUE *********** ");
  print_result(duration_sum / nb_of_pass);
  Serial.println("****************************************** ");
  Serial.println();
  //for (uint8_t idx = 0; idx < sliding_Average.getSize(); ++idx) {
  //  Serial.println(sliding_Average.getElement(idx));
  //}
  //Serial.println();
  duration_sum = 0;
  delay(2000);
}

void print_result(float value_to_print) {
  Serial.print("Time to execute command: ");
  Serial.print("\t");
  Serial.print(value_to_print, 3);
  Serial.print(" us");
  Serial.print("\t");
  Serial.print(round(value_to_print * 16));
  Serial.println(" cycles");
}