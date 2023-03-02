void processCurrentRawSample(uint8_t phase, int16_t rawSample);
void processVoltageRawSample(uint8_t phase, int16_t rawSample);
void processVoltage(uint8_t phase);
void processRawSamples(uint8_t phase);
void processPolarity(uint8_t phase, int16_t rawSample);
void confirmPolarity(uint8_t phase);
void processVoltage(uint8_t phase);
void processRawSamples(uint8_t phase);
void processStartUp(uint8_t phase);
void processStartNewCycle();
void processMinusHalfCycle(uint8_t phase);
void processLatestContribution(uint8_t phase);
void processPlusHalfCycle(uint8_t phase);

void processDataLogging();
void printDataLogging();
void printConfiguration();

int freeRam();