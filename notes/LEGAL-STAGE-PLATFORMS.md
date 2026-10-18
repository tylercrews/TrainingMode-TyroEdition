# Legal stage platform positions

Reference coordinates for the platforms on the legal stages, plus the points Low Laser Puff Practice uses for **On … Plat** and **Under … Plat**. Coordinates are in stage units with X = 0 at stage center and Y = 0 at the main floor (except where a stage's floor isn't flat).

These are **static** values. They come from each stage DAT's collision lines in the vanilla ISO, scaled by `grGroundParam`. The events don't hardcode these values. Low Laser and Techchase read the live collision lines at runtime. The table is for planning, checking and future features.

## How the points are chosen

These are the same rules as `ResolvePosition` in [src/lowlaser.c](../src/lowlaser.c):

- A **platform** is a floor line with native flag `0x0100` (one-way) that is at least 10 units wide.
- **Left/Right** is the platform nearest center whose midpoint is beyond X = ±10. **Top** is the highest platform spanning X = 0.
- **On plat** is the platform's midpoint, on its surface.
- **Under plat** is the highest solid (non-platform) floor at that same X.
- **Main floor X** is the leftmost and rightmost endpoint of the solid floor lines.

Only these line groups count:

- **Pokémon Stadium:** its base-layout group (floor lines 34–36 and 51–54). Transformations are off with hazards disabled.
- **Yoshi's Story:** solid floor lines 1–6, plus the three platform lines. Randall's line is excluded because Randall is removed with hazards disabled.

## Positions

| Stage (external ID) | Main floor X | Platform | Line (left → right) | On plat | Under plat |
| --- | --- | --- | --- | --- | --- |
| Final Destination (32) | −85.5657 .. 85.5657 | none | – | – | – |
| Battlefield (31) | −68.4 .. 68.4 | Left | (−57.6, 27.2) → (−20.0, 27.2) | (−38.8, 27.2) | (−38.8, 0.0) |
| | | Top | (−18.8, 54.4) → (18.8, 54.4) | (0.0, 54.4) | – |
| | | Right | (20.0, 27.2) → (57.6, 27.2) | (38.8, 27.2) | (38.8, 0.0) |
| Dream Land (28) | −77.2713 .. 77.2713 | Left | (−61.3929, 30.1421) → (−31.7254, 30.1421) | (−46.5591, 30.1421) | (−46.5591, 0.0088) |
| | | Top | (−19.0181, 51.4253) → (19.0171, 51.4253) | (−0.0005, 51.4253) | – |
| | | Right | (31.7036, 30.2425) → (63.0745, 30.2425) | (47.3891, 30.2425) | (47.3891, 0.0088) |
| Yoshi's Story (8) | −56.0 .. 56.0 | Left | (−59.5, 23.45) → (−28.0, 23.45) | (−43.75, 23.45) | (−43.75, −0.9479) |
| | | Top | (−15.75, 42.0) → (15.75, 42.0) | (0.0, 42.0) | – |
| | | Right | (28.0, 23.45) → (59.5, 23.45) | (43.75, 23.45) | (43.75, −0.9479) |
| Pokémon Stadium (3) | −87.75 .. 87.75 | Left | (−55.0, 25.0) → (−25.0, 25.0) | (−40.0, 25.0) | (−40.0, 0.0) |
| | | Top | none | N/A | N/A |
| | | Right | (25.0, 25.0) → (55.0, 25.0) | (40.0, 25.0) | (40.0, 0.0) |
| Fountain of Dreams (2) | −63.3476 .. 63.3476 | Left / Top / Right | moving (see below) | live only | live only |

Some details worth knowing:

- Dream Land's platforms aren't symmetric. The right side platform sits about 0.1 higher and is shifted about 0.8 right.
- Yoshi's main floor dips to about −0.95 under the side platforms, where it starts sloping toward the edges.

## Fountain of Dreams

Fountain's three platform lines are stored in joint-local coordinates. In the DAT all three read as (−14.25, 1.12) → (14.25, 1.12), and they're placed and moved by the stage's joints at runtime. The side platforms rise and fall continuously, so a static table can't give their positions. The main floor is static at X = ±63.3476.

Low Laser handles Fountain by reading the live line every frame. A Puff placed on a side platform rides it, and if the line is disabled or no longer under Puff, Puff uses the main floor until it returns. To get real numbers for Fountain, read `stc_collvert[...].pos_curr` in Dolphin's memory view. Alternatively, log the `FindPlatform` result from a debug build.

## Puff Height offsets

Low Laser's **Puff Height** adds one of these to the On/Under point. They are Jigglypuff's apex rises, computed from its ISO jump values (see [the Low Laser doc](../docs/LOW-LASER-PUFF.md#heights)):

| Height | Rise |
| --- | --- |
| Short Hop | 8.096 |
| Full Hop | 19.2 |
| Short Hop + Double Jump | 28.546 |
| Full Hop + Double Jump | 39.65 |

## Regenerating

```
python -I scripts/legal_stage_positions.py melee.iso
```

The script prints this table, without Fountain, from a vanilla ISO. `LowLaserStageTests` in [tests/test_lowlaser.py](../tests/test_lowlaser.py) also loads these same ISO lines and checks that the event's live placement matches these rules on every legal stage except Fountain.
