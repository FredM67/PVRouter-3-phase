# Remote Load Receiver

This Arduino sketch receives RF commands from the main PV Router and controls local TRIAC/SSR outputs for remote dump loads.

## Quick Start

1. **Hardware Setup**
   - Arduino UNO or compatible
   - RFM12B or RFM69 RF module (wired to SPI pins)
   - TRIAC or SSR for each load output
   - Status LED (optional)

2. **Configuration**
   - Edit `RemoteLoadReceiver.ino`
   - Set RF parameters to match transmitter
   - Configure load pins

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
   - When transmitter sends data, you'll see load states

## Wiring

### RFM12B Module

```
Arduino UNO          RFM12B
-----------          ------
D10 (SS)    <--->    NSS
D11 (MOSI)  <--->    MOSI
D12 (MISO)  <--->    MISO
D13 (SCK)   <--->    SCK
D2 (IRQ)    <--->    IRQ
3.3V        <--->    VCC    ⚠️ NOT 5V!
GND         <--->    GND
```

**Important**: RFM12B operates at 3.3V. DO NOT connect to 5V!

### Load Outputs

```
Arduino UNO          TRIAC/SSR
-----------          ---------
D5          --->     Load 0 control input
D6          --->     Load 1 control input
GND         <--->    Common ground
```

Active HIGH: Arduino HIGH = Load ON

### Status LED (Optional)

```
Arduino UNO          LED
-----------          ---
D4          --->     Anode (+)
GND         <--->    Cathode (-) via 220Ω resistor
```

- Solid ON: RF link OK
- Blinking: RF link lost

## Configuration

Edit these constants in `RemoteLoadReceiver.ino`:

```cpp
// RF Configuration - MUST match transmitter
#define RF_FREQ RF12_433MHZ      // or RF12_868MHZ, RF12_915MHZ
const uint8_t TX_NODE_ID = 10;   // Transmitter's node ID
const uint8_t MY_NODE_ID = 15;   // This receiver's unique ID
const uint8_t NETWORK_GROUP = 210; // Must match transmitter

// Load Configuration
const uint8_t NO_OF_LOADS = 2;   // Number of loads (max 8)
const uint8_t loadPins[NO_OF_LOADS] = { 5, 6 }; // Arduino pins
```

## Features

- ✅ **Fast response**: Updates within 20ms of receiving command
- ✅ **Safety timeout**: Turns all loads OFF if no RF for 500ms
- ✅ **CRC checking**: Only processes valid packets
- ✅ **Node filtering**: Only responds to designated transmitter
- ✅ **Status indicator**: LED shows RF link quality
- ✅ **Serial debugging**: Verbose output for troubleshooting

## Serial Output Example

```
=======================================
Remote Load Receiver v1.0
Based on remoteUnit_fasterControl_1
=======================================
Listening to Node ID: 10
My Node ID: 15
Network Group: 210
Number of loads: 2
---------------------------------------
RF module initialized
Waiting for commands...

RF link restored
Received: 0b00000000 - Loads: 0:OFF 1:OFF
Received: 0b00000001 - Loads: 0:ON 1:OFF
Received: 0b00000011 - Loads: 0:ON 1:ON
Received: 0b00000001 - Loads: 0:ON 1:OFF
Received: 0b00000000 - Loads: 0:OFF 1:OFF
```

## Troubleshooting

### No RF messages received

**Symptoms**: Shows "Waiting for commands..." forever

**Solutions**:
1. Check RF module wiring (especially VCC = 3.3V, not 5V!)
2. Verify SPI connections (D10-D13)
3. Check D2 → IRQ connection
4. Verify settings match transmitter (FREQ, GROUP, NODE_ID)
5. Move units closer together
6. Check antenna (10cm wire for 433MHz)

### RF link keeps dropping

**Symptoms**: Alternates between "RF OK" and "RF link LOST"

**Solutions**:
1. Improve antenna (use proper λ/4 wire)
2. Move away from metal objects, motors, AC wiring
3. Reduce distance between units
4. Check power supply stability
5. Add 100nF capacitor near RF module VCC
6. Try different NETWORK_GROUP (less interference)

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
const uint8_t MY_NODE_ID = 15;  // Unique ID
const uint8_t NO_OF_LOADS = 2;
```

**Receiver 2:**
```cpp
const uint8_t MY_NODE_ID = 16;  // Different ID
const uint8_t NO_OF_LOADS = 2;
```

**Transmitter:** Send different messages to each node ID

## Performance

- **Latency**: < 20ms from RF received to load updated
- **Timeout**: 500ms (configurable)
- **Range**: 50m indoor, 200m outdoor (typical)
- **Reliability**: > 99% with good RF environment

## License

Based on Mk2 PV Router by Robin Emley
www.Mk2PVrouter.co.uk

## Support

See main documentation:
- `QUICK_START_REMOTE_LOADS.md` - Installation guide
- `REMOTE_LOADS_README.md` - Technical details
- `REMOTE_LOADS_ARCHITECTURE.md` - System diagrams
