---
id: aisstream
name: aisstream.io
rows: [ais]
catalogue: []
access: free key
probed: 2026-11-05
confidence: docs   # no key was used, so the stream was not opened: the schema comes from aisstream's documentation page, which embeds it
---

# aisstream.io

A live WebSocket feed of AIS messages: ship positions and voyage data, plus base stations, aircraft, aids to navigation and safety messages. It keeps no history.

## Contents
The documentation's schema explorer lists 28 models, all in `catalog/aisstream.tsv` (293 fields: model, category, field, type, required, description).

- **Envelope (4)**: `AisStreamMessage` (`MetaData`, `MessageType`, `Message`), `SubscriptionMessage`, `SubscriptionConfirmation`, `SubscriptionConfirmationMessage` (`CompressionEnabled`).
- **Position (6)**: `PositionReport` (17 fields), `StandardClassBPositionReport` (22), `ExtendedClassBPositionReport` (21), `LongRangeAisBroadcastMessage` (13), `BaseStationReport` (18), `StandardSearchAndRescueAircraftReport` (19).
- **Static and voyage (2)**: `ShipStaticData` (16), `StaticDataReport` (8).
- **Binary data (6)**: `AddressedBinaryMessage`, `BinaryBroadcastMessage`, `GnssBroadcastBinaryMessage`, `BinaryAcknowledge`, `SingleSlotBinaryMessage`, `MultiSlotBinaryMessage`.
- **Safety (3)**: `AddressedSafetyMessage`, `SafetyBroadcastMessage`, `AidsToNavigationReport`.
- **Network and control (7)**: `CoordinatedUTCInquiry`, `GroupAssignmentCommand`, `Interrogation`, `AssignedModeCommand`, `DataLinkManagementMessage`, `ChannelManagement`, `UnknownMessage`.

`FilterMessageTypes` accepts 25 names, the message models above plus `DataLinkManagementMessageData`.

`PositionReport` fields: `MessageID`, `RepeatIndicator`, `UserID`, `Valid`, `NavigationalStatus`, `RateOfTurn`, `Sog`, `PositionAccuracy`, `Longitude`, `Latitude`, `Cog`, `TrueHeading`, `Timestamp`, `SpecialManoeuvreIndicator`, `Spare`, `Raim`, `CommunicationState`.

`ShipStaticData` fields: `MessageID`, `RepeatIndicator`, `UserID`, `Valid`, `AisVersion`, `ImoNumber`, `CallSign`, `Name`, `Type`, `Dimension`, `FixType`, `Eta`, `MaximumStaticDraught`, `Destination`, `Dte`, `Spare`.

`MetaData` (from the documentation's example): `MMSI`, `ShipName`, `Latitude`, `Longitude`.

Coverage and rhythm: event-driven, no schedule. The service states no SLA and does not replay events.

## How to call it
- `wss://stream.aisstream.io/v0/stream`. Send one JSON subscription within 3 seconds of connecting: `APIKey`, `BoundingBoxes` (required), `FiltersShipMMSI` (up to 200 nine-character strings, optional), `FilterMessageTypes` (optional).
- Answers are binary frames of UTF-8 JSON. Each accepted subscription returns a `SubscriptionConfirmation`; an invalid one returns nothing.
- Limits stated by the documentation: 3 subscribed connections per account, 3 open connections per IP, 1 subscription update per second per connection. A slow reader has messages dropped. Direct browser connections are not permitted.
- **Trap:** "Beginning in September 2026, uncompressed connections will be subject to per-user bandwidth limits, and messages exceeding those limits will be dropped." The documentation asks for `permessage-deflate`. Whether Qt's `QWebSocket` negotiates it was not tested here; the app's code sets no such option and asks for the whole world (see below), so the Map may be losing messages since September. Check `SubscriptionConfirmation.Message.CompressionEnabled`, which the app does not read.

## What the app reads
Row `ais`, `src/SourcesPage.cpp:335`. Client: `src/AisStream.cpp`.
- URL `src/AisStream.cpp:13`. Subscription `src/AisStream.cpp:55-57`: the key, one bounding box over the whole world, types `PositionReport` and `ShipStaticData` only.
- Envelope `src/AisStream.cpp:169-175`: `MessageType`, `MetaData.MMSI` or `mmsi` (else the body's `UserID`), `MetaData.ShipName` or `shipName`. An `error` key is shown as the status (`src/AisStream.cpp:158`).
- `PositionReport` `src/AisStream.cpp:177-180`: `Latitude`, `Longitude`, `Sog`, `Cog`, `TrueHeading`.
- `ShipStaticData` `src/AisStream.cpp:181-187`: `Name`, `Type`, `Destination`, `CallSign`, `ImoNumber`, `MaximumStaticDraught`, `Dimension.A` + `Dimension.B` as the length.

## Not read yet
- `NavigationalStatus` and `RateOfTurn` of `PositionReport`: at anchor, moored, under way, for the Map's ships and congestion at ports.
- `Eta` of `ShipStaticData`: arrivals by port, for the chokepoints and supply-chain views.
- `StandardClassBPositionReport`, `ExtendedClassBPositionReport`: smaller vessels, which the current filter drops.
- `FiltersShipMMSI` and narrower `BoundingBoxes`: following a fleet or a zone would cut the volume.

## Calls made
- `GET https://aisstream.io/robots.txt` → 200, 1248 bytes.
- `GET https://aisstream.io/documentation` → 200, 80723 bytes.
- `GET https://aisstream.io/` → 200, 16369 bytes.

Not called: `wss://stream.aisstream.io/v0/stream` (no key, as instructed).
