// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Header file for sample for manipulating DigitalTwin Interface for temperature sensor.

#ifndef DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR
#define DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR

#include "digitaltwin_interface_client.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Creates a new DIGITALTWIN_INTERFACE_CLIENT_HANDLE for this interface.
DIGITALTWIN_INTERFACE_CLIENT_HANDLE DigitalTwinSampleTemperatureSensor_CreateInterface(void);
// Sends DigitalTwin telemetry messages about current device memory usage
DIGITALTWIN_CLIENT_RESULT DigitalTwinSampleTemperatureSensor_SendTelemetryMessagesAsync(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle);

// Closes down resources associated with diagnostic interface.
void DigitalTwinSampleTemperatureSensor_Close(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle);

#ifdef __cplusplus
}
#endif


#endif // DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR
