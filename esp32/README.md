Recommened hardware ESP32, ESP32-S3 with 4MB flash or larger. 

This ESP32 scale code starts an AP and a web server that Aria scale can upload measurements to. It implements the [BLE Weight Scale Profile](https://www.bluetooth.com/specifications/specs/weight-scale-profile-1-0/) and [Body Composition Profile](https://www.bluetooth.com/specifications/specs/body-composition-service-1-0/) to allow for uploading measurements to some other devices such as [ESPHome](https://esphome.io/) or [openScale Android app](https://github.com/oliexdev/openScale).

## Usage
1. Use PlatformIO to build and upload:
   - Modify config.txt to set your AP name, password, and personal details
   - Build the filesystem and upload it using PlatformIO

2. Connect to the AP and verify the web server is working

3. Configure the Aria scale's WiFi by connecting to its open AP and running:
   ```
   curl 'http://192.168.240.1/scale/setup?ssid=YourSSID&custom_password=YourPassword' -H 'Cookie: token=FFFDDA-BBBBBBCCCC'
   ```

4. Test by stepping on the scale (code tested with firmware v37)

5. Optional: Set up Home Assistant integration
   - Configure ESPHome MiScale component
   - Install the Body Mi Scale HACS integration from https://github.com/dckiller51/bodymiscale

## Regarding the web server
You need to manually apply the patch in the patch.diff file because when Aria uploads, it sets the MIME type to application/x-www-form-urlencoded, and the web server does not parse it correctly. The patch file syntax might be incorrect since it was manually written, so please manually patch it.


## Regarding Bluetooth
It supports the [openScale](https://github.com/oliexdev/openScale) protocol, but it is not fully functional yet. Set the name to "openScale" for app compatibility, see here [here](https://github.com/oliexdev/openScale/blob/master/android_app/app/src/main/java/com/health/openscale/core/bluetooth/BluetoothFactory.java). Protocol details can be found 
[here](https://github.com/oliexdev/openScale/blob/master/android_app/app/src/main/java/com/health/openscale/core/bluetooth/BluetoothCustomOpenScale.java)
and
[here](https://github.com/oliexdev/openScale/blob/master/arduino_mcu/openScale_MCU/openScale_MCU.ino#L354C6-L354C21).

Additionally, it also contains a BLE advertising server that advertises similar to the [ESPHome Xiaomi Mi Scale](https://esphome.io/components/sensor/xiaomi_miscale) documentation.
Details can be found [here](https://github.com/esphome/esphome/blob/dev/esphome/components/xiaomi_miscale/xiaomi_miscale.cpp#L106).

## TODO
- Add support for multi-user
- Add interface to change config
- Apply patch to fix web server using platformio.ini
- Fix openScale compatibility