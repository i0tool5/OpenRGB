#include "ProfileManager.h"
#include "RGBController_Dummy.h"
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <cstring>

namespace fs = std::experimental::filesystem;

ProfileManager::ProfileManager(std::vector<RGBController *>& control) : controllers(control)
{
    UpdateProfileList();
}

ProfileManager::~ProfileManager()
{

}

bool ProfileManager::SaveProfile(std::string profile_name)
{
    /*---------------------------------------------------------*\
    | If a name was entered, save the profile file              |
    \*---------------------------------------------------------*/
    if(profile_name != "")
    {
        /*---------------------------------------------------------*\
        | Open an output file in binary mode                        |
        \*---------------------------------------------------------*/
        std::ofstream controller_file(profile_name, std::ios::out | std::ios::binary);

        /*---------------------------------------------------------*\
        | Write header                                              |
        | 16 bytes - "OPENRGB_PROFILE"                              |
        | 4 bytes - Version, unsigned int                           |
        \*---------------------------------------------------------*/
        unsigned int profile_version = 1;
        controller_file.write("OPENRGB_PROFILE", 16);
        controller_file.write((char *)&profile_version, sizeof(unsigned int));

        /*---------------------------------------------------------*\
        | Write controller data for each controller                 |
        \*---------------------------------------------------------*/
        for(std::size_t controller_index = 0; controller_index < controllers.size(); controller_index++)
        {
            unsigned char *controller_data = controllers[controller_index]->GetDeviceDescription();
            unsigned int controller_size;

            memcpy(&controller_size, controller_data, sizeof(controller_size));

            controller_file.write((const char *)controller_data, controller_size);
        }

        /*---------------------------------------------------------*\
        | Close the file when done                                  |
        \*---------------------------------------------------------*/
        controller_file.close();

        /*---------------------------------------------------------*\
        | Update the profile list                                   |
        \*---------------------------------------------------------*/
        UpdateProfileList();

        return(true);
    }
    else
    {
        return(false);
    }
}

bool ProfileManager::LoadProfile(std::string profile_name)
{
    return(LoadProfileWithOptions(profile_name, false, true));
}

bool ProfileManager::LoadSizeFromProfile(std::string profile_name)
{
    return(LoadProfileWithOptions(profile_name, true, false));
}

std::vector<RGBController*> ProfileManager::LoadProfileToList
    (
    std::string     profile_name
    )
{
    std::vector<RGBController*> temp_controllers;
    unsigned int                controller_size;
    unsigned int                controller_offset = 0;
    bool                        ret_val = false;

    std::string filename = profile_name;

    /*---------------------------------------------------------*\
    | Open input file in binary mode                            |
    \*---------------------------------------------------------*/
    std::ifstream controller_file(filename, std::ios::in | std::ios::binary);

    /*---------------------------------------------------------*\
    | Read and verify file header                               |
    \*---------------------------------------------------------*/
    char            header_string[16]{};
    unsigned int    header_version;

    controller_file.read(header_string, 16);
    controller_file.read((char *)&header_version, sizeof(unsigned int));

    controller_offset += 16 + sizeof(unsigned int);
    controller_file.seekg(controller_offset);

    if(strcmp(header_string, "OPENRGB_PROFILE") == 0)
    {
        if(header_version == 1)
        {
            /*---------------------------------------------------------*\
            | Read controller data from file until EOF                  |
            \*---------------------------------------------------------*/
            while(!(controller_file.peek() == EOF))
            {
                controller_file.read((char *)&controller_size, sizeof(controller_size));

                unsigned char *controller_data = new unsigned char[controller_size];

                controller_file.seekg(controller_offset);

                controller_file.read((char *)controller_data, controller_size);

                RGBController_Dummy *temp_controller = new RGBController_Dummy();

                temp_controller->ReadDeviceDescription(controller_data);

                temp_controllers.push_back(temp_controller);

                delete[] controller_data;

                controller_offset += controller_size;
                controller_file.seekg(controller_offset);

                ret_val = true;
            }
        }
    }

    return(temp_controllers);
}

bool ProfileManager::LoadDeviceFromListWithOptions
    (
    std::vector<RGBController*>&    temp_controllers,
    std::vector<bool>&              temp_controller_used,
    RGBController*                  load_controller,
    bool                            load_size,
    bool                            load_settings
    )
{
    for(std::size_t temp_index = 0; temp_index < temp_controllers.size(); temp_index++)
    {
        RGBController *temp_controller = temp_controllers[temp_index];

        /*---------------------------------------------------------*\
        | Test if saved controller data matches this controller     |
        \*---------------------------------------------------------*/
        if((temp_controller_used[temp_index] == false                   )
         &&(temp_controller->type        == load_controller->type       )
         &&(temp_controller->name        == load_controller->name       )
         &&(temp_controller->description == load_controller->description)
         &&(temp_controller->version     == load_controller->version    )
         &&(temp_controller->serial      == load_controller->serial     )
         &&(temp_controller->location    == load_controller->location   ))
        {
            /*---------------------------------------------------------*\
            | Update zone sizes if requested                            |
            \*---------------------------------------------------------*/
            if(load_size)
            {
                if(temp_controller->zones.size() == load_controller->zones.size())
                {
                    for(std::size_t zone_idx = 0; zone_idx < temp_controller->zones.size(); zone_idx++)
                    {
                        if((temp_controller->zones[zone_idx].name       == load_controller->zones[zone_idx].name      )
                         &&(temp_controller->zones[zone_idx].type       == load_controller->zones[zone_idx].type      )
                         &&(temp_controller->zones[zone_idx].leds_min   == load_controller->zones[zone_idx].leds_min  )
                         &&(temp_controller->zones[zone_idx].leds_max   == load_controller->zones[zone_idx].leds_max  )
                         &&(temp_controller->zones[zone_idx].leds_count != load_controller->zones[zone_idx].leds_count))
                        {
                            load_controller->ResizeZone(zone_idx, temp_controller->zones[zone_idx].leds_count);
                        }
                    }
                }
            }

            /*---------------------------------------------------------*\
            | Update settings if requested                              |
            \*---------------------------------------------------------*/
            if(load_settings)
            {
                /*---------------------------------------------------------*\
                | Update all modes                                          |
                \*---------------------------------------------------------*/
                if(temp_controller->modes.size() == load_controller->modes.size())
                {
                    for(std::size_t mode_index = 0; mode_index < temp_controller->modes.size(); mode_index++)
                    {
                        if((temp_controller->modes[mode_index].name       == load_controller->modes[mode_index].name      )
                         &&(temp_controller->modes[mode_index].value      == load_controller->modes[mode_index].value     )
                         &&(temp_controller->modes[mode_index].flags      == load_controller->modes[mode_index].flags     )
                         &&(temp_controller->modes[mode_index].speed_min  == load_controller->modes[mode_index].speed_min )
                         &&(temp_controller->modes[mode_index].speed_max  == load_controller->modes[mode_index].speed_max )
                         &&(temp_controller->modes[mode_index].colors_min == load_controller->modes[mode_index].colors_min)
                         &&(temp_controller->modes[mode_index].colors_max == load_controller->modes[mode_index].colors_max))
                        {
                            load_controller->modes[mode_index].speed      = temp_controller->modes[mode_index].speed;
                            load_controller->modes[mode_index].direction  = temp_controller->modes[mode_index].direction;
                            load_controller->modes[mode_index].color_mode = temp_controller->modes[mode_index].color_mode;

                            load_controller->modes[mode_index].colors.resize(temp_controller->modes[mode_index].colors.size());

                            for(std::size_t mode_color_index = 0; mode_color_index < temp_controller->modes[mode_index].colors.size(); mode_color_index++)
                            {
                                load_controller->modes[mode_index].colors[mode_color_index] = temp_controller->modes[mode_index].colors[mode_color_index];
                            }
                        }

                    }

                    load_controller->active_mode = temp_controller->active_mode;
                }

                /*---------------------------------------------------------*\
                | Update all colors                                         |
                \*---------------------------------------------------------*/
                if(temp_controller->colors.size() == load_controller->colors.size())
                {
                    for(std::size_t color_index = 0; color_index < temp_controller->colors.size(); color_index++)
                    {
                        load_controller->colors[color_index] = temp_controller->colors[color_index];
                    }
                }

                temp_controller_used[temp_index] = true;

                return(true);
            }
        }
    }

    return(false);
}

bool ProfileManager::LoadProfileWithOptions
    (
    std::string     profile_name,
    bool            load_size,
    bool            load_settings
    )
{
    std::vector<RGBController*> temp_controllers;
    std::vector<bool>           temp_controller_used;
    bool                        ret_val = false;

    std::string filename = profile_name;

    /*---------------------------------------------------------*\
    | Open input file in binary mode                            |
    \*---------------------------------------------------------*/
    temp_controllers = LoadProfileToList(profile_name);

    /*---------------------------------------------------------*\
    | Set up used flag vector                                   |
    \*---------------------------------------------------------*/
    temp_controller_used.resize(temp_controllers.size());

    for(unsigned int controller_idx = 0; controller_idx < temp_controller_used.size(); controller_idx++)
    {
        temp_controller_used[controller_idx] = false;
    }

    /*---------------------------------------------------------*\
    | Loop through all controllers.  For each controller, search|
    | all saved controllers until a match is found              |
    \*---------------------------------------------------------*/
    for(std::size_t controller_index = 0; controller_index < controllers.size(); controller_index++)
    {
        if(LoadDeviceFromListWithOptions(temp_controllers, temp_controller_used, controllers[controller_index], load_size, load_settings))
        {
            ret_val = true;
        }
    }

    /*---------------------------------------------------------*\
    | Delete all temporary controllers                          |
    \*---------------------------------------------------------*/
    for(unsigned int controller_idx = 0; controller_idx < temp_controllers.size(); controller_idx++)
    {
        delete temp_controllers[controller_idx];
    }

    return(ret_val);
}

void ProfileManager::DeleteProfile(std::string profile_name)
{
    remove(profile_name.c_str());

    UpdateProfileList();
}

void ProfileManager::UpdateProfileList()
{
    profile_list.clear();

    /*---------------------------------------------------------*\
    | Load profiles by looking for .orp files in current dir    |
    \*---------------------------------------------------------*/
    for(const auto & entry : fs::directory_iterator("."))
    {
        std::string filename = entry.path().filename().string();

        if(filename.find(".orp") != std::string::npos)
        {
            /*---------------------------------------------------------*\
            | Open input file in binary mode                            |
            \*---------------------------------------------------------*/
            std::ifstream profile_file(filename, std::ios::in | std::ios::binary);

            /*---------------------------------------------------------*\
            | Read and verify file header                               |
            \*---------------------------------------------------------*/
            char            header_string[16];
            unsigned int    header_version;

            profile_file.read(header_string, 16);
            profile_file.read((char *)&header_version, sizeof(unsigned int));

            if(strcmp(header_string, "OPENRGB_PROFILE") == 0)
            {
                if(header_version == 1)
                {
                    /*---------------------------------------------------------*\
                    | Add this profile to the list                              |
                    \*---------------------------------------------------------*/
                    profile_list.push_back(filename);
                }
            }

            profile_file.close();
        }
    }
}
