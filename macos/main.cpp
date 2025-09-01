/*
 * JoyECS
 * -----------------------------------
 * JoyECS is a interesting ecs-impl.
 *
 */
#include "jeecs.hpp"

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

class je_macos_context : public jeecs::game_engine_context
{
    class renderer
    {
    public:
        renderer(MTL::Device* pDevice)
            : _pDevice(pDevice->retain())
        {
            _pCommandQueue = _pDevice->newCommandQueue();
        }
        ~renderer()
        {
            _pCommandQueue->release();
            _pDevice->release();
        }
        void draw(MTK::View* pView)
        {
            NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

            MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
            MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
            MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
            pEnc->endEncoding();
            pCmd->presentDrawable(pView->currentDrawable());
            pCmd->commit();

            pPool->release();
        }

    private:
        MTL::Device* _pDevice;
        MTL::CommandQueue* _pCommandQueue;
    };
    class mtk_view_delegate : public MTK::ViewDelegate
    {
        renderer* _pRenderer;
    public:
        MyMTKViewDelegate(MTL::Device* pDevice)
            : MTK::ViewDelegate()
            , _pRenderer(new renderer(pDevice))
        {

        }
        virtual ~MyMTKViewDelegate() override
        {
            delete _pRenderer;
        }
        virtual void drawInMTKView(MTK::View* pView) override
        {
            _pRenderer->draw(pView);
        }
    }
    class app_delegate : public NS::ApplicationDelegate
    {
        NS::Window* _pWindow;
        MTK::View* _pMtkView;
        MTL::Device* _pDevice;
        mtk_view_delegate* _pViewDelegate;
    public:
        app_delegate()
            : _pWindow(nullptr)
            , _pMtkView(nullptr)
            , _pDevice(nullptr)
            , _pViewDelegate(nullptr)
        {
        }
        ~app_delegate()
        {
            _pMtkView->release();
            _pWindow->release();
            _pDevice->release();
            delete _pViewDelegate;
        }

        ////////////////////////////////////////////////////////////////////////////////////////

        NS::Menu* createMenuBar()
        {
            using NS::StringEncoding::UTF8StringEncoding;

            NS::Menu* pMainMenu = NS::Menu::alloc()->init();
            NS::MenuItem* pAppMenuItem = NS::MenuItem::alloc()->init();
            NS::Menu* pAppMenu = NS::Menu::alloc()->init(NS::String::string("Appname", UTF8StringEncoding));

            NS::String* appName = NS::RunningApplication::currentApplication()->localizedName();
            NS::String* quitItemName = NS::String::string("Quit ", UTF8StringEncoding)->stringByAppendingString(appName);
            SEL quitCb = NS::MenuItem::registerActionCallback("appQuit", [](void*, SEL, const NS::Object* pSender) {
                auto pApp = NS::Application::sharedApplication();
                pApp->terminate(pSender);
                });

            NS::MenuItem* pAppQuitItem = pAppMenu->addItem(quitItemName, quitCb, NS::String::string("q", UTF8StringEncoding));
            pAppQuitItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);
            pAppMenuItem->setSubmenu(pAppMenu);

            NS::MenuItem* pWindowMenuItem = NS::MenuItem::alloc()->init();
            NS::Menu* pWindowMenu = NS::Menu::alloc()->init(NS::String::string("Window", UTF8StringEncoding));

            SEL closeWindowCb = NS::MenuItem::registerActionCallback("windowClose", [](void*, SEL, const NS::Object*) {
                auto pApp = NS::Application::sharedApplication();
                pApp->windows()->object< NS::Window >(0)->close();
                });
            NS::MenuItem* pCloseWindowItem = pWindowMenu->addItem(NS::String::string("Close Window", UTF8StringEncoding), closeWindowCb, NS::String::string("w", UTF8StringEncoding));
            pCloseWindowItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);

            pWindowMenuItem->setSubmenu(pWindowMenu);

            pMainMenu->addItem(pAppMenuItem);
            pMainMenu->addItem(pWindowMenuItem);

            pAppMenuItem->release();
            pWindowMenuItem->release();
            pAppMenu->release();
            pWindowMenu->release();

            return pMainMenu->autorelease();
        }
        virtual void applicationWillFinishLaunching(NS::Notification* pNotification) override
        {
            NS::Menu* pMenu = createMenuBar();
            NS::Application* pApp = reinterpret_cast<NS::Application*>(pNotification->object());
            pApp->setMainMenu(pMenu);
            pApp->setActivationPolicy(NS::ActivationPolicy::ActivationPolicyRegular);
        }
        virtual void applicationDidFinishLaunching(NS::Notification* pNotification) override
        {
            CGRect frame = (CGRect){ {100.0, 100.0}, {512.0, 512.0} };

            _pWindow = NS::Window::alloc()->init(
                frame,
                NS::WindowStyleMaskClosable | NS::WindowStyleMaskTitled,
                NS::BackingStoreBuffered,
                false);

            _pDevice = MTL::CreateSystemDefaultDevice();

            _pMtkView = MTK::View::alloc()->init(frame, _pDevice);
            _pMtkView->setColorPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
            _pMtkView->setClearColor(MTL::ClearColor::Make(1.0, 0.0, 0.0, 1.0));

            _pViewDelegate = new mtk_view_delegate(_pDevice);
            _pMtkView->setDelegate(_pViewDelegate);

            _pWindow->setContentView(_pMtkView);
            _pWindow->setTitle(NS::String::string("00 - Window", NS::StringEncoding::UTF8StringEncoding));

            _pWindow->makeKeyAndOrderFront(nullptr);

            NS::Application* pApp = reinterpret_cast<NS::Application*>(pNotification->object());
            pApp->activateIgnoringOtherApps(true);
        }
        virtual bool applicationShouldTerminateAfterLastWindowClosed(NS::Application* pSender) override
        {
            return true;
        }
    }
    app_delegate delegate_instance;
    NS::AutoreleasePool* m_auto_release_pool;

    JECS_DISABLE_MOVE_AND_COPY(je_macos_context);
private:

public:
    je_macos_context(int argc, char** argv)
        : jeecs::game_engine_context(argc, argv)
    {
        m_auto_release_pool = NS::AutoreleasePool::alloc()->init();
    }
    ~je_macos_context()
    {
        m_auto_release_pool->release();
    }

    void loop()
    {
        NS::Application* shared_app = NS::Application::sharedApplication();

        shared_app->setDelegate(&delegate_instance);
        shared_app->run();
    }
};

int main(int argc, char** argv)
{
    je_macos_context context(argc, argv);
    context.loop();
}