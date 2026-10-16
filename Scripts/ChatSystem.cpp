#include "ChatSystem.h"
#include "CraftWorld2.h"

#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/System/SystemManager.h>
#include <Aurora/Graphics/Effects/WeatherSystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <vector>

#define CHAT_MAX_LINES 8
#define CHAT_MAX_CHARS 100
#define CHAT_FADE_AFTER 10.0f
#define CHAT_REMOVE_AFTER 12.0f

ChatSystem::ChatSystem()
{
}

void ChatSystem::AddLine(const std::string& text, float r, float g, float b)
{
    Line line;
    line.text = text;
    if(line.text.size() > 90)
        line.text.resize(90);
    line.r = r;
    line.g = g;
    line.b = b;
    line.age = 0.0f;
    lines.push_back(line);
    while((int)lines.size() > CHAT_MAX_LINES)
        lines.erase(lines.begin());
}

void ChatSystem::Update(float dt)
{
    if(dt < 0.0f)
        dt = 0.0f;
    for(size_t i = 0; i < lines.size(); )
    {
        lines[i].age += dt;
        if(lines[i].age > CHAT_REMOVE_AFTER)
            lines.erase(lines.begin() + (int)i);
        else
            i++;
    }
}

void ChatSystem::Draw(Aurora::Graphics::RenderManager* render)
{
    if(render == NULL || lines.empty())
        return;

    int y = 186;
    for(int i = (int)lines.size() - 1; i >= 0; i--)
    {
        float alpha = 1.0f;
        if(lines[i].age > CHAT_FADE_AFTER)
        {
            alpha = 1.0f - (lines[i].age - CHAT_FADE_AFTER) / (CHAT_REMOVE_AFTER - CHAT_FADE_AFTER);
            if(alpha < 0.0f)
                alpha = 0.0f;
        }
        render->SetFontStyle(0.45f,
            GU_COLOR(lines[i].r, lines[i].g, lines[i].b, alpha),
            GU_COLOR(0.05f, 0.05f, 0.05f, alpha),
            0x00000000);
        render->DebugPrint(8, y, "%s", lines[i].text.c_str());
        y -= 12;
        if(y < 8)
            break;
    }
    render->SetDefaultFont();
}

std::string ChatSystem::Lower(const std::string& s)
{
    std::string out = s;
    for(size_t i = 0; i < out.size(); i++)
        out[i] = (char)tolower((unsigned char)out[i]);
    return out;
}

void ChatSystem::Trim(std::string& s)
{
    size_t start = 0;
    while(start < s.size() && s[start] == ' ')
        start++;
    size_t end = s.size();
    while(end > start && s[end - 1] == ' ')
        end--;
    s = s.substr(start, end - start);
}

void ChatSystem::SetTimeHours(CraftWorld* world, float hours)
{
    while(hours < 0.0f)
        hours += 24.0f;
    while(hours >= 24.0f)
        hours -= 24.0f;
    hours = floorf(hours + 0.5f);
    if(hours >= 24.0f)
        hours = 0.0f;
    world->SetWolrdTime(hours);
    world->SetAllChunksToUpdate();
}

void ChatSystem::SetWeather(CraftWorld* world, int type, float duration)
{
    if(duration < 1.0f)
        duration = 1.0f;
    world->worldWeather->SetWeatherType(type);
    world->worldWeather->SetWeatherDuration(duration);
}

void ChatSystem::TryOpen(Aurora::System::SystemManager* sys, CraftWorld* world)
{
    if(sys == NULL || world == NULL)
        return;

    unsigned short desc[8];
    unsigned short out[CHAT_MAX_CHARS + 1];
    desc[0] = 'C';
    desc[1] = 'h';
    desc[2] = 'a';
    desc[3] = 't';
    desc[4] = 0;
    memset(out, 0, sizeof(out));

    if(sys->ShowOSK(desc, out, CHAT_MAX_CHARS) == -1)
    {
        sys->InputUpdate();
        return;
    }
    sys->InputUpdate();

    std::string msg;
    for(int i = 0; out[i] && i < CHAT_MAX_CHARS; i++)
    {
        unsigned c = out[i];
        if(c >= 32 && c <= 127)
            msg += (char)c;
    }
    Trim(msg);
    if(msg.empty())
        return;

    if(msg[0] == '/')
        HandleCommand(world, msg);
    else
        AddLine(std::string("<Steve> ") + msg, 1.0f, 1.0f, 1.0f);
}

bool ChatSystem::HandleCommand(CraftWorld* world, const std::string& raw)
{
    std::string line = raw;
    if(!line.empty() && line[0] == '/')
        line.erase(0, 1);
    Trim(line);
    if(line.empty())
    {
        AddLine("Unknown command. Type /help", 1.0f, 0.3f, 0.3f);
        return false;
    }

    std::vector<std::string> parts;
    std::string cur;
    for(size_t i = 0; i <= line.size(); i++)
    {
        if(i == line.size() || line[i] == ' ')
        {
            if(!cur.empty())
            {
                parts.push_back(cur);
                cur.clear();
            }
        }
        else
            cur += line[i];
    }

    std::string cmd = Lower(parts[0]);

    if(cmd == "help")
    {
        AddLine("/time set <day|noon|night|midnight|sunrise|0-23>", 1.0f, 1.0f, 0.4f);
        AddLine("/time add <hours>   /time query", 1.0f, 1.0f, 0.4f);
        AddLine("/weather <clear|rain|thunder> [seconds]", 1.0f, 1.0f, 0.4f);
        AddLine("/seed", 1.0f, 1.0f, 0.4f);
        AddLine("/give <item> [amount]", 1.0f, 1.0f, 0.4f);
        return true;
    }

    if(cmd == "give")
    {
        // /give <item> [amount]. Only the self form is implemented -- see the note below on why the
        // selector is accepted and ignored rather than missing.
        if(parts.size() < 2)
        {
            AddLine("Usage: /give [@s] <item> [amount]", 1.0f, 0.3f, 0.3f);
            return false;
        }

        size_t arg = 1;
        // A selector is accepted and discarded: the only player this port can hand an item to is the
        // one holding the PSP, so "@/s", "@s", "@p" and a bare player name all mean the same thing
        // here. Rejecting them would make the command a port-specific special case to remember, and
        // this is a tool meant to be typed without thinking.
        if(!parts[1].empty() && parts[1][0] == '@')
            arg = 2;

        if(parts.size() <= arg)
        {
            AddLine("Usage: /give [@s] <item> [amount]", 1.0f, 0.3f, 0.3f);
            return false;
        }

        // Names with spaces are typed as debug_stick, but if the player quotes them the pieces after
        // the first are still part of the name -- so join everything up to a trailing number.
        std::string itemName = parts[arg];
        int amount = 1;
        bool haveAmount = false;
        for(size_t i = arg + 1; i < parts.size(); i++)
        {
            bool numeric = true;
            for(size_t c = 0; c < parts[i].size(); c++)
            {
                if(isdigit((unsigned char)parts[i][c]) == 0)
                {
                    numeric = false;
                    break;
                }
            }
            if(numeric == true && i == parts.size() - 1)
            {
                amount = atoi(parts[i].c_str());
                haveAmount = true;
            }
            else
            {
                itemName += " ";
                itemName += parts[i];
            }
        }
        if(haveAmount == false)
            amount = 1;

        int id = world->GetItemIdByName(itemName);
        if(id < 0)
        {
            AddLine(std::string("No item called '") + itemName + "'", 1.0f, 0.3f, 0.3f);
            return false;
        }

        int given = world->GiveItemToPlayer(id, amount);
        char buf[128];
        if(given <= 0)
        {
            sprintf(buf, "No room for %s", world->itemTypes[id - 250].name.c_str());
            AddLine(buf, 1.0f, 0.3f, 0.3f);
        }
        else if(given < amount)
        {
            sprintf(buf, "Given %d x %s (pack full)", given, world->itemTypes[id - 250].name.c_str());
            AddLine(buf, 1.0f, 1.0f, 0.4f);
        }
        else
        {
            sprintf(buf, "Given %d x %s", given, world->itemTypes[id - 250].name.c_str());
            AddLine(buf, 1.0f, 1.0f, 0.4f);
        }
        return true;
    }

    if(cmd == "seed")
    {
        char buf[64];
        sprintf(buf, "Seed: %u", world->worldSeed);
        AddLine(buf, 1.0f, 1.0f, 0.4f);
        return true;
    }

    if(cmd == "time")
    {
        if(parts.size() < 2)
        {
            AddLine("Usage: /time set <value> | /time add <hours> | /time query", 1.0f, 0.3f, 0.3f);
            return false;
        }
        std::string sub = Lower(parts[1]);
        if(sub == "query" || sub == "querydaytime")
        {
            char buf[64];
            sprintf(buf, "Time is %.0f:00", world->worldDayTime);
            AddLine(buf, 1.0f, 1.0f, 0.4f);
            return true;
        }
        if(sub == "set")
        {
            if(parts.size() < 3)
            {
                AddLine("Usage: /time set <day|noon|night|midnight|sunrise|0-23>", 1.0f, 0.3f, 0.3f);
                return false;
            }
            std::string val = Lower(parts[2]);
            float hours = -1.0f;
            if(val == "day" || val == "sunrise")
                hours = 8.0f;
            else if(val == "noon")
                hours = 12.0f;
            else if(val == "sunset")
                hours = 18.0f;
            else if(val == "night")
                hours = 20.0f;
            else if(val == "midnight")
                hours = 0.0f;
            else
            {
                int n = 0;
                if(sscanf(parts[2].c_str(), "%d", &n) == 1)
                {
                    if(n >= 1000)
                    {
                        hours = fmodf((n % 24000) / 24000.0f * 24.0f + 6.0f, 24.0f);
                    }
                    else
                        hours = (float)n;
                }
            }
            if(hours < 0.0f)
            {
                AddLine("Unknown time. Try day, noon, night, or 0-23.", 1.0f, 0.3f, 0.3f);
                return false;
            }
            SetTimeHours(world, hours);
            char buf[64];
            sprintf(buf, "Set the time to %.0f:00", world->worldDayTime);
            AddLine(buf, 1.0f, 1.0f, 0.4f);
            return true;
        }
        if(sub == "add")
        {
            if(parts.size() < 3)
            {
                AddLine("Usage: /time add <hours>", 1.0f, 0.3f, 0.3f);
                return false;
            }
            int n = 0;
            if(sscanf(parts[2].c_str(), "%d", &n) != 1)
            {
                AddLine("Usage: /time add <hours>", 1.0f, 0.3f, 0.3f);
                return false;
            }
            SetTimeHours(world, world->worldDayTime + (float)n);
            char buf[64];
            sprintf(buf, "Set the time to %.0f:00", world->worldDayTime);
            AddLine(buf, 1.0f, 1.0f, 0.4f);
            return true;
        }
        AddLine("Usage: /time set <value> | /time add <hours> | /time query", 1.0f, 0.3f, 0.3f);
        return false;
    }

    if(cmd == "weather" || cmd == "toggledownfall")
    {
        if(cmd == "toggledownfall")
        {
            int next = (world->worldWeather->GetWeatherType() == 1) ? 0 : 1;
            SetWeather(world, next, 600.0f);
            AddLine(next ? "Rain started." : "Rain stopped.", 1.0f, 1.0f, 0.4f);
            return true;
        }
        if(parts.size() < 2)
        {
            AddLine(world->worldWeather->GetWeatherType() == 1 ? "Weather: rain" : "Weather: clear", 1.0f, 1.0f, 0.4f);
            return true;
        }
        std::string kind = Lower(parts[1]);
        float duration = 600.0f;
        if(parts.size() >= 3)
        {
            int sec = 0;
            if(sscanf(parts[2].c_str(), "%d", &sec) == 1 && sec > 0)
                duration = (float)sec;
        }
        if(kind == "clear" || kind == "sun")
        {
            SetWeather(world, 0, duration);
            AddLine("Changing to clear weather", 1.0f, 1.0f, 0.4f);
            return true;
        }
        if(kind == "rain" || kind == "rainy")
        {
            SetWeather(world, 1, duration);
            AddLine("Changing to rain", 1.0f, 1.0f, 0.4f);
            return true;
        }
        if(kind == "thunder" || kind == "thunderstorm" || kind == "storm")
        {
            SetWeather(world, 1, duration);
            AddLine("No thunderstorms yet. Changing to rain.", 1.0f, 1.0f, 0.4f);
            return true;
        }
        AddLine("Usage: /weather <clear|rain|thunder> [seconds]", 1.0f, 0.3f, 0.3f);
        return false;
    }

    AddLine("Unknown command. Type /help", 1.0f, 0.3f, 0.3f);
    return false;
}
