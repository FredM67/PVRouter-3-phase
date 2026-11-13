# Where the default calibration values come from

The default values of `calibration.h` suit the universal **3phaseDiverter** board (rev. 6.0 and
later) with 333 mV voltage-output CTs. They are calculated from the design of the board; this page
shows how, out of curiosity. You don't need it to calibrate: compare with a reference meter, and
new value = old value × reference reading / router reading.

## What the two factors mean

The firmware works on 10-bit ADC samples, steps from 0 to 1023 around a mid-point:

- `f_voltageCal` is the number of **volts per voltage step**: Vrms = `f_voltageCal` × RMS of the
  voltage samples. It only serves the voltage shown in the datalog.
- `f_powerCal` is the number of **watts per (voltage step × current step)**: P = `f_powerCal` ×
  mean of the products of the voltage and current samples. It is the one the router diverts with.
  It is the product of volts per voltage step and amps per current step.

`calibration.h` gives `f_powerCal` per amp of CT rating: `f_powerCal` = `f_powerCalPerAmp` ×
`f_ctRating`.

## Voltage: 0.8952 V per step

```
mains ── 6 × 20 kΩ (0.1 %) ── ZMPT101K 1:1 ── 150 Ω burden (0.1 %)
                                               └─ C10 10 µF ── bias network (10k / 10k / 100 nF) ── ADC
```

- The 120 kΩ chain turns the mains voltage into a current: 8.333 µA per volt. The ZMPT101K passes
  it 1:1 to its burden.
- Through C10, the VREF/2 bias network loads the 150 Ω burden: the ZMPT101K sees 145.73 Ω, of which
  98.81 % reaches the ADC at 50 Hz.
- At the ADC: 8.333 µA × 145.73 Ω × 0.9881 = **1.2000 mV per mains volt**.
- With the internal ADC reference at its nominal 1.1 V, one step is 1.1 V / 1024 = 1.0742 mV.

`f_voltageCal` = 1.0742 mV / 1.2000 mV = **0.8952 V per step**.

## Power: 0.0029225 per A of CT rating

- A voltage-output CT gives 333 mV RMS at its rated current. It feeds the ADC through C12 (10 µF)
  and the same bias network, which passes 98.81 % of its voltage (its own output impedance is
  negligible against 5 kΩ).
- For 1 A of CT rating: 1 A / 0.333 V × 1.0742 mV / 0.9881 = **3.2647 mA per current step**.

`f_powerCalPerAmp` = 0.8952 × 3.2647 × 10⁻³ = **0.0029225**, so 0.14613 with a 50 A CT.

### Current-output CT with a burden resistor

A current-output CT, such as the YHDC SCT-013-000 (100 A : 50 mA, so 2000:1), needs a burden
resistor R (R18 / R28 / R38 on the board), and the burden sets the range, not the CT label. Across
R, the CT gives R × I / 2000 volts: it behaves like a 333 mV CT rated for the current that gives
333 mV, so `f_ctRating` = 0.333 × 2000 / R = **666 / R** amps, 30.3 with 22 Ω.

Keep the peak voltage across R below half the ADC range, 0.55 V: R < 0.55 × 2000 / (√2 × the
largest current).

### Old board

The old board has other input stages and a 5 V reference: this calculation doesn't apply. Set
`f_ctRating` to 1 and `f_powerCalPerAmp` to the former `f_powerCal`, then calibrate as usual.

## How close it is

| Source | Effect |
|---|---|
| Internal ADC reference, 1.0 to 1.2 V from one ATmega328P to another | up to ±9 % on the voltage, ±18 % on the power |
| ZMPT101K ratio error | −0.3 to +0.2 % |
| 0.1 % resistors | ±0.1 to 0.2 % |
| C10 (X7R, ±10 %) | about ±0.16 % |
| CT ratio error | typically ±1 % |
| 60 Hz instead of 50 Hz | 0.06 % |

The three phases share the ADC reference, so it never unbalances them: the router balances import
and export the same way, only the watts and volts shown are off. The CTs differ from one phase to
another; to match the phases, run one cable through the three CTs, with the voltage inputs on the
same phase, and bring P2 and P3 to the value of P1.
