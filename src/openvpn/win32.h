/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2025 OpenVPN Inc <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef _WIN32
#ifndef OPENVPN_WIN32_H
#define OPENVPN_WIN32_H

#include <iphlpapi.h>
#include <ws2tcpip.h>

#include "syshead.h"
#include "mtu.h"
#include "openvpn-msg.h"
#include "argv.h"
#include "win32-util.h"

/* location of executables */
#define SYS_PATH_ENV_VAR_NAME "SystemRoot"  /* environmental variable name that normally contains the system path */
#define NETSH_PATH_SUFFIX     "\\system32\\netsh.exe"
#define WIN_ROUTE_PATH_SUFFIX "\\system32\\route.exe"
#define WIN_IPCONFIG_PATH_SUFFIX "\\system32\\ipconfig.exe"
#define WIN_NET_PATH_SUFFIX "\\system32\\net.exe"
#define WMIC_PATH_SUFFIX "\\system32\\wbem\\wmic.exe"

/*
 * Win32-specific OpenVPN code, targeted at the mingw
 * development environment.
 */

/* MSVC headers do not define this macro, so do it here */
#ifndef IN6_ARE_ADDR_EQUAL
#define IN6_ARE_ADDR_EQUAL(a, b) \
    (memcmp((const void *)(a), (const void *)(b), sizeof(struct in6_addr)) == 0)
#endif

void init_win32(void);

void uninit_win32(void);

void set_pause_exit_win32(void);

struct security_attributes
{
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
};

#define HANDLE_DEFINED(h) ((h) != NULL && (h) != INVALID_HANDLE_VALUE)

/*
 * Save old window title.
 */
struct window_title
{
    bool saved;
    char old_window_title[256];
};

struct rw_handle {
    HANDLE read;
    HANDLE write;
};

/*
 * Event-based notification of incoming TCP connections
 */

#define NE32_PERSIST_EVENT (1<<0)
#define NE32_WRITE_EVENT   (1<<1)

static inline bool
defined_net_event_win32(const struct rw_handle *event)
{
    return event->read != NULL;
}

void init_net_event_win32(struct rw_handle *event, long network_events, socket_descriptor_t sd, unsigned int flags);

long reset_net_event_win32(struct rw_handle *event, socket_descriptor_t sd);

void close_net_event_win32(struct rw_handle *event, socket_descriptor_t sd, unsigned int flags);

/*
 * A stateful variant of the net_event_win32 functions above
 */

struct net_event_win32
{
    struct rw_handle handle;
    socket_descriptor_t sd;
    long event_mask;
};

void net_event_win32_init(struct net_event_win32 *ne);

void net_event_win32_start(struct net_event_win32 *ne, long network_events, socket_descriptor_t sd);

void net_event_win32_reset(struct net_event_win32 *ne);

void net_event_win32_reset_write(struct net_event_win32 *ne);

void net_event_win32_stop(struct net_event_win32 *ne);

void net_event_win32_close(struct net_event_win32 *ne);

static inline bool
net_event_win32_defined(const struct net_event_win32 *ne)
{
    return defined_net_event_win32(&ne->handle);
}

static inline struct rw_handle *
net_event_win32_get_event(struct net_event_win32 *ne)
{
    return &ne->handle;
}

static inline long
net_event_win32_get_event_mask(const struct net_event_win32 *ne)
{
    return ne->event_mask;
}

static inline void
net_event_win32_clear_selected_events(struct net_event_win32 *ne, long selected_events)
{
    ne->event_mask &= ~selected_events;
}

/*
 * Signal handling
 */
struct win32_signal {
#define WSO_MODE_UNDEF   0
#define WSO_MODE_SERVICE 1
#define WSO_MODE_CONSOLE 2
    int mode;
    struct rw_handle in;
    DWORD console_mode_save;
    bool console_mode_save_defined;
};

extern struct win32_signal win32_signal; /* static/global */
extern struct window_title window_title; /* static/global */

void win32_signal_clear(struct win32_signal *ws);

/* win32_signal_open startup type */
#define WSO_NOFORCE       0
#define WSO_FORCE_SERVICE 1
#define WSO_FORCE_CONSOLE 2

void win32_signal_open(struct win32_signal *ws,
                       int force,  /* set to WSO force parm */
                       const char *exit_event_name,
                       bool exit_event_initial_state);

void win32_signal_close(struct win32_signal *ws);

int win32_signal_get(struct win32_signal *ws);

void win32_pause(struct win32_signal *ws);

bool win32_service_interrupt(struct win32_signal *ws);

/*
 * Set the text on the window title bar
 */

void window_title_clear(struct window_title *wt);

void window_title_save(struct window_title *wt);

void window_title_restore(const struct window_title *wt);

void window_title_generate(const char *title);

/*
 * We try to do all Win32 I/O using overlapped
 * (i.e. asynchronous) I/O for a performance win.
 */
struct overlapped_io {
#define IOSTATE_INITIAL          0
#define IOSTATE_QUEUED           1  /* overlapped I/O has been queued */
#define IOSTATE_IMMEDIATE_RETURN 2  /* I/O function returned immediately without queueing */
    int iostate;
    OVERLAPPED overlapped;
    DWORD size;
    DWORD flags;
    int status;
    bool addr_defined;
    union {
        struct sockaddr_in addr;
        struct sockaddr_in6 addr6;
    };
    int addrlen;
    struct buffer buf_init;
    struct buffer buf;
};

void overlapped_io_init(struct overlapped_io *o,
                        const struct frame *frame,
                        BOOL event_state);

void overlapped_io_close(struct overlapped_io *o);

static inline bool
overlapped_io_active(struct overlapped_io *o)
{
    return o->iostate == IOSTATE_QUEUED || o->iostate == IOSTATE_IMMEDIATE_RETURN;
}

char *overlapped_io_state_ascii(const struct overlapped_io *o);

/*
 * Use to control access to resources that only one
 * OpenVPN process on a given machine can access at
 * a given time.
 */

struct semaphore
{
    const char *name;
    bool locked;
    HANDLE hand;
};

void semaphore_clear(struct semaphore *s);

void semaphore_open(struct semaphore *s, const char *name);

bool semaphore_lock(struct semaphore *s, int timeout_milliseconds);

void semaphore_release(struct semaphore *s);

void semaphore_close(struct semaphore *s);

/*
 * Special global semaphore used to protect network
 * shell commands from simultaneous instantiation.
 *
 * It seems you can't run more than one instance
 * of netsh on the same machine at the same time.
 */

extern struct semaphore netcmd_semaphore;
void netcmd_semaphore_init(void);

void netcmd_semaphore_close(void);

void netcmd_semaphore_lock(void);

void netcmd_semaphore_release(void);

/* Set Win32 security attributes structure to allow all access */
bool init_security_attributes_allow_all(struct security_attributes *obj);

/* add constant environmental variables needed by Windows */
struct env_set;

/* get and set the current windows system path */
void set_win_sys_path(const char *newpath, struct env_set *es);

void set_win_sys_path_via_env(struct env_set *es);

char *get_win_sys_path(void);

/* call self in a subprocess */
void fork_to_self(const char *cmdline);

bool win_wfp_block(const NET_IFINDEX index, const HANDLE msg_channel, BOOL dns_only);

bool win_wfp_uninit(const NET_IFINDEX index, const HANDLE msg_channel);

/**
 * @brief Get Windows version string with architecture info.
 *
 * @param gc gc arena to allocate string.
 * @return Version string, or "N/A" on failure.
 */
const char *win32_version_string(struct gc_arena *gc);

/*
 * Send the |size| bytes in buffer |data| to the interactive service |pipe|
 * and read the result in |ack|. Returns false on communication error.
 * The string in |context| is used to prefix error messages.
 */
bool send_msg_iservice(HANDLE pipe, const void *data, size_t size,
                       ack_message_t *ack, const char *context);

/*
 * Attempt to simulate fork/execve on Windows
 */
int
openvpn_execve(const struct argv *a, const struct env_set *es, const unsigned int flags);

/* Sleep that can be interrupted by signals and exit event */
void win32_sleep(const int n);

/**
 * @brief Fetches a registry value for OpenVPN registry key.
 *
 * @param key Registry value name to fetch.
 * @param value Buffer to store the fetched string value.
 * @param size Size of `value` buffer in bytes.
 * @return `true` if successful, `false` otherwise.
 */
bool
get_openvpn_reg_value(const WCHAR *key, WCHAR *value, DWORD size);

/**
 * @brief Checks if a plugin is located in a trusted directory.
 *
 * Verifies the plugin's path against a trusted directory, which is:
 *
 * - "plugin_dir" registry value or installation path, if the registry key is missing
 * - system directory
 *
 * UNC paths are explicitly disallowed.
 *
 * @param plugin_path Normalized path to the plugin.
 * @return \c true if the plugin is in a trusted directory and not a UNC path; \c false otherwise.
 */
bool
plugin_in_trusted_dir(const WCHAR *plugin_path);

/**
 * Encrypt a region of memory using CryptProtectMemory()
 * with access restricted to the current process.
 *
 * - buf   pointer to the memory
 * - len   number of bytes to encrypt -- must be a multiple of
 *         CRYPTPROTECTMEMORY_BLOCK_SIZE = 16
 */
bool
protect_buffer_win32(char *buf, size_t len);

/**
 * Decrypt a previously encrypted region of memory using CryptUnProtectMemory()
 * with access restricted to the current process.
 *
 * - buf   pointer to the memory
 * - len   number of bytes to encrypt -- must be a multiple of
 *         CRYPTPROTECTMEMORY_BLOCK_SIZE = 16
 */
bool
unprotect_buffer_win32(char *buf, size_t len);

#endif /* ifndef OPENVPN_WIN32_H */
#endif /* ifdef _WIN32 */
