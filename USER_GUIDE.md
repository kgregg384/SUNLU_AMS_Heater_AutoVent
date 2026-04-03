# SUNLU AMS Heater Auto-Vent Controller - User Guide

## Table of Contents

1. [Introduction](#1-introduction)
2. [Quick Start](#2-quick-start)
3. [LED Status Indicators](#3-led-status-indicators)
4. [Button Controls](#4-button-controls)
5. [Learning Mode](#5-learning-mode)
6. [Normal Operation](#6-normal-operation)
7. [Troubleshooting](#7-troubleshooting)
8. [Maintenance](#8-maintenance)
9. [Safety Information](#9-safety-information)

---

## 1. Introduction

The SUNLU AMS Heater Auto-Vent Controller is an intelligent automation system for your 3D printer filament dryer. It automatically opens and closes a vent based on the dryer's operating state, helping to:

- Remove excess moisture during drying cycles
- Manage temperature buildup
- Prevent moisture build-up inside the unit due to heating with the vent closed
- Extend the life of your dryer's heating element
- Reduce energy consumption

The system works by monitoring the electrical current drawn by your dryer and learning to recognize when the heater and fan are operating.

### What You'll Need to Build This Project

**Purchased from [3dcreator.shop](https://3dcreator.shop):**
- Custom PCB (bare board or kit with components)

**You Will Need to Provide:**
- 3D printed parts (STL files available in repository)
- Seeed XIAO SAMD21 **OR** Seeed XIAO RP2040 microcontroller (code supports both!)
- ADS1115 ADC module
- ACS758 LCB-050B current sensor
- Feedback-enabled servo motor
- Momentary push button
- LED and resistor
- USB-C cable for power
- 5V USB power source (1A minimum)
- Wire for connections
- Basic soldering equipment
- Your SUNLU AMS dryer (or compatible unit)

**Assembly Time:**
- PCB assembly (if not pre-assembled): 1-2 hours
- 3D printing parts: 4 hours print time
- Final assembly and installation: 30-60 minutes
- Initial calibration: 5 minutes

---

## 2. Quick Start

### First-Time Setup

**Prerequisites:**
- PCB assembled with all components soldered
- 3D printed parts installed on your dryer
- Firmware uploaded to XIAO SAMD21 microcontroller

**Installation Steps:**

1. **Install the Controller**
   - Mount the assembled AutoVent onto the top of the sunlu case and plug the cable into the dryer
   - Connect the AC power to the AutoVent
   - Connect USB-C power to the controller

**Important**: If you need assembly instructions for the PCB or 3D printed parts, refer to the build guide included with your PCB kit or visit [3dcreator.shop](https://3dcreator.shop) for detailed assembly tutorials.

2. **Power On**
   - The LED will flash slowly while the system calibrates
   - After 10 seconds, the LED will turn solid
   - The system is now ready

3. **Run Learning Mode** (May be required if vent does not immediately open and close as expected)
   - Make sure your dryer is OFF
   - Press and hold the button for 6-7 seconds
   - Follow the LED patterns (see Section 5)
   - Learning takes about 45 seconds total
   - Your settings are automatically saved

4. **Test Operation**
   - Turn on your dryer
   - The vent should open when the heater activates
   - Turn off the heater (fan continues)
   - The vent should stay open
   - Turn off the fan
   - The vent should close after 3 minutes

**That's it!** Your Auto-Vent Controller is now ready for normal use.

---

## 3. LED Status Indicators

The status LED tells you what the controller is doing:

### Normal Operation

| LED Pattern | Meaning | What's Happening |
|-------------|---------|------------------|
| **SOLID ON** | Active | System is operating normally, monitoring your dryer |
| **OFF** | Standby | System is disabled (press button 2-5s to wake) |

### Learning Mode

| LED Pattern | Meaning | What to Do |
|-------------|---------|-----------|
| **SOLID ON** | Ready and Waiting | Make dryer change, then press button when ready |
| **FAST FLASH** | Measuring | Don't touch! Measurement in progress (10 seconds) |
| **3 LONG BLINKS** | Learning Complete | All done! Settings saved, back to normal |

### Calibration

| LED Pattern | Meaning |
|-------------|---------|
| **SLOW FLASH** | Calibrating servo position |

---

## 4. Button Controls

The single button provides three different functions based on how long you press it:

### Short Press (< 2 seconds)

**Function**: Recalibrate Servo

**When to Use**:
- If the vent isn't opening or closing fully
- After physically adjusting the servo arm
- If the vent seems stuck or misaligned

**What Happens**:
- LED flashes slowly
- Servo moves to test positions
- Takes about 3 seconds
- Returns to normal operation

### Long Press (2-5 seconds)

**Function**: Enter/Exit Standby Mode

**When to Use**:
- When you want to temporarily disable the controller
- To manually control the vent position
- During maintenance or cleaning

**What Happens**:
- LED turns OFF (entering standby)
- LED turns back ON (exiting standby)
- Vent remains in current position while in standby

**Note**: The LED will turn off after 2 seconds. This is normal - keep holding until 5 seconds to avoid accidentally entering learning mode.

### Very Long Press (> 5 seconds)

**Function**: Enter Learning Mode

**When to Use**:
- First-time setup
- Moving to a different dryer
- If vent control becomes unreliable
- After changing the current sensor installation

**What Happens**:
- LED turns off at 2 seconds (keep holding!)
- LED turns back on at 5 seconds
- Learning mode begins (see Section 5)

**Tip**: Count slowly to 7 to be sure you've held long enough.

---

## 5. Learning Mode

Learning Mode teaches the controller to recognize your specific dryer's electrical signature. This should be done once per dryer.

### When to Run Learning Mode

- **Required**: First-time installation
- **Recommended**: After moving to a different dryer
- **Optional**: If you notice the vent not responding correctly

### How to Run Learning Mode

#### Preparation

- Ensure your dryer is completely OFF (heater and fan)
- Clear any obstructions from the vent
- Have your dryer controls within reach
- Budget 2 minutes of your time

#### Step-by-Step Instructions

**PHASE 1: Baseline Measurement**

1. **Start Learning Mode**
   - Press and hold button for 6-7 seconds
   - LED turns off at 2 seconds → **Keep holding!**
   - LED turns back on at 5 seconds → Release button
   - Learning mode begins

2. **LED: SOLID** → Waiting for you
   - **Ensure dryer is completely OFF** (heater and fan)
   - When ready, **press button once**

3. **LED: FAST FLASH** → Measuring baseline
   - System is measuring (10 seconds)
   - **Do not touch anything**
   - LED will automatically go solid when done

---

**PHASE 2: Heater + Fan Measurement**

4. **LED: SOLID** → Waiting for you
   - **Turn your dryer ON** (heater + fan both running)
   - When dryer is running, **press button once**

5. **LED: FAST FLASH** → Measuring heater + fan
   - System is measuring (10 seconds)
   - **Leave dryer running, don't touch**
   - LED will automatically go solid when done

---

**PHASE 3: Fan-Only Measurement**

6. **LED: SOLID** → Waiting for you
   - **Turn heater OFF** (keep fan running)
   - On most dryers: press heat button twice quickly
   - When fan is running alone, **press button once**

7. **LED: FAST FLASH** → Measuring fan only
   - System is measuring (10 seconds)
   - **Leave fan running, don't touch**
   - Wait for completion

---

**COMPLETE!**

8. **LED: 3 LONG BLINKS** → All done!
   - Settings automatically saved to memory
   - System returns to normal operation
   - You can now use your dryer normally

---

### Learning Mode Tips

**Do's:**
- ✅ Follow the LED patterns carefully
- ✅ Let measurements complete (don't interrupt)
- ✅ Make sure your dryer controls work properly
- ✅ Perform learning when dryer is at room temperature

**Don'ts:**
- ❌ Don't rush between phases
- ❌ Don't touch the dryer during FAST FLASH measurements
- ❌ Don't turn off power during learning
- ❌ Don't worry if the vent doesn't move during learning (this is normal)

### If Learning Fails

If learning mode doesn't complete properly:

1. Turn everything off and wait 10 seconds
2. Start over from Phase 1
3. Pay close attention to the LED patterns
4. Ensure your dryer switches modes correctly

---

## 6. Normal Operation

Once learning is complete, the controller works automatically. Here's what to expect:

### Automatic Vent Control

**When Dryer Starts:**
- Heater turns on → Vent opens immediately
- Fan starts → Vent opens immediately

**During Drying Cycle:**
- Vent stays open as long as heater or fan is running
- No manual intervention needed

**When Heater Turns Off (fan continues):**
- Vent stays open
- This is normal - fan cools down the heater

**When Everything Turns Off:**
- Vent stays open for 3 more minutes (cooldown delay)
- Then vent automatically closes
- This protects your fan bearing from thermal shock

### What You Should See

**Normal Behavior:**
- LED is solid ON during operation
- Vent opens when you start the dryer
- Vent stays open while dryer runs
- Vent closes a few minutes after dryer stops

**Expected Delays:**
- Vent may take 1-2 seconds to fully open/close
- 3-minute cooldown delay before closing is intentional

### Manual Control (if needed)

While the system is designed for fully automatic operation, you can:

- **Disable temporarily**: Press button for 2-5 seconds (standby mode)
- **Recalibrate**: Short press if vent isn't moving correctly
- **Relearn**: Very long press (5+ seconds) if behavior changes

---

## 7. Troubleshooting

### Problem: Vent doesn't open when dryer starts

**Possible Causes & Solutions:**

1. **System in standby mode**
   - Check: LED is off
   - Solution: Press button for 2-5 seconds to wake

2. **Learning not performed**
   - Check: First time use or new dryer
   - Solution: Run learning mode (Section 5)

3. **Servo not moving**
   - Check: Listen for servo motor sound
   - Solution: Try short press to recalibrate
   - If still stuck: Check servo power connection

4. **Current sensor not reading**
   - Check: Verify sensor is clipped around ONE power wire
   - Solution: Reconnect sensor, restart controller

### Problem: Vent doesn't close after dryer stops

**Possible Causes & Solutions:**

1. **Cooldown delay active**
   - This is normal! Wait the full 3 minutes
   - Small parasitic loads can keep vent open

2. **Dryer still drawing current**
   - Check: Does the dryer have a display or light still on?
   - Solution: This is normal if dryer has standby power draw

3. **Thresholds too sensitive**
   - Solution: Run learning mode again

### Problem: Vent opens and closes repeatedly

**Possible Causes & Solutions:**

1. **Threshold too close to actual current**
   - Solution: Run learning mode again
   - Make sure dryer is at stable temperature during learning

2. **Heater cycling on/off**
   - This may be normal dryer behavior
   - Solution: Wait for system to stabilize (30-60 seconds)

3. **Loose sensor connection**
   - Check: Ensure current sensor is firmly clamped
   - Solution: Reposition sensor, secure with zip tie

### Problem: LED flashing patterns don't match guide

**Possible Causes & Solutions:**

1. **Interrupted learning mode**
   - Solution: Press button 2-5 seconds to exit, try again

2. **Low power supply**
   - Check: Using adequate 5V 1A+ power supply
   - Solution: Try different USB power adapter

3. **Firmware issue**
   - Contact support for firmware update

### Problem: Button doesn't respond

**Possible Causes & Solutions:**

1. **Button stuck or damaged**
   - Check: Press firmly and listen for click
   - Solution: May need button replacement

2. **Holding wrong duration**
   - Very long press requires 6+ seconds
   - Count slowly and deliberately

3. **During learning mode**
   - Button doesn't respond during measurements
   - Wait for current phase to complete

---

## 8. Maintenance

### Regular Maintenance (Monthly)

1. **Visual Inspection**
   - Check for loose wires or connections
   - Ensure servo arm hasn't come loose
   - Verify vent mechanism moves freely

2. **Clean Sensors**
   - Wipe dust off current sensor
   - Check servo feedback sensor (if accessible)

3. **Test Operation**
   - Run a short drying cycle
   - Verify vent opens and closes properly

### Deep Maintenance (Every 6 Months)

1. **Recalibrate**
   - Run learning mode to refresh thresholds
   - Useful if behavior has changed over time

2. **Clean Vent Mechanism**
   - Remove dust and debris from vent
   - Ensure smooth movement
   - Lubricate vent hinges if needed (dry lube only)

3. **Check Connections**
   - Verify all wires secure
   - Tighten any loose screws
   - Inspect for signs of wear

### When to Rerun Learning Mode

- Moved controller to a different dryer
- Significant change in vent behavior
- After replacing current sensor
- After major dryer maintenance
- Every 6-12 months as preventive measure

---

## 9. Safety Information

### General Safety

⚠️ **ELECTRICAL SAFETY**
- This device monitors AC mains voltage (120V or 240V)
- Ensure current sensor is properly rated for your voltage
- Never touch exposed current sensor connections while powered
- Disconnect dryer from power before working on connections

⚠️ **FIRE SAFETY**
- Controller does not prevent dryer malfunction
- Never leave dryer unattended for extended periods
- Ensure proper ventilation around dryer and controller
- Keep controller away from heat sources

⚠️ **MECHANICAL SAFETY**
- Servo motor can pinch fingers
- Keep clear of moving vent mechanism
- Ensure vent can move freely (no obstructions)

### Proper Use

✅ **Do:**
- Use controller within specified temperature range (0°C to 50°C)
- Provide adequate ventilation
- Use specified 5V power supply
- Follow learning mode instructions carefully
- Check connections regularly

❌ **Don't:**
- Don't expose to moisture or liquids
- Don't block vent mechanism
- Don't modify electrical connections without proper knowledge
- Don't use with dryers exceeding 50A current draw
- Don't bypass safety features

### Limitations

This controller is designed to:
- ✅ Automatically control vent based on dryer operation
- ✅ Improve energy efficiency
- ✅ Extend dryer component life

This controller does NOT:
- ❌ Replace dryer safety features (thermostats, fuses)
- ❌ Prevent dryer malfunction
- ❌ Monitor dryer temperature
- ❌ Provide fire detection or suppression

### Environmental Considerations

- **Operating Temperature**: 0°C to 50°C (32°F to 122°F)
- **Storage Temperature**: -20°C to 70°C (-4°F to 158°F)
- **Humidity**: 10% to 90% non-condensing
- **Altitude**: Up to 2000m (6500ft)

### In Case of Emergency

If you smell burning, see smoke, or suspect malfunction:

1. **Immediately unplug the dryer** from wall power
2. **Disconnect controller power**
3. **Do not attempt to use** until inspected
4. **Contact qualified technician** for inspection

### Disposal

When disposing of this device:
- Follow local e-waste regulations
- Do not throw in regular trash
- Remove any batteries (if applicable)
- Consider recycling electronics at designated facility

---

## Support and Contact

### Need Help?

If this guide doesn't solve your problem:

1. Check the Technical Specification document for advanced details
2. Review the troubleshooting section again
3. Visit the [3dcreator.shop](https://3dcreator.shop) support page
4. Check the GitHub repository for community support and discussions

### Firmware and Hardware Updates

**Purchase PCBs and Components:**
- Visit [3dcreator.shop](https://3dcreator.shop) for the latest PCB versions
- Choose between bare PCBs or assembled kits with components

**Download Design Files:**
Check the GitHub repository for:
- Latest firmware versions
- 3D printable STL files
- Schematic and PCB design files
- Bug fixes and new features
- Community support and build guides

**Repository**: [github.com/kgregg384/SUNLU_AMS_Heater_AutoVent](https://github.com/kgregg384/SUNLU_AMS_Heater_AutoVent)

---

## Appendix: Quick Reference

### Button Press Summary

| Duration | Function | LED Response |
|----------|----------|--------------|
| < 2 sec | Recalibrate | Slow flash |
| 2-5 sec | Standby toggle | OFF → ON or ON → OFF |
| > 5 sec | Learning mode | OFF (2s) → ON (5s) → Learning sequence |

### LED Pattern Quick Reference

| Pattern | Common Meaning |
|---------|----------------|
| Solid | Normal operation OR waiting for button press (learning) |
| Off | Standby mode |
| Slow flash | Calibrating |
| Fast flash | Measuring (learning) |
| 3 long blinks | Learning complete! |

### Learning Mode Quick Steps

1. **Dryer OFF** → Hold button 6s → **SOLID** (press button) → **FAST FLASH** (10s)
2. **Turn dryer ON** → **SOLID** (press button) → **FAST FLASH** (10s)
3. **Turn heater OFF** (fan only) → **SOLID** (press button) → **FAST FLASH** (10s)
4. **3 LONG BLINKS** → Done!

---

**Document Version**: 1.0
**Last Updated**: January 27, 2024
**For Hardware Version**: 1.0
**Firmware Version**: 1.0

---

**PCBs and Kits Available**: [3dcreator.shop](https://3dcreator.shop)
**Open Source Repository**: [github.com/kgregg384/SUNLU_AMS_Heater_AutoVent](https://github.com/kgregg384/SUNLU_AMS_Heater_AutoVent)

This is an open-source hardware project. PCBs and component kits available from 3dcreator.shop
