#include <iostream>
#include <irrlicht/irrlicht.h>
#include <sstream>
#include <WallpaperEngine/Irrlicht/CPkgReader.h>
#include <getopt.h>
#include <SDL_mixer.h>
#include <SDL.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "WallpaperEngine/Render/Shaders/Compiler.h"
#include "WallpaperEngine/Irrlicht/CImageLoaderTEX.h"

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Irrlicht/CContext.h"
#include "WallpaperEngine/Render/CScene.h"

bool IsRootWindow = false;
std::vector<std::string> Screens;
std::vector<irr::core::rect<irr::s32>> Viewports;

irr::f32 g_Time = 0;

WallpaperEngine::Irrlicht::CContext* IrrlichtContext;
void initialize_viewports (irr::SIrrlichtCreationParameters& irrlichtCreationParameters)
{
    if (IsRootWindow == false || Screens.empty () == true)
        return;

    Display* display = XOpenDisplay (NULL);
    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (display, &xrandr_result, &xrandr_error))
    {
        std::cerr << "XRandr is not present, cannot detect specified screens, running in window mode" << std::endl;
        return;
    }

    XRRScreenResources* screenResources = XRRGetScreenResources (display, DefaultRootWindow (display));

    // there are some situations where xrandr returns null (like screen not using the extension)
    if (screenResources == nullptr)
        return;

    for (int i = 0; i < screenResources->noutput; i ++)
    {
        XRROutputInfo* info = XRRGetOutputInfo (display, screenResources, screenResources->outputs [i]);

        // there are some situations where xrandr returns null (like screen not using the extension)
        if (info == nullptr)
            continue;

        auto cur = Screens.begin ();
        auto end = Screens.end ();

        for (; cur != end; cur ++)
        {
            if (info->connection == RR_Connected && strcmp (info->name, (*cur).c_str ()) == 0)
            {
                XRRCrtcInfo* crtc = XRRGetCrtcInfo (display, screenResources, info->crtc);

                std::cout << "Found requested screen: " << info->name << " -> " << crtc->x << "x" << crtc->y << ":" << crtc->width << "x" << crtc->height << std::endl;

                irr::core::rect<irr::s32> viewport;

                viewport.UpperLeftCorner.X = crtc->x;
                viewport.UpperLeftCorner.Y = crtc->y;
                viewport.LowerRightCorner.X = crtc->x + crtc->width;
                viewport.LowerRightCorner.Y = crtc->y + crtc->height;

                Viewports.push_back (viewport);

                XRRFreeCrtcInfo (crtc);
            }
        }

        XRRFreeOutputInfo (info);
    }

    XRRFreeScreenResources (screenResources);

    irrlichtCreationParameters.WindowId = reinterpret_cast<void*> (DefaultRootWindow (display));
}

int init_irrlicht()
{
    IrrlichtContext = new WallpaperEngine::Irrlicht::CContext ();

    irr::SIrrlichtCreationParameters irrlichtCreationParameters;
    // prepare basic configuration for irrlicht
    irrlichtCreationParameters.AntiAlias = 8;
    irrlichtCreationParameters.Bits = 16;
    // _irr_params.DeviceType = Irrlicht::EIDT_X11;
    irrlichtCreationParameters.DriverType = irr::video::EDT_OPENGL;
    irrlichtCreationParameters.Doublebuffer = false;
    irrlichtCreationParameters.EventReceiver = nullptr;
    irrlichtCreationParameters.Fullscreen = false;
    irrlichtCreationParameters.HandleSRGB = false;
    irrlichtCreationParameters.IgnoreInput = true;
    irrlichtCreationParameters.Stencilbuffer = true;
    irrlichtCreationParameters.UsePerformanceTimer = false;
    irrlichtCreationParameters.Vsync = false;
    irrlichtCreationParameters.WithAlphaChannel = false;
    irrlichtCreationParameters.ZBufferBits = 24;
    irrlichtCreationParameters.LoggingLevel = irr::ELL_DEBUG;

    initialize_viewports (irrlichtCreationParameters);

    IrrlichtContext->setDevice (irr::createDeviceEx (irrlichtCreationParameters));

    if (IrrlichtContext->getDevice () == nullptr)
    {
        return 1;
    }

    IrrlichtContext->getDevice ()->setWindowCaption (L"Test game");

    // check for ps and vs support
    if (
            IrrlichtContext->getDevice ()->getVideoDriver()->queryFeature (irr::video::EVDF_PIXEL_SHADER_1_1) == false &&
            IrrlichtContext->getDevice ()->getVideoDriver()->queryFeature (irr::video::EVDF_ARB_FRAGMENT_PROGRAM_1) == false)
    {
        IrrlichtContext->getDevice ()->getLogger ()->log ("WARNING: Pixel shaders disabled because of missing driver/hardware support");
    }

    if (
            IrrlichtContext->getDevice ()->getVideoDriver()->queryFeature (irr::video::EVDF_VERTEX_SHADER_1_1) == false &&
            IrrlichtContext->getDevice ()->getVideoDriver()->queryFeature (irr::video::EVDF_ARB_VERTEX_PROGRAM_1) == false)
    {
        IrrlichtContext->getDevice ()->getLogger ()->log ("WARNING: Vertex shaders disabled because of missing driver/hardware support");
    }

    if (IrrlichtContext->getDevice ()->getVideoDriver ()->queryFeature (irr::video::EVDF_RENDER_TO_TARGET) == false)
    {
        IrrlichtContext->getDevice ()->getLogger ()->log ("ERROR: Your hardware or this renderer do not support rendering to texture");
        return 1;
    }

    return 0;
}

void preconfigure_wallpaper_engine ()
{
    // load the assets from wallpaper engine
    IrrlichtContext->getDevice ()->getFileSystem ()->addFileArchive ("assets.zip", true, false);

    // register custom loaders
    IrrlichtContext->getDevice ()->getVideoDriver()->addExternalImageLoader (
        new WallpaperEngine::Irrlicht::CImageLoaderTex (IrrlichtContext)
    );
    IrrlichtContext->getDevice ()->getFileSystem ()->addArchiveLoader (
        new WallpaperEngine::Irrlicht::CArchiveLoaderPkg (IrrlichtContext)
    );
}

void print_help (const char* route)
{
    std::cout
        << "Usage:" << route << " [options] " << std::endl
        << "options:" << std::endl
        << "  --silent\t\tMutes all the sound the wallpaper might produce" << std::endl
        << "  --dir <folder>\tLoads an uncompressed background from the given <folder>" << std::endl
        << "  --pkg <folder>\tLoads a scene.pkg file from the given <folder>" << std::endl
        << "  --screen-root <screen name>\tDisplay as screen's background" << std::endl
        << "  --fps <maximum-fps>\tLimits the FPS to the given number, useful to keep battery consumption low" << std::endl;
}

std::string stringPathFixes(const std::string& s){
    std::string str(s);
    if(str.empty())
        return s;
    if(str[0] == '\'' && str[str.size() - 1] == '\''){
        str.erase(str.size() - 1, 1);
        str.erase(0,1);
    }
    if(str[str.size() - 1] != '/')
        str += '/';
    return std::move(str);
}

int main (int argc, char* argv[])
{
    int mode = 0;
    int max_fps = 30;
    bool audio_support = true;
    std::string path;

    int option_index = 0;

    static struct option long_options [] = {
            {"screen-root", required_argument, 0, 'r'},
            {"pkg",         required_argument, 0, 'p'},
            {"dir",         required_argument, 0, 'd'},
            {"silent",      no_argument,       0, 's'},
            {"help",        no_argument,       0, 'h'},
            {"fps",         required_argument, 0, 'f'},
            {nullptr,              0, 0,   0}
    };

    while (true)
    {
        int c = getopt_long (argc, argv, "r:p:d:shf:", long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'r':
                IsRootWindow = true;
                Screens.emplace_back (optarg);
                break;

            case 'p':
                mode = 1;
                path = optarg;
                break;

            case 'd':
                mode = 2;
                path = optarg;
                break;

            case 's':
                audio_support = false;
                break;

            case 'h':
                print_help (argv [0]);
                return 0;

            case 'f':
                max_fps = atoi (optarg);
                break;

            default:
                break;
        }
    }

    if (init_irrlicht ())
    {
        return 1;
    }

    preconfigure_wallpaper_engine ();

    irr::io::path wallpaper_path;
    irr::io::path project_path;
    irr::io::path scene_path;

    switch (mode)
    {
        case 0:
            print_help (argv [0]);
            return 0;

        // pkg mode
        case 1:
            path = stringPathFixes(path);
            wallpaper_path = IrrlichtContext->getDevice ()->getFileSystem ()->getAbsolutePath (path.c_str ());
            project_path = wallpaper_path + "project.json";
            scene_path = wallpaper_path + "scene.pkg";

            IrrlichtContext->getDevice ()->getFileSystem ()->addFileArchive (scene_path, true, false); // add the pkg file to the lookup list
            break;

        // folder mode
        case 2:
            path = stringPathFixes(path);
            wallpaper_path = IrrlichtContext->getDevice ()->getFileSystem ()->getAbsolutePath (path.c_str ());
            project_path = wallpaper_path + "project.json";

            // set our working directory
            IrrlichtContext->getDevice ()->getFileSystem ()->changeWorkingDirectoryTo (wallpaper_path);
            break;

        default:
            break;
    }

    if (audio_support == true)
    {
        int mixer_flags = MIX_INIT_MP3 | MIX_INIT_FLAC | MIX_INIT_OGG;

        if (SDL_Init (SDL_INIT_AUDIO) < 0 || mixer_flags != Mix_Init (mixer_flags))
        {
            IrrlichtContext->getDevice ()->getLogger ()->log ("Cannot initialize SDL audio system", irr::ELL_ERROR);
            return -1;
        }

        // initialize audio engine
        Mix_OpenAudio (22050, AUDIO_S16SYS, 2, 640);
    }

    WallpaperEngine::Core::CProject* project = WallpaperEngine::Core::CProject::fromFile (project_path);
    WallpaperEngine::Render::CScene* sceneRender = new WallpaperEngine::Render::CScene (project, IrrlichtContext);

    irr::u32 lastTime = 0;
    irr::u32 minimumTime = 1000 / max_fps;
    irr::u32 currentTime = 0;

    irr::u32 startTime = 0;
    irr::u32 endTime = 0;

    IrrlichtContext->getDevice ()->getSceneManager ()->setAmbientLight (sceneRender->getScene ()->getAmbientColor ().toSColor ());
    while (IrrlichtContext && IrrlichtContext->getDevice () && IrrlichtContext->getDevice ()->run ())
    {
        if (IrrlichtContext->getDevice ()->getVideoDriver () == nullptr)
            continue;

        // if (device->isWindowActive ())
        {
            currentTime = startTime = IrrlichtContext->getDevice ()->getTimer ()->getTime ();
            g_Time = currentTime / 1000.0f;

            if (Viewports.size () > 0)
            {
                auto cur = Viewports.begin ();
                auto end = Viewports.end ();

                for (; cur != end; cur ++)
                {
                    // change viewport to render to the correct portion of the display
                    IrrlichtContext->getDevice ()->getVideoDriver ()->setViewPort (*cur);

                    IrrlichtContext->getDevice ()->getVideoDriver ()->beginScene (false, true, sceneRender->getScene ()->getClearColor ().toSColor());
                    IrrlichtContext->getDevice ()->getSceneManager ()->drawAll ();
                    IrrlichtContext->getDevice ()->getVideoDriver ()->endScene ();
                }
            }
            else
            {
                IrrlichtContext->getDevice ()->getVideoDriver ()->beginScene (true, true, sceneRender->getScene ()->getClearColor ().toSColor());
                IrrlichtContext->getDevice ()->getSceneManager ()->drawAll ();
                IrrlichtContext->getDevice ()->getVideoDriver ()->endScene ();
            }

            endTime = IrrlichtContext->getDevice ()->getTimer ()->getTime ();

            IrrlichtContext->getDevice ()->sleep (minimumTime - (endTime - startTime), false);
        }
    }

    SDL_Quit ();
    return 0;
}