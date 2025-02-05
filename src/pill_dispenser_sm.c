#include "pill_dispenser_sm.h"
#include "buttons.h"
#include "dispenser.h"
#include "led.h"
#include "lib/debug.h"
#include "lora.h"
#include "storage.h"

static dispenser_state_t dispenser_state;
static int slices_ran;
static bool calibrated;

void run_dispenser_sm(dispenser_sm *dispenser_sm_ptr) {
    switch (dispenser_sm_ptr->state) {
    case stStart:
        // load dispenser slices ran, if failed to load, set to 0
        if (!load_dispenser_slice_ran(&slices_ran)) {
            save_dispenser_slice_ran(0);
            restore_dispenser_slices_ran(0);
        } else {
            restore_dispenser_slices_ran(slices_ran);
        }
        // load dispenser state, if failed to load, set to DISPENSER_TURNING to force an error calibration
        if (!load_dispenser_state(&dispenser_state)) {
            save_dispenser_state(DISPENSER_TURNING);
            dispenser_state = DISPENSER_TURNING;
        }
        // check if dispenser is calibrated
        if (!load_dispenser_calibrated(&calibrated)) {
            save_dispenser_calibrated(false);
            calibrated = false;
        }

        if (!calibrated) {
            // not calibrated, go to calibration
            dispenser_sm_ptr->state = stCalibWait;
            enable_buttons();
            dprintf(DEBUG_LEVEL_INFO, "Wait calib\n");
        } else if (dispenser_state == DISPENSER_TURNING) {
            // calibrated and turning, i.e. dispensing and power lost, go to error state
            send_message(POWER_OFF_DURING_TURNING, "Powered Off During Turn");
            dispenser_sm_ptr->state = stError;
            dprintf(DEBUG_LEVEL_INFO, "Re-calib from err\n");
        } else if (slices_ran == 0) {
            // calibrated and idle, and haven't dispensed yet. wait for user press button to start dispense
            dispenser_sm_ptr->state = stDispenseWait;
            dprintf(DEBUG_LEVEL_INFO, "Wait dispense\n");
            enable_buttons();
        } else {
            // calibrated and idle, have dispensed, continue to dispense
            dispenser_sm_ptr->state = stDispense;
            dprintf(DEBUG_LEVEL_INFO, "Ready to dispense\n");
        }
        break;
    case stError:
        error_calibration();
        dispenser_sm_ptr->state = stDispense;
        dprintf(DEBUG_LEVEL_INFO, "Error resolved, ready\n");
        break;
    case stCalibWait:
        toggle_led();
        if (get_button_event() != EVENT_NONE) {
            dispenser_sm_ptr->state = stCalib;
            dprintf(DEBUG_LEVEL_INFO, "Calib start\n");
            disable_buttons();
        }
        break;
    case stCalib:
        align_dispenser(1);
        dispenser_sm_ptr->state = stDispenseWait;
        save_dispenser_calibrated(true);
        dprintf(DEBUG_LEVEL_INFO, "Calib done\n");
        dprintf(DEBUG_LEVEL_INFO, "Wait dispense\n");
        enable_buttons();
        break;
    case stDispenseWait:
        set_led(true);
        if (get_button_event() != EVENT_NONE) {
            dispenser_sm_ptr->state = stDispense;
            dprintf(DEBUG_LEVEL_INFO, "Dispense start\n");
            disable_buttons();
        }
        break;
    case stDispense:
        dispense_all_pills();
        dispenser_sm_ptr->state = stStart;
        save_dispenser_calibrated(false);
        dprintf(DEBUG_LEVEL_INFO, "Dispense done\n");
        break;
    }
}