# ezpilot (K1S Edition)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**ezpilot** is an independent, privacy-focused open-source advanced driver assistance system (ADAS) fork designed for K1S hardware. It is based on the Bukapilot `ka1s_snapshot` baseline and retains full hardware compatibility for Malaysian vehicles (including Perodua and Proton models) with complete Right-Hand Drive (RHD) support.

---

## Key Highlights & Improvements

1. **Standalone & Independent**:
   - Zero dependency on external account servers or proprietary cloud registration.
   - Deterministic hardware-derived identity generated directly on device.
   - Operates 100% offline without remote authorization locks.

2. **Telemetry Neutralization & Privacy**:
   - External telemetry, metric pings, and remote upload services disabled by default.
   - Your driving logs, video, and vehicle telemetry remain strictly on your device.

3. **Toggleable Video Recording**:
   - Optional toggle in Settings to disable road camera video encoding.
   - Dramatically reduces thermal throttling, CPU load, and flash memory wear on Snapdragon 821 hardware while keeping 100% of driver assistance features active.

4. **Preserved K1S Hardware & Safety Compatibility**:
   - Preserves original actuator limits, steering control algorithms, CAN configurations, and Panda MCU firmware.
   - Full support for Perodua (Ativa, Myvi, Bezza, Alza, Axia) and Proton (X50, X70, etc.) interfaces.

---

## System Overview & Features

ezpilot performs the functions of:
- **Adaptive Cruise Control (ACC)**
- **Lane Keep Assistance (LKA)**
- **Forward Collision Warning (FCW)**
- **Lane Departure Warning (LDW)**
- **Assisted Lane Change (ALC)**
- **Camera-based Driver Monitoring (DM)** to alert distracted or drowsy drivers.

### Integration with Stock Features

- **LKA**: Replaced by ezpilot LKA when engaged.
- **LDW**: Replaced by ezpilot LDW.
- **ACC**: Replaced by ezpilot ACC (or configurable to use stock ACC via Settings).
- **Safety Features**: Stock emergency features (AEB, Auto High-Beam, Blind Spot Monitoring) are preserved according to vehicle harness specifications.

---

## Limitations & Safety Policy

**ezpilot does NOT make your vehicle autonomous.**

- The driver must keep their hands on the wheel and maintain attention at all times.
- ezpilot is limited in the torque it can apply and cannot negotiate sharp curves, complex intersections, or abrupt obstacles without driver intervention.
- The system is affected by extreme weather, obscured lenses, faded road markings, and heavy sunlight glare.

---

## Disclaimer & License

> **CAUTION: THIS SOFTWARE IS FOR RESEARCH AND EDUCATIONAL PURPOSES. IT IS NOT AN APPROVED AUTOMOTIVE SAFETY PRODUCT. ALWAYS OBEY LOCAL LAWS AND BE READY TO TAKE IMMEDIATE MANUAL CONTROL AT ALL TIMES.**

ezpilot is released under the **MIT License**. Third-party libraries and vehicle DBC files retain their respective upstream licenses.
