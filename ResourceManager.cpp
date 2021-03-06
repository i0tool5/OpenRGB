/*-----------------------------------------*\
|  ResourceManager.cpp                      |
|                                           |
|  OpenRGB Resource Manager controls access |
|  to application components including      |
|  RGBControllers, I2C interfaces, and      |
|  network SDK components                   |
|                                           |
|  Adam Honse (CalcProgrammer1) 9/27/2020   |
\*-----------------------------------------*/

#include "ResourceManager.h"
#include "ProfileManager.h"

#include <fstream>
#include <iostream>
#include <string>

std::unique_ptr<ResourceManager> ResourceManager::instance;

using namespace std::chrono_literals;

ResourceManager *ResourceManager::get()
{
    if(!instance)
    {
        instance = std::make_unique<ResourceManager>();
    }
        
    return instance.get();
}

ResourceManager::ResourceManager()
{
    /*-------------------------------------------------------------------------*\
    | Initialize Detection Variables                                            |
    \*-------------------------------------------------------------------------*/
    detection_percent     = 100;
    detection_string      = "";
    detection_is_required = false;
    DetectDevicesThread   = nullptr;

    /*-------------------------------------------------------------------------*\
    | Initialize Server Instance                                                |
    \*-------------------------------------------------------------------------*/
    server = new NetworkServer(rgb_controllers_hw);

    /*-------------------------------------------------------------------------*\
    | Load sizes list from file                                                 |
    \*-------------------------------------------------------------------------*/
    profile_manager         = new ProfileManager(rgb_controllers);
    rgb_controllers_sizes   = profile_manager->LoadProfileToList("sizes.ors");
}

ResourceManager::~ResourceManager()
{
    Cleanup();
}

void ResourceManager::RegisterI2CBus(i2c_smbus_interface *bus)
{
    busses.push_back(bus);
}

std::vector<i2c_smbus_interface*> & ResourceManager::GetI2CBusses()
{
    return busses;
}

void ResourceManager::RegisterRGBController(RGBController *rgb_controller)
{
    rgb_controllers.push_back(rgb_controller);
    DeviceListChanged();
}

std::vector<RGBController*> & ResourceManager::GetRGBControllers()
{
    return rgb_controllers;
}

void ResourceManager::RegisterI2CBusDetector(I2CBusDetectorFunction detector)
{
    i2c_bus_detectors.push_back(detector);
}

void ResourceManager::RegisterI2CDeviceDetector(std::string name, I2CDeviceDetectorFunction detector)
{
    i2c_device_detector_strings.push_back(name);
    i2c_device_detectors.push_back(detector);
}

void ResourceManager::RegisterDeviceDetector(std::string name, DeviceDetectorFunction detector)
{
    device_detector_strings.push_back(name);
    device_detectors.push_back(detector);
}

void ResourceManager::RegisterDeviceListChangeCallback(DeviceListChangeCallback new_callback, void * new_callback_arg)
{
    DeviceListChangeCallbacks.push_back(new_callback);
    DeviceListChangeCallbackArgs.push_back(new_callback_arg);
}

void ResourceManager::RegisterDetectionProgressCallback(DetectionProgressCallback new_callback, void *new_callback_arg)
{
    DetectionProgressCallbacks.push_back(new_callback);
    DetectionProgressCallbackArgs.push_back(new_callback_arg);
}

void ResourceManager::DeviceListChanged()
{
    DeviceListChangeMutex.lock();

    /*-------------------------------------------------*\
    | Insert hardware controllers into controller list  |
    \*-------------------------------------------------*/
    for(unsigned int hw_controller_idx = 0; hw_controller_idx < rgb_controllers_hw.size(); hw_controller_idx++)
    {
        /*-------------------------------------------------*\
        | Check if the controller is already in the list    |
        | at the correct index                              |
        \*-------------------------------------------------*/
        if(hw_controller_idx < rgb_controllers.size())
        {
            if(rgb_controllers[hw_controller_idx] == rgb_controllers_hw[hw_controller_idx])
            {
                continue;
            }
        }

        /*-------------------------------------------------*\
        | If not, check if the controller is already in the |
        | list at a different index                         |
        \*-------------------------------------------------*/
        for(unsigned int controller_idx = 0; controller_idx < rgb_controllers.size(); controller_idx++)
        {
            if(rgb_controllers[controller_idx] == rgb_controllers_hw[hw_controller_idx])
            {
                rgb_controllers.erase(rgb_controllers.begin() + controller_idx);
                rgb_controllers.insert(rgb_controllers.begin() + hw_controller_idx, rgb_controllers_hw[hw_controller_idx]);
                break;
            }
        }

        /*-------------------------------------------------*\
        | If it still hasn't been found, add it to the list |
        \*-------------------------------------------------*/
        rgb_controllers.insert(rgb_controllers.begin() + hw_controller_idx, rgb_controllers_hw[hw_controller_idx]);
    }

    /*-------------------------------------------------*\
    | Device list has changed, call the callbacks       |
    \*-------------------------------------------------*/
    for(unsigned int callback_idx = 0; callback_idx < DeviceListChangeCallbacks.size(); callback_idx++)
    {
        DeviceListChangeCallbacks[callback_idx](DeviceListChangeCallbackArgs[callback_idx]);
    }

    /*-------------------------------------------------*\
    | Device list has changed, inform all clients       |
    | connected to this server                          |
    \*-------------------------------------------------*/
    server->DeviceListChanged();

    DeviceListChangeMutex.unlock();
}

void ResourceManager::DetectionProgressChanged()
{
    DetectionProgressMutex.lock();

    /*-------------------------------------------------*\
    | Detection progress has changed, call the callbacks|
    \*-------------------------------------------------*/
    for(unsigned int callback_idx = 0; callback_idx < DetectionProgressCallbacks.size(); callback_idx++)
    {
        DetectionProgressCallbacks[callback_idx](DetectionProgressCallbackArgs[callback_idx]);
    }

    DetectionProgressMutex.unlock();
}

NetworkServer* ResourceManager::GetServer()
{
    return(server);
}

std::vector<NetworkClient*>& ResourceManager::GetClients()
{
    return(clients);
}

ProfileManager* ResourceManager::GetProfileManager()
{
    return(profile_manager);
}

unsigned int ResourceManager::GetDetectionPercent()
{
    return (detection_percent.load());
}

const char *ResourceManager::GetDetectionString()
{
    return (detection_string);
}

void ResourceManager::Cleanup()
{
    ResourceManager::get()->WaitForDeviceDetection();

    std::vector<RGBController *> rgb_controllers_hw_copy = rgb_controllers_hw;

    for(unsigned int hw_controller_idx = 0; hw_controller_idx < rgb_controllers_hw.size(); hw_controller_idx++)
    {
        for(unsigned int controller_idx = 0; controller_idx < rgb_controllers.size(); controller_idx++)
        {
            if(rgb_controllers[controller_idx] == rgb_controllers_hw[hw_controller_idx])
            {
                rgb_controllers.erase(rgb_controllers.begin() + controller_idx);
                break;
            }
        }
    }

    rgb_controllers_hw.clear();

    for(RGBController* rgb_controller : rgb_controllers_hw_copy)
    {
        delete rgb_controller;
    }

    std::vector<i2c_smbus_interface *> busses_copy = busses;

    busses.clear();

    for(i2c_smbus_interface* bus : busses_copy)
    {
        delete bus;
    }

    if(DetectDevicesThread)
    {
        DetectDevicesThread->join();
        delete DetectDevicesThread;
        DetectDevicesThread = nullptr;
    }
}

void ResourceManager::DetectDevices()
{
    /*-------------------------------------------------*\
    | Do nothing is it is already detecting devices     |
    \*-------------------------------------------------*/
    if(detection_is_required.load())
    {
        return;
    }

    /*-------------------------------------------------*\
    | If there's anything left from the last time,      |
    | we shall remove it first                          |
    \*-------------------------------------------------*/
    detection_percent = 0;
    detection_string  = "";

    DetectionProgressChanged();

    Cleanup();

    DeviceListChanged();

    /*-------------------------------------------------*\
    | Start the device detection thread                 |
    \*-------------------------------------------------*/
    detection_is_required = true;
    DetectDevicesThread = new std::thread(&ResourceManager::DetectDevicesThreadFunction, this);

    /*-------------------------------------------------*\
    | Release the current thread to allow detection     |
    | thread to start                                   |
    \*-------------------------------------------------*/
    std::this_thread::sleep_for(1ms);
}

void ResourceManager::DetectDevicesThreadFunction()
{
    DetectDeviceMutex.lock();

    unsigned int        prev_count = 0;
    float               percent = 0.0f;
    std::vector<bool>   size_used;

    size_used.resize(rgb_controllers_sizes.size());

    for(unsigned int size_idx = 0; size_idx < size_used.size(); size_idx++)
    {
        size_used[size_idx] = false;
    }

    std::vector<std::string> disabled_devices_list;

    /*-------------------------------------------------*\
    | Open device disable list and read in disabled     |
    | device strings                                    |
    \*-------------------------------------------------*/
    std::ifstream infile;
    infile.open("disabled_devices.txt");

    if (infile.good())
    {
        for (std::string line; std::getline(infile, line); )
        {
            disabled_devices_list.push_back(line);
        }
    }

    infile.close();

    /*-------------------------------------------------*\
    | Start at 0% detection progress                    |
    \*-------------------------------------------------*/
    detection_percent = 0;

    /*-------------------------------------------------*\
    | Detect i2c busses                                 |
    \*-------------------------------------------------*/
    for(unsigned int i2c_bus_detector_idx = 0; i2c_bus_detector_idx < i2c_bus_detectors.size() && detection_is_required.load(); i2c_bus_detector_idx++)
    {
        i2c_bus_detectors[i2c_bus_detector_idx](busses);
    }

    /*-------------------------------------------------*\
    | Detect i2c devices                                |
    \*-------------------------------------------------*/
    for(unsigned int i2c_detector_idx = 0; i2c_detector_idx < i2c_device_detectors.size() && detection_is_required.load(); i2c_detector_idx++)
    {
        detection_string = i2c_device_detector_strings[i2c_detector_idx].c_str();
        DetectionProgressChanged();

        bool this_device_disabled = false;
        for(std::size_t disabled_idx = 0; disabled_idx < disabled_devices_list.size(); disabled_idx++)
        {
            if(disabled_devices_list[disabled_idx] == detection_string)
            {
                this_device_disabled = true;
                break;
            }
        }

        if(!this_device_disabled)
            {
            i2c_device_detectors[i2c_detector_idx](busses, rgb_controllers_hw);
            }

        /*-------------------------------------------------*\
        | If the device list size has changed, call the     |
        | device list changed callbacks                     |
        \*-------------------------------------------------*/
        if(rgb_controllers_hw.size() != prev_count)
        {
            /*-------------------------------------------------*\
            | First, load sizes for the new controllers         |
            \*-------------------------------------------------*/
            for(unsigned int controller_size_idx = prev_count - 1; controller_size_idx < rgb_controllers_hw.size(); controller_size_idx++)
            {
                profile_manager->LoadDeviceFromListWithOptions(rgb_controllers_sizes, size_used, rgb_controllers_hw[controller_size_idx], true, false);
            }

            DeviceListChanged();
        }
        prev_count = rgb_controllers_hw.size();

        percent = (i2c_detector_idx + 1.0f) / (i2c_device_detectors.size() + device_detectors.size());

        detection_percent = percent * 100.0f;
    }

    /*-------------------------------------------------*\
    | Detect other devices                              |
    \*-------------------------------------------------*/
    for(unsigned int detector_idx = 0; detector_idx < device_detectors.size() && detection_is_required.load(); detector_idx++)
    {
        detection_string = device_detector_strings[detector_idx].c_str();
        DetectionProgressChanged();

        bool this_device_disabled = false;
        for(std::size_t disabled_idx = 0; disabled_idx < disabled_devices_list.size(); disabled_idx++)
        {
            if(disabled_devices_list[disabled_idx] == detection_string)
            {
                this_device_disabled = true;
                break;
            }
        }

        if(!this_device_disabled)
            {
            device_detectors[detector_idx](rgb_controllers_hw);
            }

        /*-------------------------------------------------*\
        | If the device list size has changed, call the     |
        | device list changed callbacks                     |
        \*-------------------------------------------------*/
        if(rgb_controllers_hw.size() != prev_count)
        {
            /*-------------------------------------------------*\
            | First, load sizes for the new controllers         |
            \*-------------------------------------------------*/
            for(unsigned int controller_size_idx = prev_count - 1; controller_size_idx < rgb_controllers_hw.size(); controller_size_idx++)
            {
                profile_manager->LoadDeviceFromListWithOptions(rgb_controllers_sizes, size_used, rgb_controllers_hw[controller_size_idx], true, false);
            }

            DeviceListChanged();
        }
        prev_count = rgb_controllers_hw.size();

        percent = (detector_idx + 1.0f + i2c_device_detectors.size()) / (i2c_device_detectors.size() + device_detectors.size());

        detection_percent = percent * 100.0f;
    }

    /*-------------------------------------------------*\
    | Make sure that when the detection is done,        |
    | progress bar is set to 100%                       |
    \*-------------------------------------------------*/

    detection_is_required = false;
    detection_percent = 100;
    detection_string = "";

    DetectionProgressChanged();
    
    DetectDeviceMutex.unlock();
}

void ResourceManager::StopDeviceDetection()
{
    detection_is_required = false;
    detection_percent = 100;
    detection_string = "Stopping";
}

void ResourceManager::WaitForDeviceDetection()
{
    DetectDeviceMutex.lock();
    DetectDeviceMutex.unlock();
}
