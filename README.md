---

# Lenovo Fan Control Utility

A lightweight background utility that forces the fan on certain Lenovo laptops to its highest speed. It's designed to provide maximum, continuous cooling for demanding tasks like gaming or rendering, overcoming the limitations of Lenovo's built-in software.

This project was born from the need to control the fan on a Lenovo Ideapad Z580, where standard tools like SpeedFan failed to work, and has since been engineered to support a wide range of similar models.

### The Problem

Many Lenovo laptops use a proprietary Embedded Controller (EC) interface, making them incompatible with standard fan control software. While Lenovo's own **Energy Management** driver provides a "Dust Removal" feature, it's not a true cooling solution. This feature runs a self-terminating program inside the EC:
-   The fan spins at max speed for about 9 seconds.
-   It then **pauses for 2 seconds**.
-   This cycle repeats for only 2 minutes before stopping completely.

This makes it useless for sustained cooling.

### The Solution: The Proactive Reset Strategy

This utility turns the flawed "Dust Removal" feature into a reliable, continuous max-speed mode. Instead of reactively re-enabling the fan after it stops, this tool works proactively:

1.  It activates the "Dust Removal" program.
2.  Using a high-precision timer, it waits for ~8.8 seconds.
3.  **Just before** the EC's internal timer can trigger the 2-second pause, the utility sends an instantaneous `Normal -> Fast` command sequence.
4.  This action resets the EC's internal program, starting a new 9-second cycle.

The result is continuous, stable, maximum-speed fan operation for as long as the application is running, with the 2-second pause being replaced by a nearly imperceptible, millisecond-long "hiccup".

### Prerequisites
*   The **[Lenovo Energy Management Driver](http://driverdl.lenovo.com.cn/lenovo/DriverFilesUploadFloder/36484/WIN8_EM.exe)** must be installed. You can check in Device Manager for `Lenovo ACPI-Compliant Virtual Power Controller`.
*   [Download the latest binary from the Releases page.](/releases)

### Usage

This is a simple tray application with no complex settings.
-   **Run `FanControl.exe`**: The application will start in the system tray, and your laptop's fan will immediately spin up to maximum speed.
-   **Exit**: To stop the utility and return to normal, system-controlled fan operation, simply right-click the tray icon and select "Exit".

### Technical Deep Dive: The Inescapable Limitation

During development, an exhaustive reverse-engineering effort was undertaken to find a way to achieve *true* continuous fan control. This involved a full analysis of the DSDT (ACPI) tables and the behavior of the Lenovo `EnergyDrv` driver.

The analysis led to one fundamental conclusion: **the "hiccup" or "dip" in fan speed during the reset is an unavoidable consequence of the API provided by Lenovo.**

#### The Problem is the API, Not the Code

We cannot eliminate the fan speed dip because we are not directly controlling the fan. The API exposed by Lenovo through the `EnergyDrv` driver does not provide a function to "set fan speed to X%". Instead, it only allows us to trigger a hard-coded, self-contained "Dust Removal" program inside the Embedded Controller (EC). This program has its own internal, non-configurable timer.

#### All Paths to an "Ideal" Solution Are Blocked:

1.  **No Alternative ACPI Methods Exist:** A full DSDT dump analysis confirms that there are no other exposed ACPI methods for direct fan control. The `VPCW` method, used by the driver, is not a generic write function; it can only write to two specific EC memory addresses (`VCMD` and `VDAT`), neither of which directly sets fan speed.

2.  **Faking EC Input Data is Impossible (with this driver):** The ideal strategy would be to trick the EC's own automatic logic by faking sensor data (e.g., writing a constant 95°C to the `CPUT` temperature register). However, the driver's API does not provide a generic `WriteToECMemory(address, value)` function needed to perform this. We can see the registers, but we can't reach them.

3.  **Modifying EC Firmware is Unfeasible and Dangerous:** The final frontier would be to reverse-engineer the EC's binary firmware, patch out the 9-second timer, and flash the modified firmware back to the chip. This is an extremely complex process that requires specialized tools and carries a near-certain risk of bricking the laptop if any mistake is made.

### Conclusion: The Proactive Reset is the Pinnacle of What's Possible

Given these hard limitations, the **"proactive reset" strategy is the absolute maximum that can be safely and reliably achieved under the circumstances.**

This utility represents the most optimized implementation of this strategy. It cannot change the physical limitations of the API, but it minimizes the resulting "dip" to the shortest possible duration by using a persistent driver handle and an instantaneous `Normal -> Fast` command sequence.

### Disclaimer

This software is provided as is. Use at your own risk. No warranty is provided.
