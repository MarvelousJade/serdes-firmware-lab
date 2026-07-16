# Synthetic PHY register map

All registers are 32-bit. The interface is intentionally small enough to back with MMIO, SPI, or a simulator.

| Offset | Name | Access | Description |
|---:|---|:---:|---|
| `0x0000` | `CONTROL` | W | Bit 0 performs reset |
| `0x0004` | `STATUS` | R | Bit 0 PLL locked; bit 1 measurement done; bit 2 measurement fault |
| `0x0008` | `CTLE_CODE` | R/W | Unsigned code 0–7; higher writes saturate |
| `0x000C` | `DFE_TAP0` | R/W | Signed 8-bit code in bits 7:0, saturated to -63…63 |
| `0x0010` | `DFE_TAP1` | R/W | Signed 8-bit code in bits 7:0, saturated to -63…63 |
| `0x0014` | `DFE_TAP2` | R/W | Signed 8-bit code in bits 7:0, saturated to -63…63 |
| `0x0018` | `PATTERN_SEED` | W | Nonzero 31-bit PRBS seed; zero selects all ones |
| `0x001C` | `MEASURE_SYMBOLS` | R/W | Symbols in the next measurement window |
| `0x0020` | `MEASURE_CONTROL` | W | Bit 0 start; bit 1 supervised-training mode |
| `0x0024` | `ERROR_COUNT` | R | PRBS decision errors in the completed window |
| `0x0028` | `SYMBOL_COUNT` | R | Symbols processed in the completed window |
| `0x002C` | `MSE_Q16` | R | Unsigned Q16 mean squared slicer error |
| `0x0030` | `MARGIN_Q16` | R | Signed Q16 mean target-aligned slicer margin |
| `0x0034` | `ERROR_CORR0` | R | Signed error correlation for DFE tap 0 |
| `0x0038` | `ERROR_CORR1` | R | Signed error correlation for DFE tap 1 |
| `0x003C` | `ERROR_CORR2` | R | Signed error correlation for DFE tap 2 |

Read-only writes are ignored by the behavioral backend. A measurement started before PLL lock, or with a zero symbol count, sets both `measurement done` and `measurement fault`; the driver returns an invalid measurement instead of consuming stale counters.

