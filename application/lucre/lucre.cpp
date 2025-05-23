/* Engine Copyright (c) 2022 Engine Development Team 
   https://github.com/beaumanvienna/vulkan

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation files
   (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/

#include <thread>

#include "engine.h"
#include "coreSettings.h"
#include "resources/resources.h"
#include "events/controllerEvent.h"
#include "events/applicationEvent.h"
#include "events/keyEvent.h"

#include "lucre.h"
#include "UI/imgui.h"
#include "keyboardInputController.h"

namespace LucreApp
{

    std::shared_ptr<Lucre> Lucre::m_Application;
    SpriteSheet* Lucre::m_Spritesheet;

    Lucre::Lucre()
        : m_CurrentScene{nullptr}, m_GUIisRunning{false}
    {
    }

    bool Lucre::Start()
    {
        InitSettings();

        m_Window = Engine::m_Engine->GetWindow();
        m_Window->SetWindowAspectRatio();
        InitCursor();

        m_Atlas.AddSpritesheet();
        m_Spritesheet = &m_Atlas;

        m_GameState.Start();
        m_CurrentScene = m_GameState.GetScene();

        m_UI = new UI("UI");
        Engine::m_Engine->PushLayer(m_UI);

        m_UIControllerIcon = new UIControllerIcon(false /* indent */, "UI controller");
        Engine::m_Engine->PushOverlay(m_UIControllerIcon);

        m_Renderer = Engine::m_Engine->GetRenderer();

        // create orthogonal camera 
        m_CameraController = std::make_shared<CameraController>(Camera::ORTHOGRAPHIC_PROJECTION);
        auto& camera = m_CameraController->GetCamera();
        auto position = glm::vec3(0.0f, 0.0f, 1.0f);
        auto direction = glm::vec3(0.0f, 0.0f, -1.0f);
        camera.SetViewDirection(position, direction);

        return true;
    }

    void Lucre::Shutdown()
    {
        m_GameState.Stop();
    }

    void Lucre::OnUpdate(const Timestep& timestep)
    {
        m_CurrentScene = m_GameState.OnUpdate();
        m_CurrentScene->OnUpdate(timestep);

        // update/render layer stack
        // helath bar
        if (static_cast<int>(m_GameState.GetState()) > static_cast<int>(GameState::State::CUTSCENE)) m_UI->Health(90.0f);
        // controller icons
        m_UIControllerIcon->Indent(m_GameState.GetState() == GameState::State::SETTINGS);
        m_UIControllerIcon->OnUpdate(timestep);
        m_Renderer->Submit2D(&m_CameraController->GetCamera(), m_UIControllerIcon->m_Registry);
        // gui
        if (m_GUIisRunning)
        {
            m_UI->OnUpdate(timestep);  // direct submits
        }

        m_Renderer->EndScene();
    }

    void Lucre::OnResize()
    {
        ASSERT(m_CurrentScene);
        m_CurrentScene->OnResize();
        m_UIControllerIcon->Init();
        m_UI->OnResize();
        m_CameraController->SetProjection();
    }

    void Lucre::InitCursor()
    {
        size_t fileSize;
        const uchar* data;

        m_EmptyCursor = Cursor::Create();
        data = (const uchar*) ResourceSystem::GetDataPointer(fileSize, "/images/images/cursorEmpty.png", IDB_CURSOR_EMPTY, "PNG");
        m_EmptyCursor->SetCursor(data, fileSize, 1, 1);

        m_Cursor = Cursor::Create();
        data = (const uchar*) ResourceSystem::GetDataPointer(fileSize, "/images/images/cursor.png", IDB_CURSOR_RETRO, "PNG");
        m_Cursor->SetCursor(data, fileSize, 32, 32);

        Engine::m_Engine->AllowCursor();
    }

    void Lucre::ShowCursor()
    {
        m_Cursor->RestoreCursor();
    }

    void Lucre::HideCursor()
    {
        m_EmptyCursor->RestoreCursor();
    }

    void Lucre::InitSettings()
    {
        m_AppSettings.InitDefaults();
        m_AppSettings.RegisterSettings();

        // apply external settings
        Engine::m_Engine->ApplyAppSettings();
    }

    void Lucre::PlaySound(int resourceID)
    {
        if (CoreSettings::m_EnableSystemSounds)
        {
            switch(resourceID)
            {
                case IDR_WAVES:
                    Engine::m_Engine->PlaySound("/sounds/waves.ogg", IDR_WAVES, "OGG");
                    break;
                case IDR_BUCKLE:
                    Engine::m_Engine->PlaySound("/sounds/buckle.ogg", IDR_BUCKLE, "OGG");
                    break;
            }

        }
    }

    // cancels gameplay or cancels the GUI
    void Lucre::Cancel()
    {
        if (m_GameState.GetState() != GameState::State::SPLASH)
        {
            m_GUIisRunning = !m_GUIisRunning;
        }
    }

    void Lucre::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);

        dispatcher.Dispatch<ControllerButtonPressedEvent>([this](ControllerButtonPressedEvent event)
            {
                switch (event.GetControllerButton())
                {
                    case Controller::BUTTON_GUIDE:
                        Cancel();
                        break;
                    case Controller::BUTTON_A:
                        PlaySound(IDR_BUCKLE);
                        break;
                }
                return false;
            }
        );

        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent event)
            {
                switch(event.GetKeyCode())
                {
                    case ENGINE_KEY_M:
                        Engine::m_Engine->ToggleDebugWindow(LucreApp::ImGUI::DebugWindow);
                        break;
                    case ENGINE_KEY_ESCAPE:
                        Cancel();
                        break;
                }
                return false;
            }
        );

        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent event)
            {
                OnResize();
                return true;
            }
        );

        // dispatch to application
        if (!event.IsHandled())
        {
            m_CurrentScene->OnEvent(event);
        }
    }

    void Lucre::OnAppEvent(AppEvent& event)
    {
        AppEventDispatcher appDispatcher(event);

        appDispatcher.Dispatch<SceneChangedEvent>([this](SceneChangedEvent event)
            {
                if (m_GameState.GetState() != GameState::State::CUTSCENE)
                {
                    // show cut scene only for game levels
                    if (static_cast<int>(event.GetScene()) > static_cast<int>(GameState::State::CUTSCENE))
                    {
                        m_GameState.GetScene(GameState::State::CUTSCENE)->ResetTimer();
                        m_GameState.SetState(GameState::State::CUTSCENE);
                        m_GameState.SetNextState(event.GetScene());
                    }
                    else
                    {
                        m_GameState.SetState(event.GetScene());
                    }
                }
                return true;
            }
        );

        appDispatcher.Dispatch<SceneFinishedEvent>([this](SceneFinishedEvent event)
            {
                m_CurrentScene->Stop();
                return true;
            }
        );
    }
}
