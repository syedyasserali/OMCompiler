/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-CurrentYear, Open Source Modelica Consortium (OSMC),
 * c/o Linköpings universitet, Department of Computer and Information Science,
 * SE-58183 Linköping, Sweden.
 *
 * All rights reserved.
 *
 * THIS PROGRAM IS PROVIDED UNDER THE TERMS OF GPL VERSION 3 LICENSE OR
 * THIS OSMC PUBLIC LICENSE (OSMC-PL) VERSION 1.2.
 * ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS PROGRAM CONSTITUTES RECIPIENT'S ACCEPTANCE
 * OF THE OSMC PUBLIC LICENSE OR THE GPL VERSION 3, ACCORDING TO RECIPIENTS CHOICE.
 *
 * The OpenModelica software and the Open Source Modelica
 * Consortium (OSMC) Public License (OSMC-PL) are obtained
 * from OSMC, either from the above address,
 * from the URLs: http://www.ida.liu.se/projects/OpenModelica or
 * http://www.openmodelica.org, and in the OpenModelica distribution.
 * GNU version 3 is obtained from: http://www.gnu.org/copyleft/gpl.html.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without
 * even the implied warranty of  MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE, EXCEPT AS EXPRESSLY SET FORTH
 * IN THE BY RECIPIENT SELECTED SUBSIDIARY LICENSE CONDITIONS OF OSMC-PL.
 *
 * See the full OSMC Public License conditions for more details.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#include "fmilib.h"

#define BUFFER 1000
#define FMI_DEBUG

static void importlogger(jm_callbacks* c, jm_string module, jm_log_level_enu_t log_level, jm_string message)
{
#ifdef FMI_DEBUG
  printf("module = %s, log level = %d: %s\n", module, log_level, message);
#endif
}

/* Logger function used by the FMU internally */
static void fmilogger(fmi1_component_t c, fmi1_string_t instanceName, fmi1_status_t status, fmi1_string_t category, fmi1_string_t message, ...)
{
#ifdef FMI_DEBUG
  char msg[BUFFER];
  va_list argp;
  va_start(argp, message);
  vsprintf(msg, message, argp);
  printf("fmiStatus = %d;  %s (%s): %s\n", status, instanceName, category, msg);
#endif
}

/*
 * Creates an instance of the FMI Import Context i.e fmi_import_context_t
 */
void* fmiImportContext_OMC(int fmi_log_level)
{
  // JM callbacks
  static int init = 0;
  static jm_callbacks callbacks;
  if (!init) {
    init = 1;
    callbacks.malloc = malloc;
    callbacks.calloc = calloc;
    callbacks.realloc = realloc;
    callbacks.free = free;
    callbacks.logger = importlogger;
    callbacks.log_level = fmi_log_level;
    callbacks.context = 0;
  }
  fmi_import_context_t* context = fmi_import_allocate_context(&callbacks);
  return context;
}

/*
 * Destroys the instance of the FMI Import Context i.e fmi_import_context_t
 */
void fmiImportFreeContext_OMC(void* context)
{
  fmi_import_free_context(context);
}

/*
 * Creates an instance of the FMI Import i.e fmi1_import_t
 * Reads the xml.
 * Loads the binary (dll/so).
 */
void* fmiImportInstance_OMC(void* context, char* working_directory)
{
  // FMI callback functions
  static int init = 0;
  fmi1_callback_functions_t callback_functions;
  if (!init) {
    init = 1;
    callback_functions.logger = fmilogger;
    callback_functions.allocateMemory = calloc;
    callback_functions.freeMemory = free;
  }
  // parse the xml file
  fmi1_import_t* fmi;
  fmi = fmi1_import_parse_xml((fmi_import_context_t*)context, working_directory);
  if(!fmi) {
    fprintf(stderr, "Error parsing the XML file contained in %s\n", working_directory);
    return 0;
  }
  // Load the binary (dll/so)
  jm_status_enu_t status;
  status = fmi1_import_create_dllfmu(fmi, callback_functions, 0);
  if (status == jm_status_error) {
    fprintf(stderr, "Could not create the DLL loading mechanism(C-API).\n");
    return 0;
  }
  return fmi;
}

/*
 * Destroys the instance of the FMI Import i.e fmi1_import_t
 * Also destroys the loaded binary (dll/so).
 */
void fmiImportFreeInstance_OMC(void* fmi)
{
  fmi1_import_t* fmi1 = (fmi1_import_t*)fmi;
  fmi1_import_destroy_dllfmu(fmi1);
  fmi1_import_free(fmi1);
}

/*
 * Destroys the instance of the FMI Event Info i.e fmi1_event_info_t
 */
void fmiFreeEventInfo_OMC(void* eventInfo)
{
  if ((fmi1_event_info_t*)eventInfo != NULL)
    free((fmi1_event_info_t*)eventInfo);
}

/*
 * Wrapper for the FMI function fmiInstantiateModel.
 */
void fmiInstantiateModel_OMC(void* fmi, char* instanceName)
{
  fmi1_import_instantiate_model((fmi1_import_t*)fmi, instanceName);
}

/*
 * Wrapper for the FMI function fmiSetTime.
 * Returns status.
 */
double fmiSetTime_OMC(void* fmi, double time, double flowTime)
{
  fmi1_import_set_time((fmi1_import_t*)fmi, time);
  return flowTime;
}

/*
 * Wrapper for the FMI function fmiSetDebugLogging.
 * Returns status.
 */
int fmiSetDebugLogging_OMC(void* fmi, int debugLogging)
{
  return fmi1_import_set_debug_logging((fmi1_import_t*)fmi, debugLogging);
}

/*
 * Wrapper for the FMI function fmiInitialize.
 * Returns FMI Event Info i.e fmi1_event_info_t.
 */
void* fmiInitialize_OMC(void* fmi, char* instanceName, int debugLogging, double time, void* in_eventInfo, double flowInit, double* fmi_TNext, double* fmi_Initialized)
{
  static int init = 0;
  if (!init) {
    init = 1;
    fmiInstantiateModel_OMC(fmi, instanceName);
    fmiSetDebugLogging_OMC(fmi, debugLogging);
    fmiSetTime_OMC(fmi, time, 1);

    fmi1_boolean_t toleranceControlled = fmi1_true;
    fmi1_real_t relativeTolerance = 0.001;
    fmi1_event_info_t* eventInfo = malloc(sizeof(fmi1_event_info_t));
    fmi1_status_t fmistatus = fmi1_import_initialize((fmi1_import_t*)fmi, toleranceControlled, relativeTolerance, eventInfo);
    *fmi_TNext = (double)fmistatus;
    return eventInfo;
  }
  return in_eventInfo;
}

/*
 * Wrapper for the FMI function fmiGetContinuousStates.
 * Returns states.
 */
void fmiGetContinuousStates_OMC(void* fmi, int numberOfContinuousStates, double flowInit, double* states)
{
  fmi1_import_get_continuous_states((fmi1_import_t*)fmi, (fmi1_real_t*)states, numberOfContinuousStates);
}

/*
 * Wrapper for the FMI function fmiSetContinuousStates.
 * Returns status.
 */
double fmiSetContinuousStates_OMC(void* fmi, int numberOfContinuousStates, double* states, double flowTime)
{
  fmi1_import_set_continuous_states((fmi1_import_t*)fmi, (fmi1_real_t*)states, numberOfContinuousStates);
  return flowTime;
}

/*
 * Wrapper for the FMI function fmiGetEventIndicators.
 * Returns events.
 */
void fmiGetEventIndicators_OMC(void* fmi, int numberOfEventIndicators, double flowEvent, double* events)
{
  fmi1_import_get_event_indicators((fmi1_import_t*)fmi, (fmi1_real_t*)events, numberOfEventIndicators);
}

/*
 * Wrapper for the FMI function fmiGetDerivatives.
 * Returns states.
 */
void fmiGetDerivatives_OMC(void* fmi, int numberOfContinuousStates, double flowStates, double* states)
{
  fmi1_import_get_derivatives((fmi1_import_t*)fmi, (fmi1_real_t*)states, numberOfContinuousStates);
}

fmi1_value_reference_t* real_to_fmi1_value_reference(int numberOfValueReferences, double* valuesReferences)
{
  fmi1_value_reference_t* valuesReferences_int = malloc(sizeof(fmi1_value_reference_t)*numberOfValueReferences);
  int i;
  for (i = 0 ; i < numberOfValueReferences ; i++)
    valuesReferences_int[i] = (int)valuesReferences[i];
  return valuesReferences_int;
}

/*
 * Wrapper for the FMI function fmiGetReal.
 * Returns realValues.
 */
void fmiGetReal_OMC(void* fmi, int numberOfValueReferences, double* realValuesReferences, double* realValues, double dummy)
{
  fmi1_value_reference_t* valuesReferences_int = real_to_fmi1_value_reference(numberOfValueReferences, realValuesReferences);
  fmi1_import_get_real((fmi1_import_t*)fmi, valuesReferences_int, numberOfValueReferences, (fmi1_real_t*)realValues);
  free(valuesReferences_int);
}

/*
 * Wrapper for the FMI function fmiSetReal.
 * Returns status.
 */
double fmiSetReal_OMC(void* fmi, int numberOfValueReferences, double* realValuesReferences, double* realValues)
{
  fmi1_value_reference_t* valuesReferences_int = real_to_fmi1_value_reference(numberOfValueReferences, realValuesReferences);
  fmi1_status_t fmistatus = fmi1_import_set_real((fmi1_import_t*)fmi, valuesReferences_int, numberOfValueReferences, (fmi1_real_t*)realValues);
  return (double)fmistatus;
}

/*
 * Wrapper for the FMI function fmiGetInteger.
 * Returns integerValues.
 */
void fmiGetInteger_OMC(void* fmi, int numberOfValueReferences, double* integerValuesReferences, int* integerValues, double dummy)
{
  fmi1_value_reference_t* valuesReferences_int = real_to_fmi1_value_reference(numberOfValueReferences, integerValuesReferences);
  fmi1_import_get_integer((fmi1_import_t*)fmi, valuesReferences_int, numberOfValueReferences, (fmi1_integer_t*)integerValues);
  free(valuesReferences_int);
}

/*
 * Wrapper for the FMI function fmiSetInteger.
 * Returns status.
 */
double fmiSetInteger_OMC(void* fmi, int numberOfValueReferences, double* integerValuesReferences, int* integerValues, double dummy)
{
  fmi1_value_reference_t* valuesReferences_int = real_to_fmi1_value_reference(numberOfValueReferences, integerValuesReferences);
  fmi1_status_t fmistatus = fmi1_import_set_integer((fmi1_import_t*)fmi, valuesReferences_int, numberOfValueReferences, (fmi1_integer_t*)integerValues);
  return (double)fmistatus;
}

/*
 * Wrapper for the FMI function fmiGetBoolean.
 * Returns booleanValues.
 */
void fmiGetBoolean_OMC(void* fmi, int numberOfValueReferences, double* booleanValuesReferences, int* booleanValues)
{
  fmi1_value_reference_t* valuesReferences_int = real_to_fmi1_value_reference(numberOfValueReferences, booleanValuesReferences);
  fmi1_import_get_boolean((fmi1_import_t*)fmi, valuesReferences_int, numberOfValueReferences, (fmi1_boolean_t*)booleanValues);
  free(valuesReferences_int);
}

/*
 * Wrapper for the FMI function fmiSetBoolean.
 * Returns status.
 */
double fmiSetBoolean_OMC(void* fmi, int numberOfValueReferences, double* booleanValuesReferences, int* booleanValues, double dummy)
{
  fmi1_value_reference_t* valuesReferences_int = real_to_fmi1_value_reference(numberOfValueReferences, booleanValuesReferences);
  fmi1_status_t fmistatus = fmi1_import_set_boolean((fmi1_import_t*)fmi, valuesReferences_int, numberOfValueReferences, (fmi1_boolean_t*)booleanValues);
  return (double)fmistatus;
}

/*
 * Wrapper for the FMI function fmiGetString.
 * Returns stringValues.
 */
void fmiGetString_OMC(void* fmi, int numberOfValueReferences, double* stringValuesReferences, char** stringValues)
{
  fmi1_value_reference_t* valuesReferences_int = real_to_fmi1_value_reference(numberOfValueReferences, stringValuesReferences);
  fmi1_import_get_string((fmi1_import_t*)fmi, valuesReferences_int, numberOfValueReferences, (fmi1_string_t*)stringValues);
  free(valuesReferences_int);
}

/*
 * Wrapper for the FMI function fmiSetString.
 * Returns status.
 */
double fmiSetString_OMC(void* fmi, int numberOfValueReferences, double* stringValuesReferences, char** stringValues, double dummy)
{
  fmi1_value_reference_t* valuesReferences_int = real_to_fmi1_value_reference(numberOfValueReferences, stringValuesReferences);
  fmi1_status_t fmistatus = fmi1_import_set_string((fmi1_import_t*)fmi, valuesReferences_int, numberOfValueReferences, (fmi1_string_t*)stringValues);
  return (double)fmistatus;
}

/*
 * Wrapper for the FMI function fmiEventUpdate.
 * Returns FMI Event Info i.e fmi1_event_info_t
 */
int fmiEventUpdate_OMC(void* fmi, int intermediateResults, void* eventInfo, double flowStates, double* flowTime)
{
  fmi1_event_info_t* e = (fmi1_event_info_t*)eventInfo;
  fmi1_import_eventUpdate((fmi1_import_t*)fmi, intermediateResults, e);
  return e->stateValuesChanged;
}

/*
 * Wrapper for the FMI function fmiCompletedIntegratorStep.
 */
int fmiCompletedIntegratorStep_OMC(void* fmi, double flowStates)
{
  fmi1_boolean_t callEventUpdate = fmi1_false;
  fmi1_import_completed_integrator_step((fmi1_import_t*)fmi, &callEventUpdate);
  return callEventUpdate;
}

/*
 * Wrapper for the FMI function fmiTerminate.
 */
int fmiTerminate_OMC(void* fmi)
{
  fmi1_status_t fmistatus = fmi1_import_terminate((fmi1_import_t*)fmi);
  return fmistatus;
}

/*
 * Wrapper for the FMI function fmiInstantiateSlave.
 */
void fmiInstantiateSlave_OMC(void* fmi, char* instanceName, char* fmuLocation, char* mimeType, double timeout, int visible, int interactive)
{
  fmi1_import_instantiate_slave((fmi1_import_t*)fmi, instanceName, fmuLocation, mimeType, timeout, visible, interactive);
}

/*
 * Wrapper for the FMI function fmiInitializeSlave.
 */
void fmiInitializeSlave_OMC(void* fmi, double tStart, int stopTimeDefined, double tStop)
{
  fmi1_import_initialize_slave((fmi1_import_t*)fmi, tStart, stopTimeDefined, tStop);
}

/*
 * Wrapper for the FMI function fmiDoStep.
 */
double fmiDoStep_OMC(void* fmi, double currentCommunicationPoint, double communicationStepSize, int newStep)
{
  fmi1_import_do_step((fmi1_import_t*)fmi, currentCommunicationPoint, communicationStepSize, newStep);
  return 0.0;
}

/*
 * Wrapper for the FMI function fmiTerminateSlave.
 */
int fmiTerminateSlave_OMC(void* fmi)
{
  fmi1_status_t fmistatus = fmi1_import_terminate_slave((fmi1_import_t*)fmi);
  return fmistatus;
}

#ifdef __cplusplus
}
#endif
