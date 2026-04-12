
## New setting for different power levels

84 = max power on ESP-32-S3.
need a drop down with varisous power settings.
Set this upon startup.

These need to be configuration options and on webportal.

## New setting for WIFI Country Code
Set the country code to get accurate channel information and comply with local regulations. This can be a dropdown with options like US, EU, JP, etc. There should be an option for Default which will not set the country code and will use the default settings of the ESP32 after a reset.

Include all WIFI countries in the dropdown. Set this upon startup.

These need to be configuration options and on webportal.

## Macaddres randomization

## Macaddres randomization cardputer before active wifi scan

Every (n) times the Cardputer performs an active Wi-Fi scan, it should randomize its MAC address to enhance privacy and reduce tracking. This can be implemented by generating a new random MAC address and setting it before initiating the Wi-Fi scan.

This (n) should be configurable, allowing users to choose how frequently they want the MAC address to be randomized (e.g., every scan, every 5 scans, or every 10 scans, eg 1 = every scan, 2 = every second scan, 5 = every 5 scans). These need to be configuration options and on webportal.

Here is some random clips of documentaion and code snippets related to MAC address randomization for the Cardputer:
-----------------------------------------------------------------------------------------------
API Implementation: In the code (likely MicroPython or Arduino C++), you must change the MAC address before activating the esp_wifi_scan_start function or calling WiFi.scanNetworks().

Example Procedure:
Set Wi-Fi mode to Station (WIFI_STA).
Initialize Wi-Fi driver.
Call esp_wifi_set_mac() with a generated random address.


Key Considerations
Active vs. Passive: While randomized MACs protect privacy during active scanning (scanning for networks), the address might change to a static one if the device connects to a known network.
Uniqueness: The generated address must still be unique within the immediate network to avoid address conflicts.
Frequency: The MAC address can be changed for every new scan or upon every reboot of the Cardputer for maximum anonymity

```
#include "WiFi.h"
#include "esp_wifi.h"

void setup() {
  Serial.begin(115200);

  // 1. Initialize Wi-Fi in Station Mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // 2. Generate a random MAC address (locally administered)
  uint8_t newMACAddress[] = {0x02, 0xFE, 0xDC, 0xBA, 0x98, 0x76};
  // Note: The first byte should have the second least significant bit set (x2, x6, xA, xE)
  // to indicate it's a locally administered address.

  // 3. Set the new MAC address
  esp_wifi_set_mac(WIFI_IF_STA, newMACAddress);
  Serial.print("New MAC Address: ");
  Serial.println(WiFi.macAddress());

  // 4. Perform Active Scan
  Serial.println("Starting scan...");
  int n = WiFi.scanNetworks();
  Serial.println("Scan done");
  
  // ... (handle scan results)
}

void loop() {}
```

Important Notes
* Locally Administered Bit: When generating a random MAC, ensure the first byte has the second least significant bit set (e.g., 0x02, 0x06, 0x0A, 0x0E) to signify a locally administered address, reducing conflicts.

* Active vs. Passive Scan: Active scanning sends probe requests with the randomized MAC, which is best for privacy. Passive scanning just listens but is slower.

* ESP-NOW: If you are using ESP-NOW for communication, you must ensure the MAC address is changed before registering peers.

Setting the Locally Administered Bit (U/L bit) to 1 in a random MAC address ensures the address is flagged as user-configured rather than manufacturer-assigned, preventing collisions with hardware (OUI) addresses. By setting the second least significant bit of the first byte to 1 (making it 0x02, 0x06, 0x0A, 0x0E, etc.), devices signify they are using private, temporary, or randomized MACs. 

Detailed Breakdown of the Locally Administered Bit
A MAC address consists of 6 bytes (48 bits). The first byte contains crucial metadata about the address type: 
The First Byte Structure: The last two bits of the first byte are critical.
Bit 0 (Least Significant Bit): Individual/Group bit (0 = Unicast, 1 = Multicast).
Bit 1 (Second Least Significant Bit): Universal/Local (U/L) bit (0 = Universal, 1 = Local).
Significance of "Local" (1): When the U/L bit is set to 1, network devices, switches, and routers know that this address is not unique worldwide, but rather assigned locally.
Why Set It to 1 (0x02, 0x06, etc.)?
Avoiding Conflicts: It prevents random addresses from accidentally matching real hardware addresses produced by manufacturers (universally administered).
MAC Randomization (Privacy): Used extensively by Android, iOS, and modern desktop OSs when scanning for Wi-Fi or connecting to networks to prevent tracking. 

Examples in Hexadecimal
To ensure the second bit is set to 1, the first hexadecimal digit must correspond to a binary value where the second bit is 
. The possible first bytes are: 
0x02, 0x06, 0x0A, 0x0E
0x12, 0x16, 0x1A, 0x1E
...and so on.
Example: A randomized MAC might look like 06:00:00:12:34:56. The 06 translates to binary 0000 0110. The second bit from the right is 1, signifying it is Locally Administered. 

## New setting to support CardPuter ADV

There will be hardware differences between the Cardputer ADV and the original Cardputer. There needs to be a setting in the web portal to select which hardware you are using and the program should adjust accordingly. 

The default should be computed by detecting the Cardputer model at startup. This is important because the Cardputer ADV has different hardware components and pinouts than the original Cardputer and if the program is not configured correctly, it may not work properly with the ADV.

One difference is that the GPS module on the Cardputer ADV has a different pinout than the original Cardputer and if the program is not configured correctly, it will not be able to read GPS data from the module.

## Charging indicator in system stats is wrong
The indicator says charging = 1 and it is not right the battery is draining.

## Make the program more modular 

like create a keyboard handler class
move logSystemStatsIfDue() into the logger class


## Clean up WIFIScanner class

There are a lot variables in the WIFIScanner class that are not being used and should be removed. For example, the scanFailureCount variable is not being used anywhere in the code and can be removed to simplify the class.

Additionally, the dwellTimeMs variable is currently an int but it should be a uint32_t to be consistent with other timing variables in the code. 

There are many boolean variables which are either not being used or could be consolidated into a single variable with different states. For example, instead of having separate boolean variables for whether different scanning modes are active, you could have a single variable that represents that a scan is active. The system is not going to support multiple scanning modes at the same time, so it is not necessary to have separate variables for each mode.

## Agressive scanning mode at the touch of a button 

Highest power shorter dwell time (175ms) and 10ms pause betweeen next scan. This should run for 10 mins and there should be an indicator on Dashboard And then it should automatically switch back to normal scanning mode. The Aggressive scanning button should not be the letter "A" becuase it is too close the "S" for silent mode. The button should be a toggle for aggerssive mode. When the mode engages there should be a beep beep beep beep (4000Hz 25ms duration) and when it turns off there should be a boop boop boop bopp with (2000Hz 25ms duration).

## Ensure config session metrics are used in scanning classes

Ensure config session metrics are used in scanning classes and not local variables. Update the dashboards to use these config values rather than reaching into the scanning classes to get these values, or being supplied them as parameters.

Added a shared shutdown summary model in Config.h:139 so the display receives explicit session metrics instead of reaching into scattered runtime state.

## Sound Options

Please make the following sound options available and functional:
1. Enable disable beep on new ssid
2. Enable disable beep on blocked ssid scanned

# GPS Aquisition problems
When its struggling to find the GPS, lower the baud rate

# Ability to save a default config and reboot
