# Remote Load Receiver

This Arduino sketch receives RF commands from the main PV Router and controls local TRIAC/SSR outputs for remote dump loads.

## Quick Start

1. **Hardware Setup**
   - Arduino UNO or compatible
   - RFM69W/CW or RFM69HW/HCW RF module (wired to SPI pins)
   - TRIAC or SSR for each load output
   - Status LEDs (optional)

2. **Configuration**
   - Set the RF parameters in `config_rf.h` to match the router
   - Configure the load pins in `config.h`

3. **Upload**
   ```bash
   # Using PlatformIO
   pio run -e uno -t upload

   # Using Arduino IDE
   # Open RemoteLoadReceiver.ino and click Upload
   ```

4. **Test**
   - Open Serial Monitor (9600 baud)
   - Should see "Waiting for commands..."
   - When the router's first frame arrives, you'll see "RF link restored"

## Wiring

### RFM69 Module

```
Arduino UNO          RFM69
-----------          -----
D10 (SS)    <--->    NSS
D11 (MOSI)  <--->    MOSI
D12 (MISO)  <--->    MISO
D13 (SCK)   <--->    SCK
D2 (IRQ)    <--->    DIO0
3.3V        <--->    3.3V   ⚠️ NOT 5V!
GND         <--->    GND
```

**Important**: the RFM69 operates at 3.3V. DO NOT connect it to 5V!

### Load Outputs

```
Arduino UNO          TRIAC/SSR
-----------          ---------
D4          --->     Load 0 control input
D3          --->     Load 1 control input
GND         <--->    Common ground
```

Active HIGH: Arduino HIGH = Load ON

### Status LEDs (Optional)

```
Arduino UNO          LEDs
-----------          ----
D5          --->     Green LED anode (+)
D7          --->     Red LED anode (+)
GND         <--->    Cathodes (-), each via a 220Ω resistor
```

- Green, slow blink (1 s on, 1 s off): the main loop is running. If it stops blinking, the firmware is stuck, and the watchdog resets the board within 1 s.
- Red, off: RF link OK
- Red, fast blink (~4 Hz): RF link lost, all loads OFF

## Configuration

Edit these constants in `config_rf.h`:

```cpp
// RF Configuration - MUST match router
namespace RFConfig
{
  inline constexpr uint8_t FREQUENCY{ RF69_868MHZ };  // or RF69_433MHZ, RF69_915MHZ
  inline constexpr uint8_t ROUTER_NODE_ID{ 10 };      // Router's node ID
  inline constexpr uint8_t REMOTE_NODE_ID{ 15 };      // This remote unit's unique ID
  inline constexpr uint8_t NETWORK_ID{ 210 };         // Must match router
  inline constexpr bool IS_RFM69HW{ false };          // true for RFM69HW/HCW
}
```

Edit load configuration in `config.h`:

```cpp
inline constexpr uint8_t NO_OF_LOADS{ 2 };              // Number of loads (max 8)
inline constexpr uint8_t loadPins[NO_OF_LOADS]{ 4, 3 }; // Arduino pins, bit 0 first
```

## Features

- ✅ **Fast response**: Updates within 20ms of receiving command
- ✅ **Safety timeout**: Turns all loads OFF if no RF for 500ms
- ✅ **Watchdog**: Resets the board (loads OFF) if the firmware stops running for 1 s
- ✅ **CRC checking**: Only processes valid packets of the expected length
- ✅ **Node filtering**: Only responds to designated transmitter
- ✅ **Status indicators**: LEDs show the RF link state and that the firmware is running
- ✅ **Serial output**: Start-up settings, RF link lost and restored

## Serial Output Example

```
=======================================
Remote Load Receiver v2.0 (RFM69)
Based on remoteUnit_fasterControl_1
=======================================
Listening to Router ID: 10
My Node ID: 15
Network ID: 210
Number of loads: 2
---------------------------------------
RF module initialized
Waiting for commands...

RF link restored
RF link LOST - turning all loads OFF
RF link restored
```

## Troubleshooting

### No RF messages received

**Symptoms**: Shows "Waiting for commands..." forever

**Solutions**:
1. Check RF module wiring (especially VCC = 3.3V, not 5V!)
2. Verify SPI connections (D10-D13)
3. Check D2 → IRQ connection
4. Verify the settings match the router (FREQUENCY, NETWORK_ID, ROUTER_NODE_ID, REMOTE_NODE_ID)
5. Move units closer together
6. Check antenna (10cm wire for 433MHz)

### RF link keeps dropping

**Symptoms**: Alternates between "RF link restored" and "RF link LOST"

**Solutions**:
1. Improve antenna (use proper λ/4 wire)
2. Move away from metal objects, motors, AC wiring
3. Reduce distance between units
4. Check power supply stability
5. Add 100nF capacitor near RF module VCC
6. Try a different NETWORK_ID (less interference)

### Loads don't switch

**Symptoms**: Receives messages but loads don't change

**Solutions**:
1. Verify TRIAC/SSR wiring
2. Check load pins match configuration
3. Measure voltage at load pin (should be 5V when ON)
4. Test TRIAC independently
5. Check load power supply
6. Verify load ratings

## Multiple Receivers

You can have multiple receivers on the same network:

**Receiver 1:**
```cpp
inline constexpr uint8_t REMOTE_NODE_ID{ 15 };  // Unique ID, in config_rf.h
```

**Receiver 2:**
```cpp
inline constexpr uint8_t REMOTE_NODE_ID{ 16 };  // Different ID, in config_rf.h
```

**Router:** sends each remote unit its own frame, to its node ID

## Performance

- **Latency**: < 20ms from RF received to load updated
- **Timeout**: 500ms (configurable)
- **Range**: 50m indoor, 200m outdoor (typical)
- **Reliability**: > 99% with good RF environment

## License

Based on Mk2 PV Router by Robin Emley
www.Mk2PVrouter.co.uk

## Support

See the router documentation: [RF module and remote loads configuration](../Mk2_3phase_RFdatalog_temp/Readme.en.md#rf-module-and-remote-loads-configuration).
