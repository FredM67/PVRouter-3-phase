#include <iostream>
#include "ewma_avg.hpp"
int main() {
    std::cout << "round_up_to_power_of_2(120) = " << (int)round_up_to_power_of_2(120) << std::endl;
    EWMA_average<120> avg;
    avg.addValue(100);
    std::cout << "After adding 100: EMA=" << avg.getAverageS() << " DEMA=" << avg.getAverageD() << " TEMA=" << avg.getAverageT() << std::endl;
    return 0;
}
