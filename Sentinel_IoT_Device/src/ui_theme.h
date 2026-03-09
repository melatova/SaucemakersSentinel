/*
 * Project:     Saucier’s Sentinel - Hot Process Timer
 * File:        ui_theme.h  —  display layout, font sizes, and timing constants
 * Author:      Nicole
 * Date:        03/07/2026
 *
 * Move text rows, resize icons, or alter
 * animations — changing it here changes it globally.
 *
 * The display is a 128x64 pixel OLED (SSD1306, monochrome).
 * x goes left to right (0-127).
 * y goes top to bottom (0-63).
 *
 * ── SCREEN LAYOUT ───────────────────────────────────────────────
 *
 *   x=0                    x=110 x=127
 *   +──────────────────────+─────+  y=0
 *   |  BIG degF  (size 2)  |16x16|  y=0-15   <- temperature + icon
 *   +──────────────────────+ icon|
 *   |  small degC (size 1) |     |  y=18-25  <- secondary temp
 *   +──────────────────────+─────+  y=28     <- separator line
 *   |  status row                |  y=31     <- e.g. "Left: 09:47"
 *   |  message row 1             |  y=41     <- e.g. "Turn up heat"
 *   |  message row 2             |  y=52     <- e.g. "Hold 2s cancel"
 *   +────────────────────────────+  y=63
 *
 *   Progress bar (cycle screen only) lives below the messages:
 *   +── full width, y=56 to y=62
 */

#ifndef UI_THEME_H
#define UI_THEME_H


// ── FONT SIZES ───────────────────────────────────────────────────
// Adafruit GFX text size multiplies the base 6x8px character.
// Size 1 = 6x8px  (fits 21 chars across 128px — labels)
// Size 2 = 12x16px (fits 10 chars — the big temperature number)
const int FONT_HERO     = 2;   // big degF temperature reading, top-left
const int FONT_HEADLINE = 2;   // screen titles like "BRAVO!" and "CANCELLING"
const int FONT_BODY     = 1;   // all labels and messages


// ── TEMPERATURE THRESHOLDS ───────────────────────────────────────
// Hot-process safety range per FDA guidelines.
// The state machine in main cpp reads these.
// Changing them here updates them everywhere.
const float TEMP_SAFE_LOW  = 185.0;   // degF — below this: sauce not safe yet
const float TEMP_SAFE_HIGH = 200.0;   // degF — above this: sauce could scorch


// ── SCREEN DIMENSIONS ────────────────────────────────────────────
const int SCREEN_W = 128;   // pixels wide
const int SCREEN_H =  64;   // pixels tall


// ── ICON (16x16, top-right corner) ──────────────────────────────
// Each screen draws its own icon here. The x position is calculated
// so the icon's right edge lands 2px from the screen edge.
const int ICON_W       = 16;
const int ICON_H       = 16;
const int ICON_X_RIGHT = 110;   // = SCREEN_W(128) - ICON_W(16) - margin(2)
const int ICON_Y       =   0;   // icons always sit at the very top


// ── TEMPERATURE BANNER (top-left, rows 0-27) ─────────────────────
// Big degF number sits at the top-left corner.
// The smaller degC reading appears just below it.
const int TEMP_X  =  0;   // degF text: left edge of screen
const int TEMP_Y  =  0;   // degF text: top of screen
const int TEMPC_X =  0;   // degC text: same left edge
const int TEMPC_Y = 18;   // degC text: 16px font height + 2px breathing room


// ── SEPARATOR LINE ───────────────────────────────────────────────
// A horizontal line drawn across the full width at this y position.
// Divides the temperature area (top) from messages (bottom).
const int SEPARATOR_Y = 28;


// ── MESSAGE ROWS (below separator) ───────────────────────────────
// Three text rows, each using FONT_BODY (8px tall).
// ROW_STATUS is the most prominent — timers and alert titles live here.
const int ROW_STATUS = 31;   // top row: status, timer, or alert title
const int ROW_MSG1   = 41;   // middle row: instructions or description
const int ROW_MSG2   = 52;   // bottom row: secondary message


// ── PROGRESS BAR (cycle screen only) ────────────────────────────
// A full-width bar in the bottom strip that fills as the cycle runs.
// It sits below ROW_MSG2 so they do not overlap.
const int PROG_X = 0;     // left edge
const int PROG_Y = 56;    // just below the last text row
const int PROG_W = 128;   // full screen width
const int PROG_H = 6;     // 6px tall — bottom lands at y=62, 1px from edge


// ── ANIMATION TIMING ─────────────────────────────────────────────
// How many milliseconds each animation frame is shown on screen.
// Larger number = slower animation. These are used by animFrame().
const int ANIM_SPEED =  200;   // general sparkles and steam puffs
const int SHIVER_MS  =  150;   // thermometer shiver on too-cold alert
const int FLASH_MS   =  120;   // flame flicker on too-hot alert
const int SPINNER_MS =  150;   // the /-\| spinner on cancelling screen


// ── LAYOUT GRID HELPERS ──────────────────────────────────────────
// Handy spacing values if you ever add new UI elements.
const int GRID_SMALL  =  4;
const int GRID_MEDIUM =  8;
const int GRID_LARGE  = 12;


#endif // UI_THEME_H
