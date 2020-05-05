// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Implements a sample DigitalTwin interface that reports the SDK information
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "digitaltwin_client_version.h"
#include "digitaltwin_sample_diagnostics.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"

// DigitalTwin interface id Diagnostics interface ID
static const char DigitalTwinSampleDiagnostic_InterfaceId[] = "dtmi:com:examples:Diagnostics;1";

// DigitalTwin component name from service perspective.
static const char DigitalTwinSampleDiagnostics_ComponentName[] = "diag";

//
// Command status codes
//
static const int commandStatusProcessing = 102;
static const int commandStatusSuccess = 200;
static const int commandStatusPending = 202;
static const int commandStatusFailure = 500;
static const int commandStatusNotPresent = 501;

//
// Application state associated with the particular interface.  It contains 
// the DIGITALTWIN_INTERFACE_CLIENT_HANDLE used for responses reporting properties.
//
typedef struct DIGITALTWIN_SAMPLE_DIAGNOSTICS_TAG
{
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceClientHandle;
    int rebootDelay;
    int numTimesRebootCommandCalled;
} DIGITALTWIN_SAMPLE_DIAGNOSTICS_STATE;


// State for interface.  For simplicity we set this as a global and set during DigitalTwin_InterfaceClient_Create, but  
// callbacks of this interface don't reference it directly but instead use userContextCallback passed to them.
static DIGITALTWIN_SAMPLE_DIAGNOSTICS_STATE digitaltwinSample_DiagnosticsState;

//  
//  Telemetry names for this interface.
//
static const char* DigitalTwinSampleDiagnostics_WorkingsetTelemetry = "workingset";

//
//  Callback command names for this interface.
//
static const char DigitalTwinSampleDiagnostics_DiagnosticsCommandReboot[] = "reboot";

static const unsigned char digitaltwinSample_Diagnostics_NotImplemented[] = "\"Requested command not implemented on this interface\"";

// DigitalTwinSampleDiagnostics_TelemetryCallback is invoked when a DigitalTwin telemetry message
// is either successfully delivered to the service or else fails.  For this sample, the userContextCallback
// is simply a string pointing to the name of the message sent.  More complex scenarios may include
// more detailed state information as part of this callback.
static void DigitalTwinSampleDiagnostics_TelemetryCallback(DIGITALTWIN_CLIENT_RESULT dtTelemetryStatus, void* userContextCallback)
{
    (void)userContextCallback;

    if (dtTelemetryStatus == DIGITALTWIN_CLIENT_OK)
    {
        // This tends to overwhelm the logging on output based on how frequently this function is invoked, so removing by default.
        // LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin successfully delivered telemetry message for <%s>", (const char*)userContextCallback);
    }
    else
    {
        LogError("DIAGNOSTICS_INTERFACE: DigitalTwin failed delivered telemetry message, error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtTelemetryStatus));
    }
}

//
// DigitalTwinSampleDiagnostics_SendTelemetryMessagesAsync is periodically invoked by the caller to
// send telemetry containing the current workingset.
//
DIGITALTWIN_CLIENT_RESULT DigitalTwinSampleDiagnostics_SendTelemetryMessagesAsync(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    DIGITALTWIN_CLIENT_RESULT result;

    float currentWorkingset = 2000.0f + ((float)rand() / RAND_MAX) * 15.0f;

    char currentMessage[128];

    sprintf(currentMessage, "{\"%s\":%.3f}", DigitalTwinSampleDiagnostics_WorkingsetTelemetry, currentWorkingset);

    if ((result = DigitalTwin_InterfaceClient_SendTelemetryAsync(interfaceHandle, (unsigned char*)currentMessage, strlen(currentMessage),
        DigitalTwinSampleDiagnostics_TelemetryCallback, NULL)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("DIAGNOSTICS_INTERFACE: DigitalTwin_InterfaceClient_SendTelemetryAsync failed for sending.");
    }

    return result;
}

static int DigitalTwinSampleDiagnostics_SetCommandResponseEmptyBody(DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse, int status)
{
    memset(dtCommandResponse, 0, sizeof(*dtCommandResponse));
    dtCommandResponse->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
    dtCommandResponse->status = status;
    return 0;
}

// DigitalTwinSampleDiagnostics_SetCommandResponse is a helper that fills out a DIGITALTWIN_CLIENT_COMMAND_RESPONSE
static int DigitalTwinSampleDiagnostics_SetCommandResponse(DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse, const unsigned char* responseData, int status)
{
    size_t responseLen = strlen((const char*)responseData);
    memset(dtCommandResponse, 0, sizeof(*dtCommandResponse));
    dtCommandResponse->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
    int result;

    // Allocate a copy of the response data to return to the invoker.  The DigitalTwin layer that invoked the application callback
    // takes responsibility for freeing this data.
    if (mallocAndStrcpy_s((char**)&dtCommandResponse->responseData, (const char*)responseData) != 0)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Unable to allocate response data");
        dtCommandResponse->status = commandStatusFailure;
        result = MU_FAILURE;
    }
    else
    {
        dtCommandResponse->responseDataLen = responseLen;
        dtCommandResponse->status = status;
        result = 0;
    }

    return result;
}

// Implement the callback to process the command "reboot".  Information pertaining to the request is specified in DIGITALTWIN_CLIENT_COMMAND_REQUEST,
// and the callback fills out data it wishes to return to the caller on the service in DIGITALTWIN_CLIENT_COMMAND_RESPONSE.
static void DigitalTwinSampleDiagnostics_RebootCallback(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtCommandRequest, DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse, void* commandCallbackContext)
{
    DIGITALTWIN_SAMPLE_DIAGNOSTICS_STATE* state = (DIGITALTWIN_SAMPLE_DIAGNOSTICS_STATE*)commandCallbackContext;

    LogInfo("DIAGNOSTICS_INTERFACE: Reboot command invoked.  It has been invoked %d times previously", state->numTimesRebootCommandCalled);
    LogInfo("DIAGNOSTICS_INTERFACE: Reboot data=<%.*s>", (int)dtCommandRequest->requestDataLen, dtCommandRequest->requestData);

    state->numTimesRebootCommandCalled++;

    (void)DigitalTwinSampleDiagnostics_SetCommandResponseEmptyBody(dtCommandResponse, commandStatusSuccess);
}

// DigitalTwinSample_ProcessCommandUpdate receives commands from the server.  This implementation acts as a simple dispatcher
// to the functions to perform the actual processing.
void DigitalTwinSample_ProcessCommandUpdate(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtCommandRequest, DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse, void* commandCallbackContext)
{
    if (strcmp(dtCommandRequest->commandName, DigitalTwinSampleDiagnostics_DiagnosticsCommandReboot) == 0)
    {
        DigitalTwinSampleDiagnostics_RebootCallback(dtCommandRequest, dtCommandResponse, commandCallbackContext);
    }
    else
    {
        // If the command is not implemented by this interface, by convention we return a 501 error to server.
        LogError("DIAGNOSTICS_INTERFACE: Command name <%s> is not associated with this interface", dtCommandRequest->commandName);
        (void)DigitalTwinSampleDiagnostics_SetCommandResponse(dtCommandResponse, digitaltwinSample_Diagnostics_NotImplemented, commandStatusNotPresent);
    }
}

// DigitalTwinSampleDiagnostics_InterfaceRegisteredCallback is invoked when this interface
// is successfully or unsuccessfully registered with the service, and also when the interface is deleted.
static void DigitalTwinSampleDiagnostics_InterfaceRegisteredCallback(DIGITALTWIN_CLIENT_RESULT dtInterfaceStatus, void* userInterfaceContext)
{
    DIGITALTWIN_SAMPLE_DIAGNOSTICS_STATE *diagnosticsState = (DIGITALTWIN_SAMPLE_DIAGNOSTICS_STATE*)userInterfaceContext;
    if (dtInterfaceStatus == DIGITALTWIN_CLIENT_OK)
    {
        // Once the interface is registered, send our reported properties to the service.  
        // It *IS* safe to invoke most DigitalTwin API calls from a callback thread like this, though it 
        // is NOT safe to create/destroy/register interfaces now.
        LogInfo("DIAGNOSTICS: Interface successfully registered, rebootDelay:<%i>", diagnosticsState->rebootDelay);
    }
    else if (dtInterfaceStatus == DIGITALTWIN_CLIENT_ERROR_INTERFACE_UNREGISTERING)
    {
        // Once an interface is marked as unregistered, it cannot be used for any DigitalTwin SDK calls.
        LogInfo("DIAGNOSTICS: Interface received unregistering callback.");
    }
    else 
    {
        LogError("DIAGNOSTICS: Interface received failed, status=<%s>.", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtInterfaceStatus));
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
DIGITALTWIN_INTERFACE_CLIENT_HANDLE DigitalTwinSampleDiagnostics_CreateInterface(void)
{
    DIGITALTWIN_CLIENT_RESULT result;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle;

    memset(&digitaltwinSample_DiagnosticsState, 0, sizeof(digitaltwinSample_DiagnosticsState));
    
    if ((result =  DigitalTwin_InterfaceClient_Create(DigitalTwinSampleDiagnostic_InterfaceId, DigitalTwinSampleDiagnostics_ComponentName, DigitalTwinSampleDiagnostics_InterfaceRegisteredCallback, (void*)&digitaltwinSample_DiagnosticsState, &interfaceHandle)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("DIAGNOSTICS: Unable to allocate interface client handle for interfaceId=<%s>, componentName=<%s>, error=<%s>", DigitalTwinSampleDiagnostic_InterfaceId, DigitalTwinSampleDiagnostics_ComponentName, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        interfaceHandle = NULL;
    }
    else if ((result = DigitalTwin_InterfaceClient_SetCommandsCallback(interfaceHandle, DigitalTwinSample_ProcessCommandUpdate, (void*)&digitaltwinSample_DiagnosticsState)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_SetCommandsCallback failed. error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        DigitalTwinSampleDiagnostics_Close(interfaceHandle);
        interfaceHandle = NULL;
    }
    else
    {
        LogInfo("DIAGNOSTICS: Created DIGITALTWIN_INTERFACE_CLIENT_HANDLE.  interfaceId=<%s>, componentName=<%s>, handle=<%p>", DigitalTwinSampleDiagnostic_InterfaceId, DigitalTwinSampleDiagnostics_ComponentName, interfaceHandle);
        digitaltwinSample_DiagnosticsState.interfaceClientHandle = interfaceHandle;
    }

    return interfaceHandle;
}


//
// DigitalTwinSampleEnvironmentalSensor_Close is invoked when the sample device is shutting down.
//
void DigitalTwinSampleDiagnostics_Close(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    // On shutdown, in general the first call made should be to DigitalTwin_InterfaceClient_Destroy.
    // This will block if there are any active callbacks in this interface, and then
    // mark the underlying handle such that no future callbacks shall come to it.
    DigitalTwin_InterfaceClient_Destroy(interfaceHandle);
}

