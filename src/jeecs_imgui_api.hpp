// jeecs_imgui_api.hpp

void jegui_init(void* window_handle, bool reboot);
void jegui_update(jegl_thread* thread_context);
void jegui_shutdown(bool reboot);

// 用于点击退出窗口时的最终回调，仅允许解除一次。
// 再次设置之前需要先解除注册
// 允许在woolang脚本中注册退出回调。
//
// 返回值由注册的回调函数指定
// 返回 true 表示退出操作被确认
// 
bool jegui_shutdown_callback(void);