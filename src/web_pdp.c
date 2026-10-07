#define _DEFAULT_SOURCE
#include "web_pdp.h"
#include "ps5_vpad.h"
#include "usb_hotplug.h"
#include "log.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __PROSPERO__
#include <ps5/kernel.h>
extern int32_t sceUserServiceGetLoginUserIdList(int32_t users[4]);
extern int32_t sceUserServiceGetForegroundUser(int32_t *user);
extern int32_t sceUserServiceGetUserName(int32_t user, char *name, size_t size);
#endif

static int server = -1;
static pthread_t thread;
static atomic_int running;

static const char PAGE[] =
"<!doctype html><html lang='en'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>PDP Pad</title><style>body{margin:0;background:#111722;color:#e8eef7;font:16px system-ui}main{max-width:760px;margin:auto;padding:28px 18px}"
"h1{margin:0;font-size:30px}p{line-height:1.5;color:#b4c2d6}.card{background:#1b2535;border:1px solid #34445b;border-radius:14px;padding:20px;margin-top:18px}"
".row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}button,select{font:inherit;border:1px solid #58718e;border-radius:8px;padding:10px;background:#243951;color:#fff;cursor:pointer}"
"button:disabled{opacity:.45;cursor:default}.danger{background:#5a2934}pre{white-space:pre-wrap;overflow-wrap:anywhere;max-height:300px;overflow:auto;font-size:12px}"
".ok{color:#7ae5b2}.muted{color:#b4c2d6}label{display:block;margin:12px 0 8px}</style><main><h1>PDP Pad</h1><p>Wired controller manager <span id='fw'></span></p>"
"<div class='card'><h2 id='status'>Connecting...</h2><p id='profile'></p><p id='input' class='muted'></p><div class='row'>"
"<button id='home' onclick='act(\"home\")'>Press Home / PS</button><button onclick='act(\"release\")'>Release virtual pad</button>"
"<button id='reconnect' onclick='act(\"reconnect\")'>Reconnect</button></div><label for='users'>Assign PDP controller to profile</label>"
"<div class='row'><select id='users'></select><button id='bind' onclick='act(\"rebind?user=\"+document.getElementById(\"users\").value)'>Assign profile</button></div></div>"
"<div class='card'><h2>Native controllers</h2><p id='native'>Checking...</p><p>Release the virtual pad to check native controller behavior. If a DualSense cannot pair, connect it by USB and press PS.</p></div>"
"<div class='card'><div class='row'><button onclick='logs()'>Refresh log</button><button class='danger' onclick='act(\"stop\")'>Stop payload</button></div><p id='message' role='status'></p><pre id='log'></pre></div></main>"
"<script>let stopped=false;const el=id=>document.getElementById(id);async function refresh(){if(stopped)return;try{let r=await fetch('/api/state',{cache:'no-store'}),s=await r.json();"
"el('fw').textContent='· FW '+s.firmware;el('status').textContent=s.usb_connected?(s.live?'Controller active':'USB connected · virtual pad released or unavailable'):'Controller unplugged';"
"el('status').className=s.live?'ok':'';let u=s.users.find(u=>u.id===s.user);el('profile').textContent=s.live?(s.user==='ffffffff'?'Unassigned: press Home and choose the separate PDP profile.':'Assigned to '+(u?u.name:s.user)):'Select Reconnect to create the virtual pad.';"
"el('input').textContent='L stick '+s.axes[0]+', '+s.axes[1]+' · R stick '+s.axes[2]+', '+s.axes[3]+' · Buttons 0x'+s.buttons.toString(16);"
"let selected=el('users').value;el('users').replaceChildren();for(let u of s.users){let o=document.createElement('option');o.value=u.id;o.textContent=u.name+(u.foreground?' (active)':'')+(u.protected?' (reserved for DualSense)':'');o.disabled=u.protected;el('users').append(o);}"
"if(s.users.some(u=>u.id===selected&&!u.protected))el('users').value=selected;else if(s.users.some(u=>u.id===s.user&&!u.protected))el('users').value=s.user;el('home').disabled=!s.live;let target=s.users.find(u=>u.id===el('users').value);el('bind').disabled=!s.live||!target||target.protected||target.native_connected;el('reconnect').disabled=!s.usb_connected||s.status===1||s.status===2||s.live;"
"let native=s.users.filter(u=>u.native_connected===true),unknown=s.users.some(u=>u.native_connected===null);el('native').textContent=(native.length?'Connected: '+native.map(u=>u.name).join(', '):unknown?'Native connection status unavailable from this payload process.':'No connected native controller reported.')+' Protected profile: '+(s.users.find(u=>u.protected)?.name||'not configured');}"
"catch(e){el('status').textContent='Dashboard offline';}}async function act(path){try{let r=await fetch('/api/'+path,{method:'POST',headers:{'X-PDP-Control':'1'}});let j=await r.json();el('message').textContent=j.message;"
"if(path==='stop'&&r.ok){stopped=true;el('status').textContent='Payload stopped';return;}await refresh();}catch(e){el('message').textContent='Request failed: '+e.message;}}"
"async function logs(){try{el('log').textContent=await(await fetch('/api/log',{cache:'no-store'})).text();}catch(e){el('message').textContent=e.message;}}refresh();setInterval(refresh,1000);</script></html>";

static void reply(int fd, int status, const char *type, const char *body)
{
    char header[256];
    const char *reason = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 409 ? "Conflict" : "Bad Request";
    size_t length = strlen(body);
    int n = snprintf(header, sizeof(header), "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", status, reason, type, length);
    const char *parts[] = {header, body};
    size_t sizes[] = {(size_t)n, length};
    for (int i = 0; i < 2; i++) {
        size_t sent = 0;
        while (sent < sizes[i]) {
            ssize_t count = send(fd, parts[i] + sent, sizes[i] - sent, 0);
            if (count <= 0) return;
            sent += (size_t)count;
        }
    }
}

static void escape_json(char *out, size_t size, const char *in)
{
    size_t pos = 0;
    for (; *in && pos + 3 < size; in++) {
        unsigned char c = (unsigned char)*in;
        if (c < 32) c = ' ';
        if (c == '"' || c == '\\') out[pos++] = '\\';
        out[pos++] = (char)c;
    }
    out[pos] = 0;
}

static void state(int fd)
{
    char body[4096], users[2048] = "", name[64], escaped[128];
    size_t used = 0;
    int32_t login[4] = {-1, -1, -1, -1}, foreground = -1;
    uint32_t firmware = 0;
#ifdef __PROSPERO__
    firmware = kernel_get_fw_version();
    sceUserServiceGetLoginUserIdList(login);
    sceUserServiceGetForegroundUser(&foreground);
#endif
    for (int i = 0; i < 4; i++) {
        if (login[i] <= 0) continue;
        snprintf(name, sizeof(name), "Profile %d", i + 1);
        int native_connected = -1;
#ifdef __PROSPERO__
        sceUserServiceGetUserName(login[i], name, sizeof(name));
        name[sizeof(name) - 1] = 0;
        native_connected = vpad_native_controller_connected(login[i]);
#endif
        escape_json(escaped, sizeof(escaped), name);
        int n = snprintf(users + used, sizeof(users) - used,
            "%s{\"id\":\"%08x\",\"name\":\"%s\",\"foreground\":%s,\"native_connected\":%s,\"protected\":%s}",
            used ? "," : "", (unsigned)login[i], escaped, login[i] == foreground ? "true" : "false",
            native_connected < 0 ? "null" : native_connected ? "true" : "false", login[i] == vpad_get_protected_user() ? "true" : "false");
        if (n < 0 || (size_t)n >= sizeof(users) - used) break;
        used += (size_t)n;
    }
    vpad_slot_info_t pad = {0};
    vpad_get_slot_info(0, &pad);
    snprintf(body, sizeof(body),
        "{\"firmware\":\"%x.%02x\",\"usb_connected\":%s,\"live\":%s,\"status\":%d,\"user\":\"%08x\",\"buttons\":%u,\"axes\":[%u,%u,%u,%u],\"frames\":%u,\"users\":[%s]}",
        (firmware >> 24) & 255, (firmware >> 16) & 255,
        usb_hotplug_pdp_connected() ? "true" : "false", pad.status == VP_READY ? "true" : "false", pad.status,
        (unsigned)pad.user_id, pad.buttons, pad.lx, pad.ly, pad.rx, pad.ry, pad.packets_injected, users);
    reply(fd, 200, "application/json", body);
}

static int control_header(const char *request)
{
    const char *line = strstr(request, "\r\n");
    while (line && line[2]) {
        line += 2;
        if (strncasecmp(line, "X-PDP-Control:", 14) == 0) {
            const char *value = line + 14;
            while (*value == ' ' || *value == '\t') value++;
            return value[0] == '1' && value[1] == '\r' && value[2] == '\n';
        }
        line = strstr(line, "\r\n");
    }
    return 0;
}

static void handle(int fd)
{
    char request[2048] = {0}, method[8], path[128];
    size_t used = 0;
    /* A client sending one byte just before each socket timeout must not
     * keep this single-worker server occupied indefinitely. */
    struct timeval started, current;
    gettimeofday(&started, NULL);
    while (!strstr(request, "\r\n\r\n")) {
        gettimeofday(&current, NULL);
        if (current.tv_sec - started.tv_sec >= 2) return;
        if (used == sizeof(request) - 1) return;
        ssize_t count = recv(fd, request + used, sizeof(request) - 1 - used, 0);
        if (count <= 0) return;
        used += (size_t)count;
        request[used] = 0;
    }
    if (sscanf(request, "%7s %127s", method, path) != 2) return;
    if (!strcmp(method, "GET") && !strcmp(path, "/")) {
        reply(fd, 200, "text/html; charset=utf-8", PAGE);
    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/state")) {
        state(fd);
    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/log")) {
        char log[12001] = {0};
        FILE *file = fopen("/data/pdp-pad/pdp-pad.log", "r");
        if (file) {
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, size > 12000 ? size - 12000 : 0, SEEK_SET);
            fread(log, 1, sizeof(log) - 1, file);
            fclose(file);
        }
        reply(fd, 200, "text/plain; charset=utf-8", log);
    } else if (!strcmp(method, "POST")) {
        if (!control_header(request)) {
            reply(fd, 403, "application/json", "{\"message\":\"Missing control header\"}");
            return;
        }
        int ok = 1;
        const char *message = "Action completed";
        if (!strcmp(path, "/api/stop")) {
            reply(fd, 200, "application/json", "{\"message\":\"Stopping payload\"}");
            pdp_request_stop();
            return;
        } else if (!strcmp(path, "/api/release")) {
            int pause = open("/data/pdp-pad/pause", O_WRONLY | O_CREAT, 0600);
            ok = pause >= 0;
            if (ok) {
                close(pause);
                vpad_remove(0);
            }
            message = ok ? "Virtual pad released or release pending. Paused until Reconnect." : "Could not persist paused state; no action taken";
        } else if (!strcmp(path, "/api/reconnect")) {
            vpad_slot_info_t pad = {0};
            vpad_get_slot_info(0, &pad);
            if (usb_hotplug_pdp_connected() && (pad.status == VP_FREE || pad.status == VP_FAILED)) {
                /* A failed insertion can still own a device. Release it
                 * before queuing a replacement rather than leaking it. */
                if (pad.status == VP_FAILED) vpad_remove(0);
                ok = unlink("/data/pdp-pad/pause") == 0 || errno == ENOENT;
                if (ok) ok = vpad_add(0, CONN_USB_WIRED, "PDP Faceoff Deluxe+ Wired");
            } else ok = 0;
            message = ok ? "Creating virtual pad" : "Controller unavailable or already active";
        } else if (!strcmp(path, "/api/home")) {
            ok = vpad_is_live(0);
            if (ok) vpad_press_ps_button(0);
            message = ok ? "Home / PS sent" : "Virtual pad is not active";
        } else if (!strncmp(path, "/api/rebind?user=", 17)) {
            char *end = NULL;
            errno = 0;
            unsigned long user = strtoul(path + 17, &end, 16);
            ok = !errno && end != path + 17 && !*end && user > 0 && user <= 0x7fffffffUL;
            if (ok) ok = vpad_rebind_user(0, (int32_t)user);
            message = ok ? "Profile assigned" : "Could not bind to that signed-in profile";
        } else {
            reply(fd, 400, "application/json", "{\"message\":\"Unknown action\"}");
            return;
        }
        char body[256];
        snprintf(body, sizeof(body), "{\"message\":\"%s\"}", message);
        reply(fd, ok ? 200 : 409, "application/json", body);
    } else reply(fd, 400, "application/json", "{\"message\":\"Unknown request\"}");
}

static void *worker(void *arg)
{
    (void)arg;
    while (running) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(server, &reads);
        struct timeval wait = {0, 100000};
        if (select(server + 1, &reads, NULL, NULL, &wait) <= 0) continue;
        int fd = accept(server, NULL, NULL);
        if (fd < 0) continue;
        struct timeval timeout = {0, 500000};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        handle(fd);
        close(fd);
    }
    return NULL;
}

int pdp_web_init(int port)
{
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) return 0;
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) != 0 || listen(server, 4) != 0) {
        close(server);
        server = -1;
        return 0;
    }
    running = 1;
    if (pthread_create(&thread, NULL, worker, NULL) != 0) {
        running = 0;
        close(server);
        server = -1;
        return 0;
    }
    log_line("PDP dashboard listening on port %d", port);
    return 1;
}

void pdp_web_cleanup(void)
{
    if (server < 0) return;
    running = 0;
    pthread_join(thread, NULL);
    close(server);
    server = -1;
}
