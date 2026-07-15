#pragma once
#include <iostream>
#include <cassert>
#include <string>
#include "SensorActionMapping.h"

// This converts sensor numbers to descriptive strings.
inline std::string SensorName(const ESensorType Sensor)
{
    switch(Sensor) {
    case ENERGY: return "energy";
    case CCA: return "CCA";
    case CCA_UP_DOWN: return "CCA up/down";
    case CCA_FORWARD_BACK: return "CCA forward/back";
    case CCA_LEFT_RIGHT: return "CCA left/right";
    case PARTICLE_MOTION_UP_DOWN: return "particle motion up/down";
    case PARTICLE_MOTION_FORWARD_BACK: return "article motion forward/back";
    case PARTICLE_MOTION_LEFT_RIGHT: return "article motion left/right";
    case PARTICLE_MOTION: return "article motion";
    case ALTEROMONAS_BIOFILM: return "alteromonas biofilm";
    case OSCILLATION: return "oscillation";
    case AGE: return "age";
    case PRESSURE: return "pressure";
    case TEMPERATURE: return "temperature";
    case LIGHT_INTENSITY: return "light intensity";
    case LIGHT_WAVELENGTH: return "light wavelength";
    default: assert(false); break;
    }
    return {};
}

// Converts action numbers to descriptive strings.
inline std::string ActionName(EActionType action)
{
    switch(action) {
    case FORWARD: return "forward force";  
    case ROTATE_YAW: return "rotate yaw amount";
    case ROTATE_PITCH: return "rotate pitch amount";
    case SETTLE: return "willing to settle";
    case SET_OSC: return "set oscillation";
    default: assert(false); break;
    }
    return {};
}

// This converts sensor numbers to mnemonic strings.
// Useful for later processing by graph-nnet.py.
inline std::string SensorShortName(ESensorType sensor)
{
    switch(sensor) {
    case ENERGY: return "Engy";
    case CCA: return "Cca";
    case CCA_UP_DOWN: return "CcaUD";
    case CCA_FORWARD_BACK: return "CcaFwdB";
    case CCA_LEFT_RIGHT: return "CcaLR";
    case PARTICLE_MOTION_UP_DOWN: return "PartMotUD";
    case PARTICLE_MOTION_FORWARD_BACK: return "PartMotFwdB";
    case PARTICLE_MOTION_LEFT_RIGHT: return "PartMot";
    case PARTICLE_MOTION: return "PartMot";
    case ALTEROMONAS_BIOFILM: return "AltBio";
    case OSCILLATION: return "Osc";
    case PRESSURE: return "Pres";
    case TEMPERATURE: return "Temp";
    case LIGHT_INTENSITY: return "LInt";
    case LIGHT_WAVELENGTH: return "LWav";
    case AGE: return "Age";
    default: assert(false); break;
    }
    return {};
}

// Converts action numbers to mnemonic strings.
// Useful for later processing by graph-nnet.py.
inline std::string ActionShortName(EActionType action)
{
    switch(action) {
    case FORWARD: return "Fwd";
    case ROTATE_YAW: return "RYaw";
    case ROTATE_PITCH: return "RPitch";
    case SETTLE: return "Settle";
    case SET_OSC: return "SetOsc";
    default: assert(false); break;
    }
    return {};
}

// List the names of the active sensors and actions to stdout.
// "Active" means those sensors and actions that are compiled into
// the code. See sensors-actions.h for how to define the enums.
inline void PrintSensorsActions()
{
    unsigned i;
    std::cout << "Sensors:" << std::endl;
    for (i = 0; i < NUM_SENSORS; ++i) {
        std::cout << "  " << SensorName(static_cast<ESensorType>(i)) << std::endl;
    }
    std::cout << "Actions:" << std::endl;
    for (i = 0; i < NUM_ACTIONS; ++i) {
        std::cout << "  " << ActionName(static_cast<EActionType>(i)) << std::endl;
    }
    std::cout << std::endl;
}
