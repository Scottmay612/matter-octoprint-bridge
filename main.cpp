/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "LightingAppCommandDelegate.h"
#include "LightingManager.h"
#include <AppMain.h>
#include <cstdlib>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <app/server/Server.h>
#include <lib/support/logging/CHIPLogging.h>

#include <string>

#include <thread>
#include <chrono>
#include <iostream>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <platform/PlatformManager.h>

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

namespace {

NamedPipeCommands sChipNamedPipeCommands;
LightingAppCommandDelegate sLightingAppCommandDelegate;
} // namespace

void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
    if (attributePath.mClusterId == OnOff::Id && attributePath.mAttributeId == OnOff::Attributes::OnOff::Id)
    {
        if (*value) {
            system("gpioset -t0 -c gpiochip4 26=1"); // Turn physical LED ON
        } else {
            system("gpioset -t0 -c gpiochip4 26=0"); // Turn physical LED OFF
        }
        LightingMgr().InitiateAction(*value ? LightingManager::ON_ACTION : LightingManager::OFF_ACTION);
    }
}

/** @brief OnOff Cluster Init
 *
 * This function is called when a specific cluster is initialized. It gives the
 * application an opportunity to take care of cluster initialization procedures.
 * It is called exactly once for each endpoint where cluster is present.
 *
 * @param endpoint   Ver.: always
 *
 * TODO Issue #3841
 * emberAfOnOffClusterInitCallback happens before the stack initialize the cluster
 * attributes to the default value.
 * The logic here expects something similar to the deprecated Plugins callback
 * emberAfPluginOnOffClusterServerPostInitCallback.
 *
 */
void emberAfOnOffClusterInitCallback(EndpointId endpoint)
{
    // TODO: implement any additional Cluster Server init actions
}

void ApplicationInit()
{
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

// --- OUR NEW C++ BACKGROUND THREAD ---
void PollOctoPrint() {
    while (true) {
        FILE* pipe = popen("python3 /home/smay123/get_temp.py", "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                
                // 1. NO EXCEPTIONS: Use strtof instead of stof
                char* endptr;
                float tempC = std::strtof(buffer, &endptr);

                // If endptr moved, it successfully read a number
                if (endptr != buffer) { 
                    int16_t matterTemp = static_cast<int16_t>(tempC * 100);

                    // 2. NO CAPTURING LAMBDAS: Pass the variable through the intptr_t argument
                    (void)chip::DeviceLayer::PlatformMgr().ScheduleWork(
                        [](intptr_t arg) {
                            int16_t temp = static_cast<int16_t>(arg);
                            chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(2, temp);
                        },
                        static_cast<intptr_t>(matterTemp)
                    );
                } else {
                    ChipLogError(NotSpecified, "Failed to parse temperature from Python script");
                }
            }
            pclose(pipe);
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}
// -------------------------------------

#ifdef __NuttX__
// NuttX requires the main function to be defined with C-linkage. However, marking
// the main as extern "C" is not strictly conformant with the C++ standard. Since
// clang >= 20 such code triggers -Wmain warning.
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
    
    std::thread octoThread(PollOctoPrint);
    octoThread.detach();

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
