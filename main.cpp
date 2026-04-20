#include "LightingAppCommandDelegate.h"
#include "LightingManager.h"
#include <AppMain.h>
#include <cstdlib>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <app/server/Server.h>
#include <lib/support/logging/CHIPLogging.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <platform/PlatformManager.h>
#include <platform/CHIPDeviceLayer.h>
#include <app/util/attribute-table.h>
#include <app/clusters/temperature-measurement-server/CodegenIntegration.h>

#if defined(CHIP_IMGUI_ENABLED) && CHIP_IMGUI_ENABLED
#include <imgui_ui/ui.h>
#include <imgui_ui/windows/connectivity.h>
#include <imgui_ui/windows/light.h>
#include <imgui_ui/windows/occupancy_sensing.h>
#include <imgui_ui/windows/qrcode.h>
#endif

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::DeviceLayer;

namespace {
NamedPipeCommands sChipNamedPipeCommands;
LightingAppCommandDelegate sLightingAppCommandDelegate;
} // namespace

// Run a python script and return its output as a float, returns -1 on failure
float RunPythonScript(const char* scriptPath) {
    FILE* pipe = popen(scriptPath, "r");
    if (!pipe) return -1.0f;

    char buffer[128];
    float result = -1.0f;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        char* endptr;
        float val = std::strtof(buffer, &endptr);
        if (endptr != buffer) {
            result = val;
        }
    }
    pclose(pipe);
    return result;
}

void PollingTimerCallback(System::Layer * systemLayer, void * appState) {

    
// --- FETCH HOTEND TEMPERATURE (Now a Thermostat!) ---
    float temp = RunPythonScript("python3 ./get_temp.py");
    if (temp >= 0.0f) {
        int16_t matterTemp = static_cast<int16_t>(temp * 100);
        chip::app::DataModel::Nullable<int16_t> nullableTemp;
        nullableTemp.SetNonNull(matterTemp);
        
        // Use the Thermostat SET command, NOT the TemperatureMeasurement one
        (void)chip::app::Clusters::Thermostat::Attributes::LocalTemperature::Set(2, nullableTemp);
    }
    
    // --- FETCH BED TEMPERATURE ---
    float bedTemp = RunPythonScript("python3 ./get_bed_temp.py");
    if (bedTemp >= 0.0f) {
        int16_t matterBedTemp = static_cast<int16_t>(bedTemp * 100);
        chip::app::DataModel::Nullable<int16_t> nullableBedTemp;
        nullableBedTemp.SetNonNull(matterBedTemp);
        (void)chip::app::Clusters::Thermostat::Attributes::LocalTemperature::Set(3, nullableBedTemp);
    }
    
    // --- FETCH PRINT PROGRESS ---
    float progress = RunPythonScript("python3 ./get_progress.py");
    
    if (progress < 0.0f) {
        progress = 0.0f; 
    }

    uint8_t matterBrightness = static_cast<uint8_t>((progress / 100.0f) * 254);
    if (matterBrightness == 0 && progress > 0.0f) matterBrightness = 1;
    
    chip::app::DataModel::Nullable<uint8_t> nullableLevel(matterBrightness);
    (void)LevelControl::Attributes::CurrentLevel::Set(1, nullableLevel);
    
    
    // --- SYNC ON/OFF WITH PRINTER STATE (only on change) ---
    float isPrinting = RunPythonScript("python3 ./get_state.py");
    bool printerOn = (isPrinting == 1.0f);

    bool currentOnOff = false;
    OnOff::Attributes::OnOff::Get(1, &currentOnOff);

    if (printerOn != currentOnOff) {
        (void)chip::app::Clusters::OnOff::Attributes::OnOff::Set(1, printerOn);
    }
    
    // Schedule next poll in 5 seconds (CRITICAL: Give OctoPrint time to breathe!)
    systemLayer->StartTimer(System::Clock::Milliseconds32(5000), PollingTimerCallback, nullptr);
}

void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size, uint8_t * value)
{
    // --- 1. LIGHTBULB ON/OFF (PAUSE/RESUME PRINT) ---
    if (attributePath.mClusterId == OnOff::Id && attributePath.mAttributeId == OnOff::Attributes::OnOff::Id)
    {
        if (*value) {
            system("python3 ./resume_print.py");
        } else {
            system("python3 ./pause_print.py");
        }
        LightingMgr().InitiateAction(*value ? LightingManager::ON_ACTION : LightingManager::OFF_ACTION);
    }
    
    // --- 2. PROGRESS BAR (READ ONLY) ---
    if (attributePath.mClusterId == LevelControl::Id && 
        attributePath.mAttributeId == LevelControl::Attributes::CurrentLevel::Id)
    {
        return; 
    }

    // --- 3. CATCH THE TEMPERATURE DIAL TURNING ---
    if (attributePath.mClusterId == Thermostat::Id && 
        attributePath.mAttributeId == Thermostat::Attributes::OccupiedHeatingSetpoint::Id)
    {
        int16_t matterTargetTemp;
        memcpy(&matterTargetTemp, value, sizeof(matterTargetTemp));
        float targetTemp = matterTargetTemp / 100.0f;

        char cmd[128];
        if (attributePath.mEndpointId == 2) {
            // It was the Hotend dial
            snprintf(cmd, sizeof(cmd), "python3 ./set_hotend_temp.py %.1f", targetTemp);
            system(cmd);
        } else if (attributePath.mEndpointId == 3) {
            // It was the Bed dial
            snprintf(cmd, sizeof(cmd), "python3 ./set_bed_temp.py %.1f", targetTemp);
            system(cmd);
        }
    }

    // --- 4. CATCH THE "OFF" MODE BUTTON ---
    if (attributePath.mClusterId == Thermostat::Id && 
        attributePath.mAttributeId == Thermostat::Attributes::SystemMode::Id)
    {
        // If the value is 0, the user tapped "Off" in the Google Home App
        if (*value == 0) { 
            if (attributePath.mEndpointId == 2) {
                // Turn off Hotend
                system("python3 ./set_hotend_temp.py 0.0");
            } else if (attributePath.mEndpointId == 3) {
                // Turn off Bed
                system("python3 ./set_bed_temp.py 0.0");
            }
        }
        // (If they tap "Heat", Google Home automatically resends the dial temp, 
        // which triggers block #3 above and heats it right back up!)
    }
}

void emberAfOnOffClusterInitCallback(EndpointId endpoint)
{
    // TODO: implement any additional Cluster Server init actions
}

void ApplicationInit()
{
    // 1. Force the Thermostat to be "Heating Only" (2)
    // Cast to the strict ControlSequenceOfOperationEnum type!
    auto heatingOnly = static_cast<chip::app::Clusters::Thermostat::ControlSequenceOfOperationEnum>(2);
    (void)chip::app::Clusters::Thermostat::Attributes::ControlSequenceOfOperation::Set(2, heatingOnly);
    (void)chip::app::Clusters::Thermostat::Attributes::ControlSequenceOfOperation::Set(3, heatingOnly);

    // 2. Give it a safe default target temperature (20.0°C) so Google Home doesn't freak out
    int16_t defaultTemp = 2000;
    (void)chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Set(2, defaultTemp);
    (void)chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Set(3, defaultTemp);

    // 3. Force BOTH Thermostats into "Heat" mode (4)
    // Cast to the strict SystemModeEnum type!
    auto heatMode = static_cast<chip::app::Clusters::Thermostat::SystemModeEnum>(4);
    (void)chip::app::Clusters::Thermostat::Attributes::SystemMode::Set(2, heatMode);
    (void)chip::app::Clusters::Thermostat::Attributes::SystemMode::Set(3, heatMode);

    // 4. Hardcode Hotend Limits (0°C to 300°C)
    int16_t minTemp = 0;
    int16_t maxHotend = 30000;
    (void)chip::app::Clusters::Thermostat::Attributes::AbsMinHeatSetpointLimit::Set(2, minTemp);
    (void)chip::app::Clusters::Thermostat::Attributes::MinHeatSetpointLimit::Set(2, minTemp);
    (void)chip::app::Clusters::Thermostat::Attributes::AbsMaxHeatSetpointLimit::Set(2, maxHotend);
    (void)chip::app::Clusters::Thermostat::Attributes::MaxHeatSetpointLimit::Set(2, maxHotend);

    // 5. Hardcode Bed Limits (0°C to 120°C)
    int16_t maxBed = 12000;
    (void)chip::app::Clusters::Thermostat::Attributes::AbsMinHeatSetpointLimit::Set(3, minTemp);
    (void)chip::app::Clusters::Thermostat::Attributes::MinHeatSetpointLimit::Set(3, minTemp);
    (void)chip::app::Clusters::Thermostat::Attributes::AbsMaxHeatSetpointLimit::Set(3, maxBed);
    (void)chip::app::Clusters::Thermostat::Attributes::MaxHeatSetpointLimit::Set(3, maxBed);

    // 6. Start the pipe
    std::string path = std::string(LinuxDeviceOptions::GetInstance().app_pipe);
    if ((!path.empty()) and (sChipNamedPipeCommands.Start(path, &sLightingAppCommandDelegate) != CHIP_NO_ERROR))
    {
        ChipLogError(NotSpecified, "Failed to start CHIP NamedPipeCommands");
        TEMPORARY_RETURN_IGNORED sChipNamedPipeCommands.Stop();
    }
}
void ApplicationShutdown()
{
    if (sChipNamedPipeCommands.Stop() != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Failed to stop CHIP NamedPipeCommands");
    }
}

#ifdef __NuttX__
extern "C" {
#endif

int main(int argc, char * argv[])
{
    if (ChipLinuxAppInit(argc, argv) != 0)
    {
        return -1;
    }

    CHIP_ERROR err = LightingMgr().Init();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Failed to initialize lighting manager: %" CHIP_ERROR_FORMAT, err.Format());
        chip::DeviceLayer::PlatformMgr().Shutdown();
        return -1;
    }

    // Start the polling timer immediately
    DeviceLayer::SystemLayer().StartTimer(System::Clock::Milliseconds32(0), PollingTimerCallback, nullptr);

#if defined(CHIP_IMGUI_ENABLED) && CHIP_IMGUI_ENABLED
    example::Ui::ImguiUi ui;
    ui.AddWindow(std::make_unique<example::Ui::Windows::QRCode>());
    ui.AddWindow(std::make_unique<example::Ui::Windows::Connectivity>());
    ui.AddWindow(std::make_unique<example::Ui::Windows::OccupancySensing>(chip::EndpointId(1), "Occupancy"));
    ui.AddWindow(std::make_unique<example::Ui::Windows::Light>(chip::EndpointId(1)));
    ChipLinuxAppMainLoop(&ui);
#else
    ChipLinuxAppMainLoop();
#endif

    return 0;
}

#ifdef __NuttX__
}
#endif