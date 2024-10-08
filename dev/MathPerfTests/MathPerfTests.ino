
#include <Arduino.h>

#include "movingAvg.h"

const int nb_of_interation_per_pass = 30000;
const int nb_of_pass = 100;

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

volatile long long_1 = 0;
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
  void addValue(int32_t input) __attribute__((optimize("-Os")));

  auto getAverageS() const {
    return ema;
  }

  auto getAverageD() const {
    return (ema << 1) - ema_ema;
  }

  auto getAverageT() const {
    return 3 * (ema - ema_ema) + ema_ema_ema;
  }

private:
  int32_t ema_ema_ema_raw{ 0 };
  int32_t ema_ema_ema{ 0 };
  int32_t ema_ema_raw{ 0 };
  int32_t ema_ema{ 0 };
  int32_t ema_raw{ 0 };
  int32_t ema{ 0 };
};

template< uint8_t A> void EWMA_average<A>::addValue(int32_t input) {
  ema_raw = ema_raw - ema + input;
  ema = ema_raw >> (round_up_to_power_of_2(A));

  ema_ema_raw = ema_ema_raw - ema_ema + ema;
  ema_ema = ema_ema_raw >> (round_up_to_power_of_2(A) - 1);

  ema_ema_ema_raw = ema_ema_ema_raw - ema_ema_ema + ema_ema;
  ema_ema_ema = ema_ema_ema_raw >> (round_up_to_power_of_2(A) - 2);
}

movingAvg< int32_t, 10, 12 > sliding_Average;
EWMA_average< 120 > ewma_average;
constexpr uint8_t alpha{ round_up_to_power_of_2(120) };

void setup() {
  Serial.begin(115200);
  Serial.println("Setup ***");
}

void pause() {
  byte done = false;
  byte dummyByte;

  while (done != true) {
    if (Serial.available() > 0) {
      dummyByte = Serial.read();  // to 'consume' the incoming byte
      if (dummyByte == 'g') done++;
    }
  }
}

// void loop() {
//   int16_t input_data;

//   for (int i = 0; i < nb_of_interation_per_pass; i++) {
//     if (i < 5)
//       input_data = 0;
//     else if (i < (nb_of_interation_per_pass >> 1))
//       input_data = -5000 + random(-100, 100);
//     else
//       input_data = 0 + random(-20, 20);

//     const auto temp{ nb_of_interation_per_pass >> 2 };
//     if (i > (temp - 2) && i < (temp + 2))
//       input_data = 2000;

//     if (i > (3 * temp - 15) && i < (3 * temp + 25))
//       input_data = 200;

//     ewma_average.addValue(input_data);
//     sliding_Average.addValue(input_data);

//     Serial.print("Input:");
//     Serial.print(input_data);
//     Serial.print(",");
//     Serial.print("AVG:");
//     Serial.print(sliding_Average.getAverage());
//     Serial.print(",");
//     Serial.print("EMA:");
//     Serial.print(ewma_average.getAverageS());
//     Serial.print(",");
//     Serial.print("DEMA:");
//     Serial.println(ewma_average.getAverageD());
//     Serial.print(",");
//     Serial.print("TEMA:");
//     Serial.println(ewma_average.getAverageT());

//     // **************** PUT YOUR COMMAND TO TEST HERE ********************
//   }

//   pause();
// }

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
    byte_1 = random(0, 256);
    byte_2 = random(1, 256);

    // STEP 3: Calculation of the time taken to run the dummy FOR loop and the command to test.
    initial_time = micros();
    for (int i = 0; i < nb_of_interation_per_pass; i++) {
      dummy++;  // The dummy instruction is also performed here so that we can remove the effect of the dummy FOR loop accurately.
      // **************** PUT YOUR COMMAND TO TEST HERE ********************
      ewma_average.addValue(i);  // Target command example
      long_2 = ewma_average.getAverageD();
                                 // **************** PUT YOUR COMMAND TO TEST HERE ********************
    }
    final_time = micros();

    // STEP 4: Calculation of the time taken to run only the target command.
    duration = float(final_time - initial_time) / nb_of_interation_per_pass - duration_dummy_loop;
    duration_sum += duration;
    dummy = 0;

    Serial.print(j);
    Serial.print(". ");
    print_result(duration);
  }

  Serial.println();
  Serial.println("********* FINAL AVERAGED VALUE *********** ");
  print_result(duration_sum / nb_of_pass);
  Serial.println("****************************************** ");
  Serial.println();
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