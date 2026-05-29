# Tears of Kyne

Local SKSE water-need mod with a local PrismaUI HUD.

## Runtime notes

Configuration 
Keybinds
FillKey (Default: G): Press this key while standing in water to fill your waterskin.
MenuKey (Default: F10): Opens the in-game settings configuration window.
ToggleHUDKey (Default: NUM1): Toggles the visibility of the HUD widget on or off.
HUD Positioning & Scale
HudX & HudY: Controls the screen coordinates for the HUD widget. You can drag the widget freely while the settings menu is open or use the in-game sliders.
MenuX & MenuY: Remembers the last screen position of the settings window itself so it stays where you last dragged it.
WidgetScale: Adjusts the overall size of the HUD widget (represented as a percentage).
Gameplay & Themes
WidgetStyle (Default: 0): Changes the visual style of the UI widget (e.g., Default, Minimal, etc.).
Difficulty (Default: 2): Sets the gameplay difficulty scaling for the mod mechanics.
Easy: Your hydration increases slowly at 3 thirst per hour, and each drink restores 65 hydration.
Medium: Hydration rises at 6.5 thirst per hour, while drinks restore 55 hydration.
Hard: Thirst increases at 10 thirst per hour, and drinks restore 45 hydration.
Very Hard: Hydration climbs rapidly at 16 thirst per hour, while drinks only restore 35 hydration.
AccessibilityTheme (Default: 0): Toggles high-contrast or alternative color themes for better UI readability.
0 = Default
1 = Protanopia (Red-blindness)
2 = Deuteranopia (Green-blindness)
3 = Tritanopia (Blue-blindness)
4 = Protanomaly (Red-weakness)
5 = Deuteranomaly (Green-weakness)