#ifndef CHATSYSTEM_H_
#define CHATSYSTEM_H_

#include <string>
#include <vector>

class CraftWorld;

namespace Aurora
{
    namespace Graphics { class RenderManager; }
    namespace System { class SystemManager; }
}

class ChatSystem
{
public:
    ChatSystem();

    void TryOpen(Aurora::System::SystemManager* sys, CraftWorld* world);
    void Update(float dt);
    void Draw(Aurora::Graphics::RenderManager* render);

    void AddLine(const std::string& text, float r, float g, float b);

private:
    struct Line
    {
        std::string text;
        float r, g, b;
        float age;
    };

    std::vector<Line> lines;

    bool HandleCommand(CraftWorld* world, const std::string& raw);
    void SetTimeHours(CraftWorld* world, float hours);
    void SetWeather(CraftWorld* world, int type, float duration);
    static std::string Lower(const std::string& s);
    static void Trim(std::string& s);
};

#endif
