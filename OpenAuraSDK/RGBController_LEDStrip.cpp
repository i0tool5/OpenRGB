/*-----------------------------------------*\
|  RGBController_LEDStrip.cpp               |
|                                           |
|  Generic RGB Interface for OpenAuraSDK    |
|  KeyboardVisualizer LED strip interface   |
|                                           |
|  Adam Honse (CalcProgrammer1) 6/20/2019   |
\*-----------------------------------------*/

#include "RGBController_LEDStrip.h"


RGBController_LEDStrip::RGBController_LEDStrip()
{
    strip = new LEDStrip();

    strip->Initialize((char *)"COM1", 0, 0, 0, 0, 0, 0, 0);
    strip->SetNumLEDs(15);

    name = "LED Strip";

    mode led_mode;
    led_mode.name = "Custom";
    modes.push_back(led_mode);

    for (int i = 0; i < 15; i++)
    {
        colors.push_back(0x00000000);
        led new_led;
        new_led.name = "LED Strip";
        leds.push_back(new_led);
    }

    zone led_zone;
    led_zone.name = "LED Strip";
    std::vector<int> led_zone_map;
    for (int i = 0; i < 15; i++)
    {
        led_zone_map.push_back(i);
    }
    led_zone.map.push_back(led_zone_map);
    zones.push_back(led_zone);
}

int RGBController_LEDStrip::GetMode()
{
    return 0;
}

void RGBController_LEDStrip::SetMode(int mode)
{

}

void RGBController_LEDStrip::SetCustomMode()
{

}

void RGBController_LEDStrip::SetAllLEDs(RGBColor color)
{
    for (int i = 0; i < colors.size(); i++)
    {
        colors[i] = color;
    }

    strip->SetLEDs(colors);
}

void RGBController_LEDStrip::SetAllZoneLEDs(int zone, RGBColor color)
{
    for (int i = 0; i < colors.size(); i++)
    {
        colors[i] = color;
    }

    strip->SetLEDs(colors);
}

void RGBController_LEDStrip::SetLED(int led, RGBColor color)
{
    colors[led] = color;

    strip->SetLEDs(colors);
}