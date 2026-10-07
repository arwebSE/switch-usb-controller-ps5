/* Host-only regression tests for the PDP-specific virtual-pad lifecycle. */
#define _DEFAULT_SOURCE
#define PDP_ONLY
#define __PROSPERO__
#include "../src/ps5_vpad.c"
#include <assert.h>

enum { NATIVE_USER = 100, PDP_USER = 200, OWN_HANDLE = 0x4030d };
static int32_t users[4] = {NATIVE_USER, -1, -1, -1};
static int native_mode = -1, binds, deletes, disconnects, insertions, bind_result;
static uint32_t inserted_buttons;
static int32_t bound_user;

void log_line(const char *fmt, ...) { (void)fmt; }
void notify_ps5(const char *fmt, ...) { (void)fmt; }
int elevate_privileges(void) { return 1; }
void restore_privileges(void) {}
int shellui_press_ps_button(int32_t handle) { (void)handle; return -1; }
int shellui_remote_bind_device(uint64_t dev, int32_t user)
{ (void)dev; (void)user; assert(!"Remote binding forbidden"); return -1; }
int shellui_remote_disconnect_device(uint64_t dev) { (void)dev; return -1; }
int shellcore_vda(int *code) { (void)code; return -1; }
int32_t sceUserServiceInitialize(void *p) { (void)p; return 0; }
int32_t sceUserServiceGetInitialUser(int32_t *user) { *user = NATIVE_USER; return 0; }
int32_t sceUserServiceGetForegroundUser(int32_t *user) { *user = NATIVE_USER; return 0; }
int32_t sceUserServiceGetLoginUserIdList(int32_t out[4]) { memcpy(out, users, sizeof(users)); return 0; }
int32_t sceUserServiceGetUserStatus(int32_t user, int32_t *status)
{ (void)user; *status = 1; return 0; }
int32_t scePadInit(void) { return 0; }
int32_t scePadGetHandle(int32_t user, int32_t type, int32_t index)
{ (void)user; assert(type == 0); assert(index == 0); return native_mode < 0 ? -1 : 9; }
int32_t scePadReadState(int32_t handle, PadData *data)
{ assert(handle == 9); data->connected = native_mode == 1; return 0; }
int32_t scePadSetProcessPrivilege(int32_t p) { (void)p; return 0; }
int32_t scePadVirtualDeviceAddDevice(void *param, int32_t type)
{ (void)param; (void)type; assert(!"Tests must not create actual devices"); return -1; }
int32_t scePadVirtualDeviceInsertData(int32_t handle, const void *raw)
{
    assert(handle == OWN_HANDLE);
    const PadData *data = raw;
    assert(data->connected && data->orient_w == 1.0f);
    insertions++; inserted_buttons = data->buttons; return 0;
}
int32_t scePadVirtualDeviceDeleteDevice(int32_t handle)
{ assert(handle == OWN_HANDLE); deletes++; return 0; }
uint64_t sceKernelGetProcessTime(void) { return 0; }
static int32_t bind_pad(uint64_t dev, int32_t user)
{
    assert(dev == OWN_HANDLE && user != NATIVE_USER);
    binds++; bound_user = user; return bind_result;
}
static int32_t disconnect_pad(uint64_t dev)
{ assert(dev == OWN_HANDLE); disconnects++; return 0; }

static void pending(int matches)
{
    memset(g_slots, 0, sizeof(g_slots));
    g_slots[0].status = VP_PENDING;
    g_slots[0].handle = -1;
    pad_state_neutral(&g_slots[0].pad_state);
    g_pending = 0; g_matches = matches; g_match_dev = OWN_HANDLE;
    g_t_add = now_ms() - T_IDENTIFY - 1; g_t_match = now_ms() - T_AMBIGUITY - 1;
}

int main(void)
{
    assert(parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d subType=22") == OWN_HANDLE);
    assert(parse_device_id_from_klog("DEVICE_ADDED deviceId=0x4030d subType=0x16") == OWN_HANDLE);
    assert(parse_device_id_from_klog("DEVICE_ADDED dev_id=0x4030d subType=22") == OWN_HANDLE);
    assert(!parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d"));
    assert(!parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d subType=12"));
    assert(!parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d subType=220"));
    assert(!parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d subType=22junk"));
    assert(vpad_native_controller_connected(NATIVE_USER) == -1);
    native_mode = 0; assert(vpad_native_controller_connected(NATIVE_USER) == 0);
    native_mode = 1; assert(vpad_native_controller_connected(NATIVE_USER) == 1);
    native_mode = -1;
    g_ready = 1; g_protected_user = NATIVE_USER; g_pdp_user = PDP_USER;
    g_bind = bind_pad; g_disconnect = disconnect_pad;
    assert(get_user_id_for_slot(0) == -1); /* PDP profile not signed in. */
    pending(1); finish_identification(now_ms());
    assert(g_slots[0].status == VP_READY && g_slots[0].user_id == -1 && !binds);
    users[1] = PDP_USER;
    vpad_poll(now_ms());
    assert(binds == 1 && bound_user == PDP_USER && g_slots[0].user_id == PDP_USER);
    assert(!vpad_rebind_user(0, NATIVE_USER));
    assert(binds == 1);
    assert(vpad_rebind_user(0, PDP_USER));
    bind_result = -1;
    assert(!vpad_rebind_user(0, PDP_USER));
    assert(g_slots[0].user_id == PDP_USER);
    bind_result = 0;
    pad_state_t input; pad_state_neutral(&input); input.buttons = PAD_BTN_CROSS;
    vpad_update(0, &input); vpad_press_ps_button(0); vpad_poll(now_ms());
    assert(inserted_buttons == (PAD_BTN_CROSS | PAD_BTN_PS));
    vpad_poll(now_ms() + 200); assert(inserted_buttons == PAD_BTN_CROSS);
    vpad_remove(0);
    assert(deletes == 1 && disconnects == 1 && g_slots[0].status == VP_FREE);
    pending(1); vpad_remove(0); assert(g_slots[0].remove_when_found);
    finish_identification(now_ms());
    assert(deletes == 2 && g_slots[0].status == VP_FREE);
    int previous_binds = binds;
    pending(2); finish_identification(now_ms());
    assert(g_slots[0].status == VP_FAILED && deletes == 2 && binds == previous_binds);
    assert(insertions > 0);
    puts("PDP lifecycle tests passed");
    return 0;
}
