/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{
 * @addtogroup Autotuning module
 * @brief Reads from @ref ManualControlCommand and fakes a rate mode while
 *        toggling the channels to relay mode
 * @{
 *
 * @file       autotune.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2012.
 * @brief      Module to handle all comms to the AHRS on a periodic basis.
 *
 * @see        The GNU Public License (GPL) Version 3
 *
 ******************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/**
 * Input objects: None, takes sensor data via pios
 * Output objects: @ref AttitudeRaw @ref AttitudeActual
 *
 * This module computes an attitude estimate from the sensor data
 *
 * The module executes in its own thread.
 *
 * UAVObjects are automatically generated by the UAVObjectGenerator from
 * the object definition XML file.
 *
 * Modules have no API, all communication to other modules is done through UAVObjects.
 * However modules may use the API exposed by shared libraries.
 * See the OpenPilot wiki for more details.
 * http://www.openpilot.org/OpenPilot_Application_Architecture
 *
 */

#include <openpilot.h>

#include "flightstatus.h"
#include "hwsettings.h"
#include "manualcontrolcommand.h"
#include "manualcontrolsettings.h"
#include "relaytuning.h"
#include "relaytuningsettings.h"
#include "stabilizationdesired.h"
#include "stabilizationsettings.h"
#include "taskinfo.h"

// Private constants
#define STACK_SIZE_BYTES 1024
#define TASK_PRIORITY    (tskIDLE_PRIORITY + 2)

// Private types
enum AUTOTUNE_STATE { AT_INIT, AT_START, AT_ROLL, AT_PITCH, AT_FINISHED, AT_SET };

// Private variables
static xTaskHandle taskHandle;
static bool autotuneEnabled;

// Private functions
static void AutotuneTask(void *parameters);
static void update_stabilization_settings();

/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
int32_t AutotuneInitialize(void)
{
    // Create a queue, connect to manual control command and flightstatus
#ifdef MODULE_AUTOTUNE_BUILTIN
    autotuneEnabled = true;
#else
    HwSettingsInitialize();
    uint8_t optionalModules[HWSETTINGS_OPTIONALMODULES_NUMELEM];

    HwSettingsOptionalModulesGet(optionalModules);

    if (optionalModules[HWSETTINGS_OPTIONALMODULES_AUTOTUNE] == HWSETTINGS_OPTIONALMODULES_ENABLED) {
        autotuneEnabled = true;
    } else {
        autotuneEnabled = false;
    }
#endif

    return 0;
}

/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
int32_t AutotuneStart(void)
{
    // Start main task if it is enabled
    if (autotuneEnabled) {
        xTaskCreate(AutotuneTask, (signed char *)"Autotune", STACK_SIZE_BYTES / 4, NULL, TASK_PRIORITY, &taskHandle);

        PIOS_TASK_MONITOR_RegisterTask(TASKINFO_RUNNING_AUTOTUNE, taskHandle);
        PIOS_WDG_RegisterFlag(PIOS_WDG_AUTOTUNE);
    }
    return 0;
}

MODULE_INITCALL(AutotuneInitialize, AutotuneStart);

/**
 * Module thread, should not return.
 */
static void AutotuneTask(__attribute__((unused)) void *parameters)
{
    // AlarmsClear(SYSTEMALARMS_ALARM_ATTITUDE);

    enum AUTOTUNE_STATE state = AT_INIT;

    portTickType lastUpdateTime = xTaskGetTickCount();

    while (1) {
        PIOS_WDG_UpdateFlag(PIOS_WDG_AUTOTUNE);
        // TODO:
        // 1. get from queue
        // 2. based on whether it is flightstatus or manualcontrol

        portTickType diffTime;

        const uint32_t PREPARE_TIME = 2000;
        const uint32_t MEAURE_TIME = 30000;

        FlightStatusData flightStatus;
        FlightStatusGet(&flightStatus);

        // Only allow this module to run when autotuning
        if (flightStatus.FlightMode != FLIGHTSTATUS_FLIGHTMODE_AUTOTUNE) {
            state = AT_INIT;
            vTaskDelay(50);
            continue;
        }

        StabilizationDesiredData stabDesired;
        StabilizationDesiredGet(&stabDesired);

        StabilizationSettingsData stabSettings;
        StabilizationSettingsGet(&stabSettings);

        ManualControlSettingsData manualSettings;
        ManualControlSettingsGet(&manualSettings);

        ManualControlCommandData manualControl;
        ManualControlCommandGet(&manualControl);

        RelayTuningSettingsData relaySettings;
        RelayTuningSettingsGet(&relaySettings);

        bool rate = relaySettings.Mode == RELAYTUNINGSETTINGS_MODE_RATE;

        if (rate) { // rate mode
            stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL] = STABILIZATIONDESIRED_STABILIZATIONMODE_RATE;
            stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH] = STABILIZATIONDESIRED_STABILIZATIONMODE_RATE;

            stabDesired.Roll = manualControl.Roll * stabSettings.ManualRate[STABILIZATIONSETTINGS_MANUALRATE_ROLL];
            stabDesired.Pitch = manualControl.Pitch * stabSettings.ManualRate[STABILIZATIONSETTINGS_MANUALRATE_PITCH];
        } else {
            stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL] = STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE;
            stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH] = STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE;

            stabDesired.Roll = manualControl.Roll * stabSettings.RollMax;
            stabDesired.Pitch = manualControl.Pitch * stabSettings.PitchMax;
        }

        stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_YAW] = STABILIZATIONDESIRED_STABILIZATIONMODE_RATE;
        stabDesired.Yaw = manualControl.Yaw * stabSettings.ManualRate[STABILIZATIONSETTINGS_MANUALRATE_YAW];
        stabDesired.Throttle = manualControl.Throttle;

        switch (state) {
        case AT_INIT:

            lastUpdateTime = xTaskGetTickCount();

            // Only start when armed and flying
            if (flightStatus.Armed == FLIGHTSTATUS_ARMED_ARMED && stabDesired.Throttle > 0) {
                state = AT_START;
            }
            break;

        case AT_START:

            diffTime = xTaskGetTickCount() - lastUpdateTime;

            // Spend the first block of time in normal rate mode to get airborne
            if (diffTime > PREPARE_TIME) {
                state = AT_ROLL;
                lastUpdateTime = xTaskGetTickCount();
            }
            break;

        case AT_ROLL:

            diffTime = xTaskGetTickCount() - lastUpdateTime;

            // Run relay mode on the roll axis for the measurement time
            stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL] = rate ? STABILIZATIONDESIRED_STABILIZATIONMODE_RELAYRATE :
                                                                                         STABILIZATIONDESIRED_STABILIZATIONMODE_RELAYATTITUDE;
            if (diffTime > MEAURE_TIME) { // Move on to next state
                state = AT_PITCH;
                lastUpdateTime = xTaskGetTickCount();
            }
            break;

        case AT_PITCH:

            diffTime = xTaskGetTickCount() - lastUpdateTime;

            // Run relay mode on the pitch axis for the measurement time
            stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH] = rate ? STABILIZATIONDESIRED_STABILIZATIONMODE_RELAYRATE :
                                                                                          STABILIZATIONDESIRED_STABILIZATIONMODE_RELAYATTITUDE;
            if (diffTime > MEAURE_TIME) { // Move on to next state
                state = AT_FINISHED;
                lastUpdateTime = xTaskGetTickCount();
            }
            break;

        case AT_FINISHED:

            // Wait until disarmed and landed before updating the settings
            if (flightStatus.Armed == FLIGHTSTATUS_ARMED_DISARMED && stabDesired.Throttle <= 0) {
                state = AT_SET;
            }

            break;

        case AT_SET:
            update_stabilization_settings();
            state = AT_INIT;
            break;

        default:
            // Set an alarm or some shit like that
            break;
        }

        StabilizationDesiredSet(&stabDesired);

        vTaskDelay(10);
    }
}

/**
 * Called after measuring roll and pitch to update the
 * stabilization settings
 *
 * takes in @ref RelayTuning and outputs @ref StabilizationSettings
 */
static void update_stabilization_settings()
{
    RelayTuningData relayTuning;

    RelayTuningGet(&relayTuning);

    RelayTuningSettingsData relaySettings;
    RelayTuningSettingsGet(&relaySettings);

    StabilizationSettingsData stabSettings;
    StabilizationSettingsGet(&stabSettings);

    // Eventually get these settings from RelayTuningSettings
    const float gain_ratio_r = 1.0f / 3.0f;
    const float zero_ratio_r = 1.0f / 3.0f;
    const float gain_ratio_p = 1.0f / 5.0f;
    const float zero_ratio_p = 1.0f / 5.0f;

    // For now just run over roll and pitch
    for (uint i = 0; i < 2; i++) {
        float wu = 1000.0f * 2 * M_PI / relayTuning.Period[i]; // ultimate freq = output osc freq (rad/s)

        float wc = wu * gain_ratio_r; // target openloop crossover frequency (rad/s)
        float zc = wc * zero_ratio_r; // controller zero location (rad/s)
        float kpu = 4.0f / M_PI / relayTuning.Gain[i]; // ultimate gain, i.e. the proportional gain for instablity
        float kp = kpu * gain_ratio_r; // proportional gain
        float ki = zc * kp; // integral gain

        // Now calculate gains for the next loop out knowing it is the integral of
        // the inner loop -- the plant is position/velocity = scale*1/s
        float wc2 = wc * gain_ratio_p; // crossover of the attitude loop
        float kp2 = wc2; // kp of attitude
        float ki2 = wc2 * zero_ratio_p * kp2; // ki of attitude

        switch (i) {
        case 0: // roll
            stabSettings.RollRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_KP] = kp;
            stabSettings.RollRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_KI] = ki;
            stabSettings.RollPI[STABILIZATIONSETTINGS_ROLLPI_KP] = kp2;
            stabSettings.RollPI[STABILIZATIONSETTINGS_ROLLPI_KI] = ki2;
            break;
        case 1: // Pitch
            stabSettings.PitchRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_KP] = kp;
            stabSettings.PitchRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_KI] = ki;
            stabSettings.PitchPI[STABILIZATIONSETTINGS_ROLLPI_KP] = kp2;
            stabSettings.PitchPI[STABILIZATIONSETTINGS_ROLLPI_KI] = ki2;
            break;
        default:
            // Oh shit oh shit oh shit
            break;
        }
    }
    switch (relaySettings.Behavior) {
    case RELAYTUNINGSETTINGS_BEHAVIOR_MEASURE:
        // Just measure, don't update the stab settings
        break;
    case RELAYTUNINGSETTINGS_BEHAVIOR_COMPUTE:
        StabilizationSettingsSet(&stabSettings);
        break;
    case RELAYTUNINGSETTINGS_BEHAVIOR_SAVE:
        StabilizationSettingsSet(&stabSettings);
        UAVObjSave(StabilizationSettingsHandle(), 0);
        break;
    }
}

/**
 * @}
 * @}
 */
