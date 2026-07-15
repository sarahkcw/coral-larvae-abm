#pragma once
#include "Actions/Actions.h"
#include "Sensors/LarvalSensorBaseComponent.h"
#include "Sensors/ChemicalSensors.h"
#include "Sensors/HydromechanicalSensors.h"
#include "Sensors/LightSensors.h"
#include "Sensors/Sensors.h"
#include "Sensors/SoundSensors.h"

constexpr float GSensor_Min = 0.0;
constexpr float GSensor_Max = 1.0;
constexpr float GSensor_Range = GSensor_Max - GSensor_Min;

constexpr float GNeuron_Min = -1.0;
constexpr float GNeuron_Max = 1.0;
constexpr float GNeuron_Range = GNeuron_Max - GNeuron_Min;

constexpr float GAction_Min = -1.0;
constexpr float GAction_Max = 1.0;
constexpr float GAction_Range = GAction_Max - GAction_Min;

enum ESensorType
{
	OSCILLATION,
	AGE,
	ENERGY,
	
	ALTEROMONAS_BIOFILM,
	CCA,
	CCA_FORWARD_BACK,
	CCA_UP_DOWN,
	CCA_LEFT_RIGHT,
	
	TEMPERATURE,
	PRESSURE,

	LIGHT_INTENSITY,
	LIGHT_WAVELENGTH,

	
	PARTICLE_MOTION_FORWARD_BACK,
	PARTICLE_MOTION_UP_DOWN,
	PARTICLE_MOTION_LEFT_RIGHT,
	PARTICLE_MOTION,

	NUM_SENSORS, // End of active sensors marker
};

enum EActionType
{
	FORWARD,
	ROTATE_YAW,
	ROTATE_PITCH,        
	SET_OSC,  
	SETTLE,  
	NUM_ACTIONS, // End of active actions marker 
};

inline TSubclassOf<ULarvalSensorBaseComponent> GetSensorClassFromEnum(const ESensorType SensorEnum) {
	switch (SensorEnum) {
	case ENERGY: return UEnergySensor::StaticClass();
	case CCA: return UCCASensor::StaticClass();
	case CCA_FORWARD_BACK: return UCcaFwdBackSensor::StaticClass();
	case CCA_LEFT_RIGHT: return UCcaLRSensor::StaticClass();
	case CCA_UP_DOWN: return UCcaUDSensor::StaticClass();
	case ALTEROMONAS_BIOFILM: return UAlteromonasBioFilm::StaticClass();
	case OSCILLATION: return UOscillatorSensor::StaticClass();
	case AGE: return UAgeSensor::StaticClass();
	case LIGHT_INTENSITY: return ULightIntensitySensor::StaticClass();
	case LIGHT_WAVELENGTH: return ULightWavelengthSensor::StaticClass();
	case PARTICLE_MOTION_FORWARD_BACK: return UParticleMotionFwdBackSensor::StaticClass();
	case PARTICLE_MOTION_UP_DOWN: return UParticleMotionUDSensor::StaticClass();
	case PARTICLE_MOTION_LEFT_RIGHT: return UParticleMotionLRSensor::StaticClass();
	case PARTICLE_MOTION: return UParticleMotionSensor::StaticClass();
	case PRESSURE: return UPressureSensor::StaticClass();
	case TEMPERATURE: return UTemperatureSensor::StaticClass();
	case NUM_SENSORS: break;		
	default: return nullptr;
	}
	return nullptr;
}

inline TSubclassOf<UActorComponent> GetActionClassFromEnum(const EActionType ActionEnum) {
	switch (ActionEnum) {
	case FORWARD: return UForwardForceAction::StaticClass();
	case ROTATE_YAW: return URotateYawAction::StaticClass();
	case ROTATE_PITCH: return URotatePitchAction::StaticClass();
	case SETTLE: return USettleReadinessAction::StaticClass();
	case SET_OSC: return UOscillatorAction::StaticClass();
	case NUM_ACTIONS: break;
	default: return nullptr;
	}
    return nullptr;
}
