// jeecs_imgui_api.hpp

void jegui_init(void* window_handle, bool reboot);
void jegui_update(jegl_thread* thread_context);
void jegui_shutdown(bool reboot);

// ���ڵ���˳�����ʱ�����ջص�����������һ�Ρ�
// �ٴ�����֮ǰ��Ҫ�Ƚ��ע��
// ������woolang�ű���ע���˳��ص���
//
// ����ֵ��ע��Ļص�����ָ��
// ���� true ��ʾ�˳�������ȷ��
// 
bool jegui_shutdown_callback(void);