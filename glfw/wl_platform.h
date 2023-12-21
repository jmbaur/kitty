//========================================================================
// GLFW 3.4 Wayland - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2014 Jonas Ådahl <jadahl@gmail.com>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include <wayland-client.h>
#include <dlfcn.h>
#include <poll.h>
#include <stdbool.h>

typedef VkFlags VkWaylandSurfaceCreateFlagsKHR;

typedef struct VkWaylandSurfaceCreateInfoKHR
{
    VkStructureType                 sType;
    const void*                     pNext;
    VkWaylandSurfaceCreateFlagsKHR  flags;
    struct wl_display*              display;
    struct wl_surface*              surface;
} VkWaylandSurfaceCreateInfoKHR;

typedef VkResult (APIENTRY *PFN_vkCreateWaylandSurfaceKHR)(VkInstance,const VkWaylandSurfaceCreateInfoKHR*,const VkAllocationCallbacks*,VkSurfaceKHR*);
typedef VkBool32 (APIENTRY *PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)(VkPhysicalDevice,uint32_t,struct wl_display*);

#include "posix_thread.h"
#ifdef __linux__
#include "linux_joystick.h"
#else
#include "null_joystick.h"
#endif
#include "backend_utils.h"
#include "xkb_glfw.h"
#include "wl_cursors.h"

#include "wayland-xdg-shell-client-protocol.h"
#include "wayland-xdg-decoration-unstable-v1-client-protocol.h"
#include "wayland-relative-pointer-unstable-v1-client-protocol.h"
#include "wayland-pointer-constraints-unstable-v1-client-protocol.h"
#include "wayland-primary-selection-unstable-v1-client-protocol.h"
#include "wayland-primary-selection-unstable-v1-client-protocol.h"
#include "wl_text_input.h"
#include "wayland-xdg-activation-v1-client-protocol.h"

#define _glfw_dlopen(name) dlopen(name, RTLD_LAZY | RTLD_LOCAL)
#define _glfw_dlclose(handle) dlclose(handle)
#define _glfw_dlsym(handle, name) dlsym(handle, name)

#define _GLFW_PLATFORM_WINDOW_STATE         _GLFWwindowWayland  wl
#define _GLFW_PLATFORM_LIBRARY_WINDOW_STATE _GLFWlibraryWayland wl
#define _GLFW_PLATFORM_MONITOR_STATE        _GLFWmonitorWayland wl
#define _GLFW_PLATFORM_CURSOR_STATE         _GLFWcursorWayland  wl

#define _GLFW_PLATFORM_CONTEXT_STATE
#define _GLFW_PLATFORM_LIBRARY_CONTEXT_STATE

typedef struct wl_cursor_theme* (* PFN_wl_cursor_theme_load)(const char*, int, struct wl_shm*);
typedef void (* PFN_wl_cursor_theme_destroy)(struct wl_cursor_theme*);
typedef struct wl_cursor* (* PFN_wl_cursor_theme_get_cursor)(struct wl_cursor_theme*, const char*);
typedef struct wl_buffer* (* PFN_wl_cursor_image_get_buffer)(struct wl_cursor_image*);
#define wl_cursor_theme_load _glfw.wl.cursor.theme_load
#define wl_cursor_theme_destroy _glfw.wl.cursor.theme_destroy
#define wl_cursor_theme_get_cursor _glfw.wl.cursor.theme_get_cursor
#define wl_cursor_image_get_buffer _glfw.wl.cursor.image_get_buffer

typedef struct wl_egl_window* (* PFN_wl_egl_window_create)(struct wl_surface*, int, int);
typedef void (* PFN_wl_egl_window_destroy)(struct wl_egl_window*);
typedef void (* PFN_wl_egl_window_resize)(struct wl_egl_window*, int, int, int, int);
#define wl_egl_window_create _glfw.wl.egl.window_create
#define wl_egl_window_destroy _glfw.wl.egl.window_destroy
#define wl_egl_window_resize _glfw.wl.egl.window_resize


struct libdecor;
struct libdecor_frame;
struct libdecor_state;
struct libdecor_configuration;

enum libdecor_error
{
	LIBDECOR_ERROR_COMPOSITOR_INCOMPATIBLE,
	LIBDECOR_ERROR_INVALID_FRAME_CONFIGURATION,
};

enum libdecor_window_state
{
	LIBDECOR_WINDOW_STATE_NONE = 0,
	LIBDECOR_WINDOW_STATE_ACTIVE = 1,
	LIBDECOR_WINDOW_STATE_MAXIMIZED = 2,
	LIBDECOR_WINDOW_STATE_FULLSCREEN = 4,
	LIBDECOR_WINDOW_STATE_TILED_LEFT = 8,
	LIBDECOR_WINDOW_STATE_TILED_RIGHT = 16,
	LIBDECOR_WINDOW_STATE_TILED_TOP = 32,
	LIBDECOR_WINDOW_STATE_TILED_BOTTOM = 64
};

enum libdecor_capabilities
{
	LIBDECOR_ACTION_MOVE = 1,
	LIBDECOR_ACTION_RESIZE = 2,
	LIBDECOR_ACTION_MINIMIZE = 4,
	LIBDECOR_ACTION_FULLSCREEN = 8,
	LIBDECOR_ACTION_CLOSE = 16
};

struct libdecor_interface
{
	void (* error)(struct libdecor*,enum libdecor_error,const char*);
	void (* reserved0)(void);
	void (* reserved1)(void);
	void (* reserved2)(void);
	void (* reserved3)(void);
	void (* reserved4)(void);
	void (* reserved5)(void);
	void (* reserved6)(void);
	void (* reserved7)(void);
	void (* reserved8)(void);
	void (* reserved9)(void);
};

struct libdecor_frame_interface
{
	void (* configure)(struct libdecor_frame*,struct libdecor_configuration*,void*);
	void (* close)(struct libdecor_frame*,void*);
	void (* commit)(struct libdecor_frame*,void*);
	void (* dismiss_popup)(struct libdecor_frame*,const char*,void*);
	void (* reserved0)(void);
	void (* reserved1)(void);
	void (* reserved2)(void);
	void (* reserved3)(void);
	void (* reserved4)(void);
	void (* reserved5)(void);
	void (* reserved6)(void);
	void (* reserved7)(void);
	void (* reserved8)(void);
	void (* reserved9)(void);
};

typedef struct libdecor* (* PFN_libdecor_new)(struct wl_display*,const struct libdecor_interface*);
typedef void (* PFN_libdecor_unref)(struct libdecor*);
typedef int (* PFN_libdecor_get_fd)(struct libdecor*);
typedef int (* PFN_libdecor_dispatch)(struct libdecor*,int);
typedef struct libdecor_frame* (* PFN_libdecor_decorate)(struct libdecor*,struct wl_surface*,const struct libdecor_frame_interface*,void*);
typedef void (* PFN_libdecor_frame_unref)(struct libdecor_frame*);
typedef void (* PFN_libdecor_frame_set_app_id)(struct libdecor_frame*,const char*);
typedef void (* PFN_libdecor_frame_set_title)(struct libdecor_frame*,const char*);
typedef void (* PFN_libdecor_frame_set_minimized)(struct libdecor_frame*);
typedef void (* PFN_libdecor_frame_set_fullscreen)(struct libdecor_frame*,struct wl_output*);
typedef void (* PFN_libdecor_frame_unset_fullscreen)(struct libdecor_frame*);
typedef void (* PFN_libdecor_frame_map)(struct libdecor_frame*);
typedef void (* PFN_libdecor_frame_commit)(struct libdecor_frame*,struct libdecor_state*,struct libdecor_configuration*);
typedef void (* PFN_libdecor_frame_set_min_content_size)(struct libdecor_frame*,int,int);
typedef void (* PFN_libdecor_frame_set_max_content_size)(struct libdecor_frame*,int,int);
typedef void (* PFN_libdecor_frame_set_maximized)(struct libdecor_frame*);
typedef void (* PFN_libdecor_frame_unset_maximized)(struct libdecor_frame*);
typedef void (* PFN_libdecor_frame_set_capabilities)(struct libdecor_frame*,enum libdecor_capabilities);
typedef void (* PFN_libdecor_frame_unset_capabilities)(struct libdecor_frame*,enum libdecor_capabilities);
typedef void (* PFN_libdecor_frame_set_visibility)(struct libdecor_frame*,bool visible);
typedef struct xdg_toplevel* (* PFN_libdecor_frame_get_xdg_toplevel)(struct libdecor_frame*);
typedef bool (* PFN_libdecor_configuration_get_content_size)(struct libdecor_configuration*,struct libdecor_frame*,int*,int*);
typedef bool (* PFN_libdecor_configuration_get_window_state)(struct libdecor_configuration*,enum libdecor_window_state*);
typedef struct libdecor_state* (* PFN_libdecor_state_new)(int,int);
typedef void (* PFN_libdecor_state_free)(struct libdecor_state*);
#define libdecor_new _glfw.wl.libdecor.libdecor_new_
#define libdecor_unref _glfw.wl.libdecor.libdecor_unref_
#define libdecor_get_fd _glfw.wl.libdecor.libdecor_get_fd_
#define libdecor_dispatch _glfw.wl.libdecor.libdecor_dispatch_
#define libdecor_decorate _glfw.wl.libdecor.libdecor_decorate_
#define libdecor_frame_unref _glfw.wl.libdecor.libdecor_frame_unref_
#define libdecor_frame_set_app_id _glfw.wl.libdecor.libdecor_frame_set_app_id_
#define libdecor_frame_set_title _glfw.wl.libdecor.libdecor_frame_set_title_
#define libdecor_frame_set_minimized _glfw.wl.libdecor.libdecor_frame_set_minimized_
#define libdecor_frame_set_fullscreen _glfw.wl.libdecor.libdecor_frame_set_fullscreen_
#define libdecor_frame_unset_fullscreen _glfw.wl.libdecor.libdecor_frame_unset_fullscreen_
#define libdecor_frame_map _glfw.wl.libdecor.libdecor_frame_map_
#define libdecor_frame_commit _glfw.wl.libdecor.libdecor_frame_commit_
#define libdecor_frame_set_min_content_size _glfw.wl.libdecor.libdecor_frame_set_min_content_size_
#define libdecor_frame_set_max_content_size _glfw.wl.libdecor.libdecor_frame_set_max_content_size_
#define libdecor_frame_set_maximized _glfw.wl.libdecor.libdecor_frame_set_maximized_
#define libdecor_frame_unset_maximized _glfw.wl.libdecor.libdecor_frame_unset_maximized_
#define libdecor_frame_set_capabilities _glfw.wl.libdecor.libdecor_frame_set_capabilities_
#define libdecor_frame_unset_capabilities _glfw.wl.libdecor.libdecor_frame_unset_capabilities_
#define libdecor_frame_set_visibility _glfw.wl.libdecor.libdecor_frame_set_visibility_
#define libdecor_frame_get_xdg_toplevel _glfw.wl.libdecor.libdecor_frame_get_xdg_toplevel_
#define libdecor_configuration_get_content_size _glfw.wl.libdecor.libdecor_configuration_get_content_size_
#define libdecor_configuration_get_window_state _glfw.wl.libdecor.libdecor_configuration_get_window_state_
#define libdecor_state_new _glfw.wl.libdecor.libdecor_state_new_
#define libdecor_state_free _glfw.wl.libdecor.libdecor_state_free_

typedef enum _GLFWdecorationSideWayland
{
    CENTRAL_WINDOW,
    TOP_DECORATION,
    LEFT_DECORATION,
    RIGHT_DECORATION,
    BOTTOM_DECORATION,
} _GLFWdecorationSideWayland;

typedef struct _GLFWWaylandBufferPair {
    struct wl_buffer *a, *b, *front, *back;
    struct { uint8_t *a, *b, *front, *back; } data;
    bool has_pending_update;
    size_t size_in_bytes, width, height, stride;
    bool a_needs_to_be_destroyed, b_needs_to_be_destroyed;
} _GLFWWaylandBufferPair;

typedef struct _GLFWWaylandCSDEdge {
    struct wl_surface *surface;
    struct wl_subsurface *subsurface;
    _GLFWWaylandBufferPair buffer;
    int x, y;
} _GLFWWaylandCSDEdge;

typedef enum WaylandWindowState {

    TOPLEVEL_STATE_NONE = 0,
    TOPLEVEL_STATE_MAXIMIZED = 1,
    TOPLEVEL_STATE_FULLSCREEN = 2,
    TOPLEVEL_STATE_RESIZING = 4,
    TOPLEVEL_STATE_ACTIVATED = 8,
    TOPLEVEL_STATE_TILED_LEFT = 16,
    TOPLEVEL_STATE_TILED_RIGHT = 32,
    TOPLEVEL_STATE_TILED_TOP = 64,
    TOPLEVEL_STATE_TILED_BOTTOM = 128,
    TOPLEVEL_STATE_SUSPENDED = 256,
} WaylandWindowState;

typedef struct glfw_wl_xdg_activation_request {
    GLFWid window_id;
    GLFWactivationcallback callback;
    void *callback_data;
    uintptr_t request_id;
    void *token;
} glfw_wl_xdg_activation_request;


static const WaylandWindowState TOPLEVEL_STATE_DOCKED = TOPLEVEL_STATE_MAXIMIZED | TOPLEVEL_STATE_FULLSCREEN | TOPLEVEL_STATE_TILED_TOP | TOPLEVEL_STATE_TILED_LEFT | TOPLEVEL_STATE_TILED_RIGHT | TOPLEVEL_STATE_TILED_BOTTOM;

enum WaylandWindowPendingState {
    PENDING_STATE_TOPLEVEL = 1,
    PENDING_STATE_DECORATION = 2
};

// Wayland-specific per-window data
//
typedef struct _GLFWwindowWayland
{
    int                         width, height;
    bool                        visible;
    bool                        hovered;
    bool                        transparent;
    struct wl_surface*          surface;
    bool                        waiting_for_swap_to_commit;
    struct wl_egl_window*       native;
    struct wl_callback*         callback;

    struct {
        struct xdg_surface*     surface;
        struct xdg_toplevel*    toplevel;
        struct zxdg_toplevel_decoration_v1* decoration;
    } xdg;

   struct {
        struct libdecor_frame*  frame;
        bool                    mode;
   } libdecor;

    _GLFWcursor*                currentCursor;
    double                      cursorPosX, cursorPosY, allCursorPosX, allCursorPosY;

    char*                       title;
    char                        appId[256];

    // We need to track the monitors the window spans on to calculate the
    // optimal scaling factor.
    int                         scale;
    bool                        initial_scale_notified;
    _GLFWmonitor**              monitors;
    int                         monitorsCount;
    int                         monitorsSize;

    struct {
        struct zwp_relative_pointer_v1*    relativePointer;
        struct zwp_locked_pointer_v1*      lockedPointer;
    } pointerLock;

    struct {
        bool serverSide, buffer_destroyed;
        _GLFWdecorationSideWayland focus;
        _GLFWWaylandCSDEdge top, left, right, bottom;

        struct {
            uint8_t *data;
            size_t size;
        } mapping;

        struct {
            int width, height, scale;
            bool focused;
        } for_window_state;

        struct {
            unsigned int width, top, horizontal, vertical, visible_titlebar_height;
        } metrics;

        struct {
            int32_t x, y, width, height;
        } geometry;

        struct {
            uint32_t *data;
            size_t for_decoration_size, stride, segments, corner_size;
        } shadow_tile;
        monotonic_t last_click_on_top_decoration_at;

        uint32_t titlebar_color;
        bool use_custom_titlebar_color;
    } decorations;

    struct {
        unsigned long long id;
        void(*callback)(unsigned long long id);
        struct wl_callback *current_wl_callback;
    } frameCallbackData;

    struct {
        int32_t width, height;
    } user_requested_content_size;

    bool maximize_on_first_show;
    // counters for ignoring axis events following axis_discrete events in the
    // same frame along the same axis
    struct {
        unsigned int x, y;
    } axis_discrete_count;
    bool surface_configured_once;

    uint32_t pending_state;
    struct {
        int width, height;
        uint32_t toplevel_states;
        uint32_t decoration_mode;
    } current, pending;
} _GLFWwindowWayland;

typedef enum _GLFWWaylandOfferType
{
    EXPIRED,
    CLIPBOARD,
    DRAG_AND_DROP,
    PRIMARY_SELECTION
}_GLFWWaylandOfferType ;

typedef struct _GLFWWaylandDataOffer
{
    void *id;
    _GLFWWaylandOfferType offer_type;
    size_t idx;
    bool is_self_offer;
    bool is_primary;
    const char *mime_for_drop;
    uint32_t source_actions;
    uint32_t dnd_action;
    struct wl_surface *surface;
    const char **mimes;
    size_t mimes_capacity, mimes_count;
} _GLFWWaylandDataOffer;

// Wayland-specific global data
//
typedef struct _GLFWlibraryWayland
{
    struct wl_display*          display;
    struct wl_registry*         registry;
    struct wl_compositor*       compositor;
    struct wl_subcompositor*    subcompositor;
    struct wl_shm*              shm;
    struct wl_seat*             seat;
    struct wl_pointer*          pointer;
    struct wl_keyboard*         keyboard;
    struct wl_data_device_manager*          dataDeviceManager;
    struct wl_data_device*      dataDevice;
    struct xdg_wm_base*         wmBase;
    struct zxdg_decoration_manager_v1*      decorationManager;
    struct zwp_relative_pointer_manager_v1* relativePointerManager;
    struct zwp_pointer_constraints_v1*      pointerConstraints;
    struct wl_data_source*                  dataSourceForClipboard;
    struct zwp_primary_selection_device_manager_v1* primarySelectionDeviceManager;
    struct zwp_primary_selection_device_v1*    primarySelectionDevice;
    struct zwp_primary_selection_source_v1*    dataSourceForPrimarySelection;
    struct xdg_activation_v1* xdg_activation_v1;

    int                         compositorVersion;
    int                         seatVersion;

    struct wl_surface*          cursorSurface;
    GLFWCursorShape             cursorPreviousShape;
    uint32_t                    serial, input_serial, pointer_serial, keyboard_enter_serial;

    int32_t                     keyboardRepeatRate;
    monotonic_t                 keyboardRepeatDelay;

    struct {
        uint32_t                key;
        id_type                 keyRepeatTimer;
        GLFWid                  keyboardFocusId;
    } keyRepeatInfo;
    id_type                     cursorAnimationTimer;
    _GLFWXKBData                xkb;
    _GLFWDBUSData               dbus;

    _GLFWwindow*                pointerFocus;
    GLFWid                      keyboardFocusId;

    struct {
        void*                   handle;

        PFN_wl_cursor_theme_load theme_load;
        PFN_wl_cursor_theme_destroy theme_destroy;
        PFN_wl_cursor_theme_get_cursor theme_get_cursor;
        PFN_wl_cursor_image_get_buffer image_get_buffer;
    } cursor;

    struct {
        void*                   handle;

        PFN_wl_egl_window_create window_create;
        PFN_wl_egl_window_destroy window_destroy;
        PFN_wl_egl_window_resize window_resize;
    } egl;

    struct {
        glfw_wl_xdg_activation_request *array;
        size_t capacity, sz;
    } activation_requests;

    EventLoopData eventLoopData;
    size_t dataOffersCounter;
    _GLFWWaylandDataOffer dataOffers[8];

    struct {
        void*                   handle;
        struct libdecor*        context;
        PFN_libdecor_new        libdecor_new_;
        PFN_libdecor_unref      libdecor_unref_;
        PFN_libdecor_get_fd     libdecor_get_fd_;
        PFN_libdecor_dispatch   libdecor_dispatch_;
        PFN_libdecor_decorate   libdecor_decorate_;
        PFN_libdecor_frame_unref libdecor_frame_unref_;
        PFN_libdecor_frame_set_app_id libdecor_frame_set_app_id_;
        PFN_libdecor_frame_set_title libdecor_frame_set_title_;
        PFN_libdecor_frame_set_minimized libdecor_frame_set_minimized_;
        PFN_libdecor_frame_set_fullscreen libdecor_frame_set_fullscreen_;
        PFN_libdecor_frame_unset_fullscreen libdecor_frame_unset_fullscreen_;
        PFN_libdecor_frame_map libdecor_frame_map_;
        PFN_libdecor_frame_commit libdecor_frame_commit_;
        PFN_libdecor_frame_set_min_content_size libdecor_frame_set_min_content_size_;
        PFN_libdecor_frame_set_max_content_size libdecor_frame_set_max_content_size_;
        PFN_libdecor_frame_set_maximized libdecor_frame_set_maximized_;
        PFN_libdecor_frame_unset_maximized libdecor_frame_unset_maximized_;
        PFN_libdecor_frame_set_capabilities libdecor_frame_set_capabilities_;
        PFN_libdecor_frame_unset_capabilities libdecor_frame_unset_capabilities_;
        PFN_libdecor_frame_set_visibility libdecor_frame_set_visibility_;
        PFN_libdecor_frame_get_xdg_toplevel libdecor_frame_get_xdg_toplevel_;
        PFN_libdecor_configuration_get_content_size libdecor_configuration_get_content_size_;
        PFN_libdecor_configuration_get_window_state libdecor_configuration_get_window_state_;
        PFN_libdecor_state_new libdecor_state_new_;
        PFN_libdecor_state_free libdecor_state_free_;
    } libdecor;
} _GLFWlibraryWayland;

// Wayland-specific per-monitor data
//
typedef struct _GLFWmonitorWayland
{
    struct wl_output*           output;
    uint32_t                    name;
    int                         currentMode;

    int                         x;
    int                         y;
    int                         scale;

} _GLFWmonitorWayland;

// Wayland-specific per-cursor data
//
typedef struct _GLFWcursorWayland
{
    struct wl_cursor*           cursor;
    struct wl_buffer*           buffer;
    int                         width, height;
    int                         xhot, yhot;
    unsigned int                currentImage;
    /** The scale of the cursor, or 0 if the cursor should be loaded late, or -1 if the cursor variable itself is unused. */
    int                         scale;
    /** Cursor shape stored to allow late cursor loading in setCursorImage. */
    GLFWCursorShape             shape;
} _GLFWcursorWayland;


void _glfwAddOutputWayland(uint32_t name, uint32_t version);
void _glfwWaylandAfterBufferSwap(_GLFWwindow *window);
void _glfwSetupWaylandDataDevice(void);
void _glfwSetupWaylandPrimarySelectionDevice(void);
void animateCursorImage(id_type timer_id, void *data);
struct wl_cursor* _glfwLoadCursor(GLFWCursorShape, struct wl_cursor_theme*);
void destroy_data_offer(_GLFWWaylandDataOffer*);
