# Tears of Kyne

Local SKSE water-need mod with a local PrismaUI HUD.

## Runtime Notes & Configuration

### Keybinds
*   **FillKey** (Default: `G`): Press this key while standing in water to fill your waterskin.
*   **MenuKey** (Default: `F10`): Opens the in-game settings configuration window.
*   **ToggleHUDKey** (Default: `NUM1`): Toggles the visibility of the HUD widget on or off.

### HUD Positioning & Scale
*   **HudX & HudY**: Controls the screen coordinates for the HUD widget. You can drag the widget freely while the settings menu is open or use the in-game sliders.
*   **MenuX & MenuY**: Remembers the last screen position of the settings window itself so it stays where you last dragged it.
*   **WidgetScale**: Adjusts the overall size of the HUD widget (represented as a percentage).

---

## Gameplay & Themes

### WidgetStyle
*   **Default:** `0`
*   Changes the visual style of the UI widget (e.g., Default, Minimal, etc.).

### Difficulty Modes
*   **Default:** `2` (Medium)
*   Sets the gameplay difficulty scaling for the mod mechanics.

| Difficulty | Thirst Increase Rate | Hydration Restored per Drink |
| :--- | :--- | :--- |
| **Easy** | 3 thirst / hour | 65 hydration |
| **Medium** | 6.5 thirst / hour | 55 hydration |
| **Hard** | 10 thirst / hour | 45 hydration |
| **Very Hard** | 16 thirst / hour | 35 hydration |

### Accessibility Theme
*   **Default:** `0`
*   Toggles high-contrast or alternative color themes for better UI readability.

| Value | Theme / Condition |
| :--- | :--- |
| `0` | Default |
| `1` | Protanopia (Red-blindness) |
| `2` | Deuteranopia (Green-blindness) |
| `3` | Tritanopia (Blue-blindness) |
| `4` | Protanomaly (Red-weakness) |
| `5` | Deuteranomaly (Green-weakness) |