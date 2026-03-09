# ReverseGeocache — Actor Communication Flow

```
  ┌──────────────┐         Published via Bus            ┌──────────────┐
  │  InputActor  │─── SIG_BUTTON_A_PRESS ──────────────▶│   BoxActor   │
  │   (Pri 3)    │─── SIG_BUTTON_A_HOLD  ──────────────▶│   (Pri 2)    │
  │              │                                      │              │
  │ Polls pins   │                                      │ Coordinator  │
  │ 5 & 11 @20Hz │                                      │ Servo/Buzzer │
  │ Debounce     │                                      │ State persist│
  └──────────────┘                                      └──────┬───┬──┘
                                                               │   │
                         Direct commands (postEvent)           │   │
               ┌───── SIG_GPS_START/STOP ──────────────────────┘   │
               │       ┌── SIG_NAV_SET_WP/STOP ─────-──────────────┘
               │       │
               ▼       │        Published via Bus
  ┌──────────────┐     │                                ┌──────────────┐
  │   GpsActor   │     │    SIG_BOX_STATE ─────────────▶│ DisplayActor │
  │   (Pri 2)    │     │    SIG_WAYPOINT_INFO ─────────▶│   (Pri 1)    │
  │              │     │                                │              │
  │ PA1010D I2C  │     │                                │ ST7789 TFT   │
  │ NMEA parsing │     │                                │ 240x240 SPI1 │
  └──────┬───────┘     │                                └──────────────┘
         │             │                                       ▲
         │             ▼                                       │
         │    ┌──────────────┐                                 │
         │    │   NavActor   │                                 │
         │    │   (Pri 2)    │── SIG_NAV_UPDATE ───────────────┘
         │    │              │
         │    │ Distance/    │── SIG_NAV_ARRIVED ──▶ BoxActor
         │    │ Bearing calc │
         │    └──────────────┘
         │             ▲
         │             │
         └─────────────┘
      SIG_GPS_FIX (via Bus → NavActor AND BoxActor)
      SIG_GPS_SATS (via Bus → DisplayActor)
      SIG_GPS_NO_FIX (via Bus → BoxActor)
```

## Signal Flow Summary

### Published (Bus)

| Source | Signal | Subscribers |
|--------|--------|-------------|
| InputActor | `SIG_BUTTON_A_PRESS` | BoxActor |
| InputActor | `SIG_BUTTON_A_HOLD` | BoxActor |
| GpsActor | `SIG_GPS_FIX` | NavActor, BoxActor |
| GpsActor | `SIG_GPS_SATS` | DisplayActor |
| GpsActor | `SIG_GPS_NO_FIX` | BoxActor |
| NavActor | `SIG_NAV_UPDATE` | DisplayActor |
| NavActor | `SIG_NAV_ARRIVED` | BoxActor |
| BoxActor | `SIG_BOX_STATE` | DisplayActor |
| BoxActor | `SIG_WAYPOINT_INFO` | DisplayActor |

### Direct Commands (postEvent)

| Source | Signal | Target |
|--------|--------|--------|
| BoxActor | `SIG_GPS_START` | GpsActor |
| BoxActor | `SIG_GPS_STOP` | GpsActor |
| BoxActor | `SIG_NAV_SET_WP` | NavActor |
| BoxActor | `SIG_NAV_STOP` | NavActor |

### Internal Timer Signals (self-posted)

| Actor | Signal | Purpose |
|-------|--------|---------|
| InputActor | `SIG_POLL_BUTTONS` | 20Hz button polling |
| GpsActor | `SIG_READ_GPS` | 1Hz GPS read |
| GpsActor | `SIG_GPS_TIMEOUT` | 30s fix timeout |
| BoxActor | `SIG_SLEEP_TIMEOUT` | 5min auto-sleep |
| BoxActor | `SIG_TONE_DONE` | Melody note sequencer |
| BoxActor | `SIG_SERVO_DONE` | Servo move complete |

## Actor Summary

| Actor | Queue | Stack | Priority | Responsibility |
|-------|-------|-------|----------|----------------|
| InputActor | 4 | 512B | 3 (highest) | Poll buttons, debounce, detect press/hold |
| GpsActor | 8 | 1024B | 2 | Read PA1010D over I2C, parse NMEA, publish fixes |
| NavActor | 8 | 512B | 2 | Waypoint management, distance/bearing calculation |
| BoxActor | 8 | 1024B | 2 | Servo, buzzer, state persistence, central coordinator |
| DisplayActor | 8 | 2048B | 1 (lowest) | TFT rendering, subscribes to all display-relevant signals |
