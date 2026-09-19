#include "Application.h"
#include "Config.h"
#include "TestBoxScene.h"
#include "DebugDemoScene.h"
#include "MyCoolScene.h"
#include <iostream>
#include <memory>
#include <string_view>
#include <cstdlib>

namespace 
{   
    std::unique_ptr<GameScene> createScene(const std::string& name, const std::string& resourceDir) 
    {
        if (name == "testbox") 
    	{
            return std::make_unique<TestBoxScene>(resourceDir);
        } 
    	else if (name == "debug") 
    	{
            return std::make_unique<DebugDemoScene>(resourceDir);
        }
    	else if (name == "myCoolScene") 
    	{
    		return std::make_unique<MyCoolScene>(resourceDir);
    	}
        std::cerr << "Unknown scene '" << name << "', using debug scene\n";
        return std::make_unique<DebugDemoScene>(resourceDir);
    }
} 

int main(int argc, char* argv[]) 
{
    try 
    {
        FlyTracer::AppConfig config;
        (void)config.loadFromFile("resources/config.cfg");
		std::string myCoolScene = "myCoolScene";

        auto scene = createScene(myCoolScene, config.resourceDirectory);
        Application app(config.windowWidth, config.windowHeight, config.windowTitle,
                        std::move(scene), config.shaderDirectory);
        app.run();

    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
