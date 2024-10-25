# Taiko Drum Controller - Arduino Pro Micro (ATmega32U4)

![Taiko Drum Controller](./images/ATMegaTaiko.png)

An open-source hardware project to build your own **Taiko no Tatsujin** arcade controller for PC gaming.

## About this Project

This project guides you in creating a custom Taiko no Tatsujin controller to enjoy the arcade experience at home.

> **Note:** This project is for personal and non-commercial use only.

---

## Materials Needed

To assemble your Taiko drum controller, you'll need:

1. **Arduino Micro/Leonardo** (with ATmega32U4 microcontroller)
2. **4 piezoelectric sensors**
3. **4 100kΩ resistors**
4. Essential electronic components (e.g., breadboards, jumper wires)
5. Wood planks and cutting tools (only if building the drum from scratch).  
   > If you have an aftermarket Taiko Force Lv5, you can use it directly.

---

## Steps to Build the Controller

1. **Prepare the Drum**  
   Construct the drum and securely attach the 4 piezoelectric sensors to it. See the diagram below for recommended sensor placement.

   ![Sensor Setup](./images/piezo_locations.png)

2. **Connect Components**  
   Wire the piezoelectric sensors and other components to the Arduino as shown in the schematic below.  
   > Note: Polarity doesn’t matter for the piezoelectric sensors. This setup is designed for Arduino Micro; if using another board, refer to its documentation.

   ![Controller Scheme](./images/scheme.png)

3. **Flash the Firmware**  
   Upload the firmware to the Arduino board. You may need to adjust parameters such as `SAMPLE_CACHE_LENGTH`, `HIT_THRES`, `RESET_THRES`, and `sensitivity` as described below.

4. **Enjoy!**  
   Once configured, your controller is ready to use.

---

## Parameter Tuning

1. **Hit and Reset Thresholds**  
   Set `DEBUG 1` to disable keyboard output and monitor signal values via the serial port. Roll across one of the drum’s 4 zones, visualizing the output graph from the serial monitor.  
   - **Hit threshold**: Should be lower than your hardest hit.
   - **Reset threshold**: Should be above the low point between hits (trough) and below the hit value.  

   Repeat this process for each drum area to find optimal thresholds.

   ![Setting hit and reset values](./images/tune_hit_reset.png)

2. **Sampling Length**  
   In `cache.h`, set `SAMPLE_CACHE_LENGTH` to a power of 2 (e.g., 2, 8, 16, 32). A value of **16** is optimal for Arduino, but if you’re using a faster microcontroller (sampling at 4000Hz or more), you can set it to **32** for a smoother signal.

3. **Sensor Sensitivity**  
   Sensor output can vary. Use the `sensitivity` parameter to normalize discrepancies. For example, if the **right-don** zone outputs higher values, adjust `sensitivity` to `{1.0, 1.0, 0.5, 1.0}` to balance the response.

   ![Setting sensitivity values](./images/tune_sensitivities.png)

   > Proper sensor installation is critical. Ensure sensors are securely attached to the drum.

4. **Launch the Game**  
   With tuning complete, start the game and enjoy your custom Taiko controller!

--- 
