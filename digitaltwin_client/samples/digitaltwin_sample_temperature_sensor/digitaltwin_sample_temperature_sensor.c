// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Implements a sample DigitalTwin interface that reports the SDK information
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "digitaltwin_client_version.h"
#include "digitaltwin_sample_temperature_sensor.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"

// DigitalTwin interface id TemperatureSensor interface ID
static const char DigitalTwinSampleTemperatureSensor_InterfaceId[] = "dtmi:com:examples:TemperatureSensor;1";

// DigitalTwin component name from service perspective.
static const char DigitalTwinSampleTemperatureSensor_ComponentName[] = "tempSensor1";

//
// Application state associated with the particular interface.  It contains 
// the DIGITALTWIN_INTERFACE_CLIENT_HANDLE used for responses reporting properties.
//
typedef struct DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR_TAG
{
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceClientHandle;
    double targetTemperature;
} DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR_STATE;


// State for interface.  For simplicity we set this as a global and set during DigitalTwin_InterfaceClient_Create, but  
// callbacks of this interface don't reference it directly but instead use userContextCallback passed to them.
static DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR_STATE digitaltwinSample_TemperatureSensorState;

//  
//  Telemetry names for this interface.
//
static const char* DigitalTwinSampleTemperatureSensor_TemperatureTelemetry = "temperature";

//
//  Property names and data for DigitalTwin read-only properties for this interface.
//
// digitaltwinSample_DeviceState* represents the environmental sensor's read-only property, whether its online or not.
static const char digitaltwinSample_TargetTemperatureProperty[] = "targetTemperature";
static double digitaltwinSample_TargetTemperatureData = 33.3;
static const int digitaltwinSample_TargetTemperatureDataLen = sizeof(digitaltwinSample_TargetTemperatureData) - 1;

// DigitalTwinSampleTemperatureSensor_TelemetryCallback is invoked when a DigitalTwin telemetry message
// is either successfully delivered to the service or else fails.  For this sample, the userContextCallback
// is simply a string pointing to the name of the message sent.  More complex scenarios may include
// more detailed state information as part of this callback.
static void DigitalTwinSampleTemperatureSensor_TelemetryCallback(DIGITALTWIN_CLIENT_RESULT dtTelemetryStatus, void* userContextCallback)
{
    (void)userContextCallback;

    if (dtTelemetryStatus == DIGITALTWIN_CLIENT_OK)
    {
        // This tends to overwhelm the logging on output based on how frequently this function is invoked, so removing by default.
        // LogInfo("TEMPERATURE_SENSOR_INTERFACE: DigitalTwin successfully delivered telemetry message for <%s>", (const char*)userContextCallback);
    }
    else
    {
        LogError("TEMPERATURE_SENSOR_INTERFACE: DigitalTwin failed delivered telemetry message, error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtTelemetryStatus));
    }
}

//
// DigitalTwinSampleTemperatureSensor_SendTelemetryMessagesAsync is periodically invoked by the caller to
// send telemetry containing the current workingset.
//
DIGITALTWIN_CLIENT_RESULT DigitalTwinSampleTemperatureSensor_SendTelemetryMessagesAsync(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    DIGITALTWIN_CLIENT_RESULT result;

    float currentTemp = 40.0f + ((float)rand() / RAND_MAX) * 15.0f;

    char currentMessage[128];

    sprintf(currentMessage, "{\"%s\":%.3f}", DigitalTwinSampleTemperatureSensor_TemperatureTelemetry, currentTemp);

    if ((result = DigitalTwin_InterfaceClient_SendTelemetryAsync(interfaceHandle, (unsigned char*)currentMessage, strlen(currentMessage),
        DigitalTwinSampleTemperatureSensor_TelemetryCallback, NULL)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("TEMPERATURE_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_SendTelemetryAsync failed for sending.");
    }

    return result;
}

// DigitalTwinSampleTemperatureSensor_PropertyCallback is invoked when a property is updated (or failed) going to server.
// In this sample, we route ALL property callbacks to this function and just have the userContextCallback set
// to the propertyName.  Product code will potentially have context stored in this userContextCallback.
static void DigitalTwinSampleTemperatureSensor_PropertyCallback(DIGITALTWIN_CLIENT_RESULT dtReportedStatus, void* userContextCallback)
{
    if (dtReportedStatus == DIGITALTWIN_CLIENT_OK)
    {
        LogInfo("TEMPERATURE_SENSOR_INTERFACE: Updating property=<%s> succeeded", (const char*)userContextCallback);
    }
    else
    {
        LogError("TEMPERATURE_SENSOR_INTERFACE: Updating property property=<%s> failed, error=<%s>", (const char*)userContextCallback, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtReportedStatus));
    }
}

// Sends a reported property for target temperature of this simulated device.
static DIGITALTWIN_CLIENT_RESULT DigitalTwinSampleTemperatureSensor_ReportTargetTemperatureAsync(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    DIGITALTWIN_CLIENT_RESULT result;

    char currentMessage[32];

    sprintf(currentMessage, "%.3f", digitaltwinSample_TargetTemperatureData);

    result = DigitalTwin_InterfaceClient_ReportPropertyAsync(interfaceHandle, digitaltwinSample_TargetTemperatureProperty,
        (unsigned char*)currentMessage, digitaltwinSample_TargetTemperatureDataLen, NULL,
        DigitalTwinSampleTemperatureSensor_PropertyCallback, (void*)digitaltwinSample_TargetTemperatureProperty);

    if (result != DIGITALTWIN_CLIENT_OK)
    {
        LogError("TEMPERATURE_SENSOR_INTERFACE: Reporting property=<%s> failed, error=<%s>", digitaltwinSample_TargetTemperatureProperty, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
    }
    else
    {
        LogInfo("TEMPERATURE_SENSOR_INTERFACE: Queued async report read only property for %s", digitaltwinSample_TargetTemperatureProperty);
    }

    return result;
}

// Processes a property update, which the server initiated, for customer name.
static void DigitalTwinSampleTemperatureSensor_TargetTemperatureCallback(const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate, void* propertyCallbackContext)
{
    DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR_STATE* tempSensorState = (DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR_STATE*)propertyCallbackContext;
    DIGITALTWIN_CLIENT_RESULT result;

    LogInfo("TEMPERATURE_SENSOR_INTERFACE: TargetTemperature property invoked...");
    LogInfo("TEMPERATURE_SENSOR_INTERFACE: TargetTemperature data=<%.*s>", (int)dtClientPropertyUpdate->propertyDesiredLen, dtClientPropertyUpdate->propertyDesired);

    DIGITALTWIN_CLIENT_PROPERTY_RESPONSE propertyResponse;

    // Version of this structure for C SDK.
    propertyResponse.version = DIGITALTWIN_CLIENT_PROPERTY_RESPONSE_VERSION_1;
    propertyResponse.responseVersion = dtClientPropertyUpdate->desiredVersion;

    if (dtClientPropertyUpdate->propertyDesired != NULL && strlen((char*)dtClientPropertyUpdate->propertyDesired) > 0)
    {
        tempSensorState->targetTemperature = atof((char*)dtClientPropertyUpdate->propertyDesired);
    }
    LogInfo("TEMPERATURE_SENSOR_INTERFACE: TargetTemperature sucessfully updated...");

    // Indicates success
    propertyResponse.statusCode = 200;
    // Optional additional human readable information about status.
    propertyResponse.statusDescription = "Property Updated Successfully";

    //
    // DigitalTwin_InterfaceClient_ReportPropertyAsync takes the DIGITALTWIN_CLIENT_PROPERTY_RESPONSE and returns information back to service.
    //
    result = DigitalTwin_InterfaceClient_ReportPropertyAsync(tempSensorState->interfaceClientHandle, DigitalTwinSampleTemperatureSensor_TemperatureTelemetry,
        dtClientPropertyUpdate->propertyDesired, dtClientPropertyUpdate->propertyDesiredLen, &propertyResponse,
        DigitalTwinSampleTemperatureSensor_PropertyCallback, (void*)DigitalTwinSampleTemperatureSensor_TemperatureTelemetry);
    if (result != DIGITALTWIN_CLIENT_OK)
    {
        LogError("TEMPERATURE_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_ReportPropertyAsync for CustomerName failed, error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
    }
    else
    {
        LogInfo("TEMPERATURE_SENSOR_INTERFACE: Successfully queued Property update for CustomerName");
    }
}

// DigitalTwinSampleTemperatureSensor_ProcessPropertyUpdate receives updated properties from the server.  This implementation
// acts as a simple dispatcher to the functions to perform the actual processing.
static void DigitalTwinSampleTemperatureSensor_ProcessPropertyUpdate(const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate, void* propertyCallbackContext)
{
    if (strcmp(dtClientPropertyUpdate->propertyName, digitaltwinSample_TargetTemperatureProperty) == 0)
    {
        DigitalTwinSampleTemperatureSensor_TargetTemperatureCallback(dtClientPropertyUpdate, propertyCallbackContext);
    }
    else
    {
        // If the property is not implemented by this interface, presently we only record a log message but do not have a mechanism to report back to the service
        LogError("TEMPERATURE_SENSOR_INTERFACE: Property name <%s> is not associated with this interface", dtClientPropertyUpdate->propertyName);
    }
}

// DigitalTwinSampleTemperatureSensor_InterfaceRegisteredCallback is invoked when this interface
// is successfully or unsuccessfully registered with the service, and also when the interface is deleted.
static void DigitalTwinSampleTemperatureSensor_InterfaceRegisteredCallback(DIGITALTWIN_CLIENT_RESULT dtInterfaceStatus, void* userInterfaceContext)
{
    DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR_STATE* tempSensorState = (DIGITALTWIN_SAMPLE_TEMPERATURE_SENSOR_STATE*)userInterfaceContext;
    if (dtInterfaceStatus == DIGITALTWIN_CLIENT_OK)
    {
        // Once the interface is registered, send our reported properties to the service.  
        // It *IS* safe to invoke most DigitalTwin API calls from a callback thread like this, though it 
        // is NOT safe to create/destroy/register interfaces now.
        LogInfo("TEMPERATURE_SENSOR_INTERFACE: Interface successfully registered.");
        DigitalTwinSampleTemperatureSensor_ReportTargetTemperatureAsync(tempSensorState->interfaceClientHandle);
    }
    else if (dtInterfaceStatus == DIGITALTWIN_CLIENT_ERROR_INTERFACE_UNREGISTERING)
    {
        // Once an interface is marked as unregistered, it cannot be used for any DigitalTwin SDK calls.
        LogInfo("TEMPERATURE_SENSOR_INTERFACE: Interface received unregistering callback.");
    }
    else 
    {
        LogError("TEMPERATURE_SENSOR_INTERFACE: Interface received failed, status=<%s>.", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtInterfaceStatus));
    }
}

//
// DigitalTwinSampleDiagnostics_CreateInterface is the initial entry point into the DigitalTwinDiagnostics Interface.
// It simply creates a DIGITALTWIN_INTERFACE_CLIENT_HANDLE that is mapped to the diagnostics component name.
// This call is synchronous, as simply creating an interface only performs initial allocations.
//
// NOTE: The actual registration of this interface is left to the caller, which may register 
// multiple interfaces on one DIGITALTWIN_DEVICE_CLIENT_HANDLE.
//
DIGITALTWIN_INTERFACE_CLIENT_HANDLE DigitalTwinSampleTemperatureSensor_CreateInterface(void)
{
    DIGITALTWIN_CLIENT_RESULT result;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle;

    memset(&digitaltwinSample_TemperatureSensorState, 0, sizeof(digitaltwinSample_TemperatureSensorState));
    
    if ((result =  DigitalTwin_InterfaceClient_Create(DigitalTwinSampleTemperatureSensor_InterfaceId, DigitalTwinSampleTemperatureSensor_ComponentName, DigitalTwinSampleTemperatureSensor_InterfaceRegisteredCallback, (void*)&digitaltwinSample_TemperatureSensorState, &interfaceHandle)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("TEMPERATURE_SENSOR_INTERFACE: Unable to allocate interface client handle for interfaceId=<%s>, componentName=<%s>, error=<%s>", DigitalTwinSampleTemperatureSensor_InterfaceId, DigitalTwinSampleTemperatureSensor_ComponentName, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        interfaceHandle = NULL;
    }
    else if ((result = DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallback(interfaceHandle, DigitalTwinSampleTemperatureSensor_ProcessPropertyUpdate, (void*)&digitaltwinSample_TemperatureSensorState)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("TEMPERATURE_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallback failed. error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        DigitalTwinSampleTemperatureSensor_Close(interfaceHandle);
        interfaceHandle = NULL;
    }
    else
    {
        LogInfo("TEMPERATURE_SENSOR_INTERFACE: Created DIGITALTWIN_INTERFACE_CLIENT_HANDLE.  interfaceId=<%s>, componentName=<%s>, handle=<%p>", DigitalTwinSampleTemperatureSensor_InterfaceId, DigitalTwinSampleTemperatureSensor_ComponentName, interfaceHandle);
        digitaltwinSample_TemperatureSensorState.interfaceClientHandle = interfaceHandle;
    }

    return interfaceHandle;
}


//
// DigitalTwinSampleEnvironmentalSensor_Close is invoked when the sample device is shutting down.
//
void DigitalTwinSampleTemperatureSensor_Close(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    // On shutdown, in general the first call made should be to DigitalTwin_InterfaceClient_Destroy.
    // This will block if there are any active callbacks in this interface, and then
    // mark the underlying handle such that no future callbacks shall come to it.
    DigitalTwin_InterfaceClient_Destroy(interfaceHandle);
}

