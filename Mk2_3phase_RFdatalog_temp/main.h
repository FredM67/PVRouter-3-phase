void processStartUp(const uint8_t phase);
void processStartNewCycle();
uint8_t nextLogicalLoadToBeAdded();
uint8_t nextLogicalLoadToBeRemoved();
void processMinusHalfCycle(const uint8_t phase);
void processPlusHalfCycle(const uint8_t phase);

void proceedHighEnergyLevel();
void proceedLowEnergyLevel();
void processDataLogging();
bool proceedLoadPrioritiesAndForcing(const int16_t currentTemperature_x100);
void sendResults(bool bOffPeak);
void printConfiguration();
void printOffPeakConfiguration();
void send_rf_data();

inline void setPinState(const uint8_t pin, bool bState);

void processCurrentRawSample(const uint8_t phase, const int16_t rawSample);
void processVoltageRawSample(const uint8_t phase, const int16_t rawSample);
void processRawSamples(const uint8_t phase);

void confirmPolarity(const uint8_t phase);
void processVoltage(const uint8_t phase);

void processPolarity(const uint8_t phase, const int16_t rawSample);
void confirmPolarity(const uint8_t phase);

void updatePhysicalLoadStates();

void printParamsForSelectedOutputMode();