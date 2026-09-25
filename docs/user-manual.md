# Bike Odometer User Manual

Status: initial draft, based on the current firmware and Android app.

## About Bike Odometer

Bike Odometer counts wheel rotations using a magnetic sensor and stores ride
history on the device. The Android app, labelled **Bike Counter**, connects over
Bluetooth to show distance, ride duration, battery level, and trip history.

Distance is calculated from wheel rotations and the selected wheel size. Set the
wheel size for your bicycle before relying on the displayed distances.

## What you need

- A powered Bike Odometer device with its sensor and wheel magnet installed.
- An Android phone with Bluetooth and the Bike Counter app installed.
- Your bicycle's wheel size in inches.

Installation photos, magnet alignment instructions, and the app download link
will be added to this manual. For now, obtain the app and hardware setup
instructions from the person supplying your device.

## First connection

1. Enable Bluetooth on your phone and open **Bike Counter**.
2. Allow the Bluetooth or nearby-device permissions requested by the app.
3. Keep the phone near the odometer. If the bicycle has been parked, rotate the
   wheel so the magnet passes the sensor to wake Bluetooth discovery.
4. On **Connect Sensor**, wait for automatic discovery. The firmware advertises
   as `Bike_Odometer`; the app may show a formatted device name.
5. Select your sensor under **Connected Devices** and wait for its data to load.
6. Use the wheel-size selector in the top-right corner to choose your wheel size.

The device is discoverable for approximately one minute after power-up or when
movement resumes after a quiet period. Open the app before waking the sensor.

Connect the app before your first ride so it can set the device's clock from your
phone. The firmware does not store trips until its clock has been set. Check that
your phone's date and time are correct.

## Setting wheel size

With the sensor connected, open the wheel-size selector and choose the matching
diameter in inches. Available choices are **10, 11, 12, 14, 16, 18, 20, 24, 26,
27, and 29 inches**.

The app saves the setting to the sensor. If the selection returns to its previous
value, reconnect the sensor and try again. The current app has no custom-size
entry.

Changing wheel size also changes the distances calculated for stored rotations,
including historical rides shown in the app.

## Recording a ride

Once the clock is set, the odometer records wheel activity automatically; you do
not need to press a start or stop button in the app or keep the phone connected.

Activity is stored in five-minute intervals. An interval with no rotations ends
the current trip. A long ride is split into records of up to four hours each.
Duration and average speed are based on these intervals, so they are approximate.

The device retains the newest 3,000 completed trips. When its archive fills,
older trips are replaced. Reconnect the app periodically to download ride data.

## Viewing your activity

### Current

Open **Current** in the bottom navigation to see total distance, duration, and
rotations for the selected sensor's loaded trips.

- **7D** shows daily distances over seven days.
- **30D** shows daily distances over thirty days.
- **1Y** shows monthly distances over twelve months.
- **Previous** and **Next** move between available periods.

The latest chart period is based on the latest recorded activity. The distance
and duration cards show totals across loaded trips, not just the chart period.
This screen is an activity summary rather than a live speed display.

### History

Open **History** and select a trip to see its date, start and end times, distance
in kilometres, duration in minutes, average speed in km/h, and a distance chart
for its five-minute intervals. Use the back arrow to return to the list.

### Previously connected sensors

The **Connect Sensor** screen also lists **Historical Devices**. Select one to
view data already saved on your phone. Historical data may not include recent
rides until the sensor connects again.

To return to sensor selection, tap the sensor name beneath **Bike Counter** in
the app header. This disconnects the selected session.

## Battery level

When battery data is available, the app shows a percentage in the header. Treat
this as an estimate; a saved reading may not represent the current battery level.
Battery replacement instructions will be added after the enclosure and battery
access procedure are documented.

## Troubleshooting

| Problem | What to try |
| --- | --- |
| **No sensors found** | Check Bluetooth and app permissions. Keep the phone nearby, then rotate the wheel after the bike has been parked to wake discovery. |
| Sensor data fails to load | Keep the phone near the device and select the sensor again. Reopen the app if necessary. |
| Recent ride is missing | Reconnect to download data and allow the current five-minute recording interval to finish. Confirm that you connected the app before the first ride to set the clock. |
| Distance looks incorrect | Check the selected wheel size and the sensor/magnet installation. |
| Wheel-size change does not persist | Make sure the sensor is connected, then select the size again. |
| Battery percentage is absent | The sensor may not have supplied a battery reading. Try reconnecting. |

## Still to be documented

- App download and installation, including supported Android versions.
- Hardware mounting, magnet alignment, and a first-installation check.
- Battery replacement, enclosure care, and weather protection.
- Screenshots of setup, activity, and history screens.
- Data backup and recovery guidance, and a support contact.
