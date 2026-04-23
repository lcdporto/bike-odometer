# Android BLE Integration Spec

## Protocol version

- Version: `1`
- Last updated: 2026-04-10

This version guarantees that each trip object includes a stable `id` field.

## Scope

This document specifies how an Android app should interact with the bike odometer firmware over BLE.

It covers:

- GATT discovery
- Characteristic UUIDs and data formats
- Trips transfer over chunked notifications
- Recommended Android client flow
- Error handling and practical constraints

This spec reflects the current firmware behavior in `src/ble_service.c`.

## Peripheral Identity

- Device name: `Bike_Odometer`
- Primary service UUID: `78563412-3412-7856-1234-567812345678`

## Characteristics

All characteristics are vendor-specific 128-bit UUIDs under the primary service.

### Pulse Count

- UUID: `79563412-3412-7856-1234-567812345678`
- Properties: `READ`
- Format: UTF-8 string containing an unsigned decimal integer
- Example: `1234`

### Trips

- UUID: `88563412-3412-7856-1234-567812345678`
- Properties: `READ`, `WRITE`, `NOTIFY`
- Read format: UTF-8 JSON string, truncated to the BLE ATT maximum if too long
- Notify format: UTF-8 JSON streamed in multiple chunks
- Trip object fields:
  - `id`: stable numeric trip identifier derived from the NVS slot
  - `startDate`: trip start timestamp in Unix milliseconds
  - `buckets`: array of pulse counts per 5-minute bucket
- Write commands:
  - `start`
  - `1`
  - empty write payload
  - `cancel`

### Battery

- UUID: `8a563412-3412-7856-1234-567812345678`
- Properties: `READ`
- Format: UTF-8 JSON
- Example:

```json
{"mv":3012,"pct":87}
```

### Unix Time

- UUID: `8b563412-3412-7856-1234-567812345678`
- Properties: `READ`, `WRITE`
- Read format: UTF-8 string containing Unix time in seconds
- Write format: UTF-8 string containing Unix time in seconds
- Example: `1712667000`

### Wheel Size

- UUID: `8d563412-3412-7856-1234-567812345678`
- Properties: `READ`, `WRITE`
- Read format: UTF-8 decimal string in inches with two fractional digits
- Write format: same as read format
- Example: `26.30`

## BLE Constraints

### Attribute size

A single GATT attribute value is limited to 512 bytes by ATT.

Implication:

- `READ` on the Trips characteristic is only suitable for small trip histories.
- Full trip export must use notifications.

### Notification chunk size

Trips notifications are chunked to the negotiated ATT payload size:

- payload bytes per notification = `negotiated MTU - 3`

The firmware also caps each chunk by its internal notify buffer size, which is configured from `CONFIG_BT_L2CAP_TX_MTU`.

### Ordering

Notifications are sent sequentially using `bt_gatt_notify_cb`, so chunk order is preserved within one transfer.

### Concurrency

Only one trips stream should be considered active at a time.

If the app sends `start` while a transfer is already active, the firmware may reject it with a GATT procedure-in-progress error.

## Trips Transfer Protocol

### Intent

The Trips characteristic streams a single UTF-8 JSON document over notifications.

The document shape is:

```json
{"trips":[{"id":1000,"startDate":1712667000,"buckets":[1,2,3]}]}
```

### Trip identifier semantics

Each trip object includes an `id` field.

- Type: unsigned integer
- Example: `1000`
- Source: `NVS_ID_TRIP_BASE + trip_index`

Timestamp semantics:

- `startDate` is Unix time in **seconds**.

The identifier is stable for stored trips because trips are appended to NVS slots and are not currently renumbered or compacted.

Important app rule:

- Use `id` as the canonical key in local storage.
- Do not use array index as identity.

ID examples:

- first stored trip: `1000`
- second stored trip: `1001`
- nth stored trip: `1000 + n`

The app should use `id` as the canonical trip key instead of list position.

### Start procedure

The Android app must:

1. Connect
2. Discover services
3. Enable notifications on the Trips characteristic CCCD
4. Write `start` to the Trips characteristic
5. Concatenate incoming notification payloads in arrival order
6. Detect end-of-document and parse JSON

Minimal example sequence:

1. Enable CCCD notifications for Trips
2. Write `start` to Trips characteristic
3. Collect notification chunks in order
4. Parse a complete JSON object
5. Upsert trips by `id`

### End of stream detection

The current firmware does not send an explicit transfer header, footer flag, sequence number, or checksum.

The app should consider the transfer complete when the concatenated UTF-8 payload forms a complete JSON document.

For the current JSON shape, the simplest completion check is:

- the accumulated text ends with `]}`
- the JSON parser accepts the document

The parser should be authoritative. Do not rely only on suffix matching.

### Cancel procedure

To stop a transfer, write `cancel` to the Trips characteristic.

### Failure modes

Possible failures when writing `start`:

- notifications not enabled: CCCD improper configuration
- no active matching connection: unlikely error
- transfer already active: procedure in progress

If the device disconnects, the transfer is aborted and must be restarted after reconnect.

Recommended retry strategy:

1. reconnect
2. discover services
3. enable Trips notifications again
4. issue a fresh `start`

## Recommended Android Client Flow

### Connection setup

Recommended sequence:

1. Scan for `Bike_Odometer` or for the primary service UUID
2. Connect with `autoConnect = false`
3. Wait for `STATE_CONNECTED`
4. Call `discoverServices()`
5. Request a larger MTU, ideally `517`
6. Enable notifications on Trips only after services are discovered

MTU request note:

- Android may negotiate less than requested.
- The app must work correctly with any accepted MTU.

### CCCD enable sequence

To enable notifications on Android, both of these are required:

1. `setCharacteristicNotification(characteristic, true)`
2. Write the CCCD descriptor with value `ENABLE_NOTIFICATION_VALUE`

The CCCD UUID is the standard:

- `00002902-0000-1000-8000-00805f9b34fb`

### Trips download state machine

Recommended client state machine:

- `Idle`
- `EnablingTripsNotifications`
- `Ready`
- `StartingTripsTransfer`
- `ReceivingTripsChunks`
- `TripsTransferComplete`
- `TripsTransferFailed`

Recommended rules:

- Keep one active request queue for BLE operations
- Do not issue concurrent descriptor writes and characteristic writes
- Clear the trips accumulation buffer before each new `start`
- Treat disconnect as terminal failure for the in-progress transfer

## Android Data Handling Requirements

### UTF-8 assembly

Treat each notification value as raw bytes and append to a byte buffer.

Do not decode chunk-by-chunk into independent strings and then manipulate them loosely. Decode the full assembled byte array as UTF-8 when needed, or decode incrementally in a way that preserves byte boundaries.

Recommended approach:

- accumulate in `ByteArrayOutputStream`
- after each chunk, attempt UTF-8 decode and JSON parse if desired

### JSON parsing

The Trips payload can be large. For moderate histories, full-buffer parsing is fine. For larger histories, a streaming JSON parser is safer.

Recommended parser options:

- Moshi with Okio
- kotlinx.serialization
- Jackson streaming parser

Recommended persistence behavior:

- Upsert trips by `id`
- Replace `buckets` for matching `id` with the latest payload
- Keep `startDate` as part of the trip record

### Data model

Suggested Kotlin data classes:

```kotlin
data class TripsEnvelope(
	val trips: List<TripDto>
)

data class TripDto(
	val id: Long,
	val startDate: Long,
	val buckets: List<Int>
)

data class BatteryDto(
	val mv: Int,
	val pct: Int
)
```

## Android Implementation Outline

### UUID constants

```kotlin
val BIKE_SERVICE_UUID: UUID = UUID.fromString("78563412-3412-7856-1234-567812345678")
val PULSE_UUID: UUID = UUID.fromString("79563412-3412-7856-1234-567812345678")
val TRIPS_UUID: UUID = UUID.fromString("88563412-3412-7856-1234-567812345678")
val BATTERY_UUID: UUID = UUID.fromString("8a563412-3412-7856-1234-567812345678")
val TIME_UUID: UUID = UUID.fromString("8b563412-3412-7856-1234-567812345678")
val WHEEL_SIZE_UUID: UUID = UUID.fromString("8d563412-3412-7856-1234-567812345678")
val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
```

### Trips download outline

```kotlin
suspend fun downloadTrips(gatt: BluetoothGatt): TripsEnvelope {
	val service = requireNotNull(gatt.getService(BIKE_SERVICE_UUID))
	val tripsChar = requireNotNull(service.getCharacteristic(TRIPS_UUID))

	enableNotifications(gatt, tripsChar)

	val buffer = ByteArrayOutputStream()
	beginTripsCollection(buffer)

	writeCharacteristic(gatt, tripsChar, "start".toByteArray(Charsets.UTF_8))

	val bytes = awaitTripsCompletion()
	return json.decodeFromString(bytes.toString(Charsets.UTF_8))
}
```

The actual implementation should wire `onCharacteristicChanged` into a transfer coordinator that:

- appends bytes
- attempts completion detection
- completes a coroutine or callback when full JSON is available

## Practical Recommendations

### Always request MTU

Request `517` after connection. This reduces the number of notifications required for trips transfer.

### Serialize BLE operations

Android BLE stacks are sensitive to overlapping operations. Use a single operation queue.

### Enable notifications before writing `start`

If CCCD is not enabled first, the device may reject the command.

### Add app-side timeout

Recommended transfer timeout behavior:

- reset timeout after each chunk
- fail if no new chunk arrives within a chosen window, for example 5 to 10 seconds

### Be robust to retransfers

On retries:

- clear old buffer content
- re-enable notifications if needed
- issue a fresh `start`

## Current Protocol Limitations

These are important for Android integration because they shape what the app must assume.

### No framing metadata

The current trips stream has:

- no sequence number
- no total length
- no transfer ID
- no checksum
- no explicit end marker other than the JSON structure itself

This is acceptable for a single ordered BLE notification stream, but it makes the app rely on full JSON reassembly.

### No resume support

If the connection drops mid-transfer, the app must restart the full transfer.

### No filtering or paging

The firmware currently exports the entire trips history when `start` is issued.

For larger histories, consider a future protocol revision with:

- `start:<index>`
- `start:<index>:<count>`
- binary framing
- sequence numbers and total length

## Suggested Future Protocol Revision

If the Android app becomes a long-term production client, the next protocol revision should add a small binary or JSON header per chunk.

Recommended fields:

- transfer ID
- chunk sequence
- total bytes or final-chunk flag
- payload bytes

The per-trip `id` field that exists today should remain stable across any future protocol revision.

## Test Checklist For Android

Use this checklist against the current firmware:

1. Connect and discover the primary bike service
2. Read pulse count successfully
3. Read battery JSON successfully
4. Read and write Unix time successfully
5. Read and write wheel size successfully
6. Enable notifications on Trips
7. Write `start` to Trips
8. Receive multiple notifications for non-trivial trip history
9. Reassemble valid UTF-8 JSON
10. Parse JSON into app data model
11. Retry transfer after disconnect
12. Write `cancel` during transfer and verify stream stops
