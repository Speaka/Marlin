/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

//
// Bed Tramming Wizard
//

#include "../../inc/MarlinConfigPre.h"

#if BOTH(HAS_LCD_MENU, ASSISTED_TRAMMING_WIZARD)

#include "menu_item.h"

#include "../../feature/tramming.h"

#include "../../module/motion.h"
#include "../../module/probe.h"
#include "../../gcode/queue.h"

//#define DEBUG_OUT 1
#include "../../core/debug_out.h"

float z_measured[G35_PROBE_COUNT] = { 0 };
static uint8_t tram_index = 0;
static uint8_t reference = 0;
bool already_probing=false;
bool auto_probing =false;

bool probe_single_point() {
  // In BLTOUCH HS mode, the probe travels in a deployed state.
  // Users of Tramming Wizard might have a badly misaligned bed, so raise Z by the
  // length of the deployed pin (BLTOUCH stroke < 7mm)
  if(!already_probing){
    already_probing = true;//lock multiple probing instances
    do_blocking_move_to_z((Z_CLEARANCE_BETWEEN_PROBES) + TERN0(BLTOUCH_HS_MODE, 7));
    const float z_probed_height = probe.probe_at_point(screws_tilt_adjust_pos[tram_index], PROBE_PT_RAISE, 0, true);
    DEBUG_ECHOLNPAIR("probe_single_point: ", z_probed_height, "mm");
    z_measured[tram_index] = z_probed_height;

    #ifdef ASSISTED_TRAMMING_WAIT_POSITION
      // Move XY to safe position
      if (DEBUGGING(LEVELING)) DEBUG_ECHOLNPGM("Moving away");
      const xyz_pos_t wait_pos = ASSISTED_TRAMMING_WAIT_POSITION;
      do_blocking_move_to(wait_pos, XY_PROBE_FEEDRATE_MM_S);
    #endif
    already_probing = false;
    return !isnan(z_probed_height);
  }
  return false;
}

void _menu_single_probe(const uint8_t point) {
  if(point>=G35_PROBE_COUNT) tram_index=0;
  else tram_index = 0;
  ui.defer_status_screen();
  DEBUG_ECHOLNPAIR("Screen: single probe screen Arg:", point);
  START_MENU();
  STATIC_ITEM(MSG_LEVEL_CORNERS, SS_LEFT);
  STATIC_ITEM(MSG_ADJUST_BED_SP, SS_LEFT, ftostr42_52(z_measured[tram_index] - z_measured[reference])); // Print diff
  #ifdef ASSISTED_TRAMMING_WIZARD_AUTO  // should not be used with clear after probing
  ACTION_ITEM(MSG_BUTTON_NEXT, []{ auto_probing = false; _menu_single_probe(tram_index+1);}); // Next Point
  ACTION_ITEM(MSG_BUTTON_DONE, []{ auto_probing = false; ui.goto_previous_screen_no_defer(); }); // Back
  END_MENU();
  
  auto_probing = true;
  while (auto_probing) { // Also stop on tolerance reached? LEVEL_CORNERS_PROBE_TOLERANCE from level corner PR
    if (probe_single_point()) ui.refresh();
    idle();
  }
  auto_probing = false;
  #else
  ACTION_ITEM(MSG_UBL_BC_INSERT2, []{ if (probe_single_point()) ui.refresh(); }); // Manually initiate probing
  ACTION_ITEM(MSG_BUTTON_NEXT, []{ _menu_single_probe(tram_index+1);}); // Next Point
  ACTION_ITEM(MSG_BUTTON_DONE, []{ ui.goto_previous_screen_no_defer(); }); // Back  
  END_MENU();
  #endif 
}

void tramming_wizard_menu() {
  DEBUG_ECHOLNPAIR("Screen: tramming_wizard_menu");
  START_MENU();
  STATIC_ITEM(MSG_SELECT_REFERENCE);

  // Draw a menu item for each tramming point
  LOOP_L_N(i, G35_PROBE_COUNT)
    SUBMENU_N_P(i, (char*)pgm_read_ptr(&tramming_point_name[i]), []{reference=MenuItemBase::itemIndex; _menu_single_probe(MenuItemBase::itemIndex); });

  ACTION_ITEM(MSG_BUTTON_DONE, []{ ui.goto_previous_screen_no_defer(); });
  END_MENU();
}

// Init the wizard and enter the submenu
void goto_tramming_wizard() {
  DEBUG_ECHOLNPAIR("Screen: goto_tramming_wizard", 1);
  tram_index = 0;
  ui.defer_status_screen();
  //probe_single_point(); // Probe first point to get differences

  // Inject G28, wait for homing to complete,
  set_all_unhomed();
  queue.inject_P(G28_STR);
  ui.goto_screen([]{
    _lcd_draw_homing();
    if (all_axes_homed())
      ui.goto_screen(tramming_wizard_menu);
  });
}

#endif // HAS_LCD_MENU && ASSISTED_TRAMMING_WIZARD
