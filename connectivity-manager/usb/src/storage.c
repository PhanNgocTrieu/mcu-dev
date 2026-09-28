#include "storage.h"

#include "log.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#define USB_MOUNT_ROOT_DEFAULT "/run/connectivity/usb"

static char g_root[USB_PATH_LEN] = USB_MOUNT_ROOT_DEFAULT;
static usb_storage_ops_t g_ops;
static int g_ops_ready;

static void copy_msg(char* dst, size_t len, const char* src)
{
    if (!dst || len == 0) {
        return;
    }
    snprintf(dst, len, "%s", src ? src : "");
}

static int default_find_block(const char* sys_path, char* block_out, size_t block_len)
{
    DIR* dir;
    struct dirent* entry;

    if (!sys_path || !sys_path[0] || !block_out || block_len == 0) {
        return 0;
    }
    dir = opendir("/sys/block");
    if (!dir) {
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        char sys_block[PATH_MAX];
        char resolved[PATH_MAX];
        char partition[PATH_MAX];

        if (entry->d_name[0] == '.') {
            continue;
        }
        snprintf(sys_block, sizeof(sys_block), "/sys/block/%s", entry->d_name);
        if (!realpath(sys_block, resolved)) {
            continue;
        }
        /* Block device của USB nằm dưới syspath của usb_device, ví dụ .../1-1.2/.../block/sda. */
        if (!strstr(resolved, sys_path)) {
            continue;
        }
        snprintf(partition, sizeof(partition), "/sys/block/%s/%s1", entry->d_name, entry->d_name);
        if (access(partition, F_OK) == 0) {
            snprintf(block_out, block_len, "/dev/%s1", entry->d_name);
        } else {
            snprintf(block_out, block_len, "/dev/%s", entry->d_name);
        }
        closedir(dir);
        return 1;
    }
    closedir(dir);
    return 0;
}

static int try_mount(const char* block, const char* target, const char* fstype, const char* data)
{
    if (mount(block, target, fstype, MS_NOSUID | MS_NODEV, data) == 0) {
        return 1;
    }
    /* EBUSY: điểm mount hoặc thiết bị đã được gắn. Coi như xong. */
    return errno == EBUSY;
}

static int default_mount_fs(const char* block, const char* target, char* err, size_t err_len)
{
    /*
     * Thử các filesystem USB hay gặp. Kernel cần module tương ứng
     * (vfat, exfat, ntfs3, ext4). Không gọi udisks.
     */
    static const struct {
        const char* fstype;
        const char* data;
    } attempts[] = {
        {"vfat", "utf8,shortname=mixed"},
        {"exfat", ""},
        {"ntfs3", ""},
        {"ext4", ""},
    };
    size_t i;

    for (i = 0; i < sizeof(attempts) / sizeof(attempts[0]); ++i) {
        if (try_mount(block, target, attempts[i].fstype, attempts[i].data)) {
            snprintf(err, err_len, "mounted_%s", attempts[i].fstype);
            return 1;
        }
    }
    snprintf(err, err_len, "mount_failed_errno=%d", errno);
    return 0;
}

static int default_unmount_fs(const char* target, char* err, size_t err_len)
{
    if (!target || !target[0]) {
        copy_msg(err, err_len, "no_mount_point");
        return 0;
    }
    if (umount2(target, MNT_DETACH) == 0 || errno == EINVAL || errno == ENOENT) {
        copy_msg(err, err_len, "unmounted");
        return 1;
    }
    snprintf(err, err_len, "umount_failed_errno=%d", errno);
    return 0;
}

static void ensure_default_ops(void)
{
    if (g_ops_ready) {
        return;
    }
    g_ops.find_block = default_find_block;
    g_ops.mount_fs = default_mount_fs;
    g_ops.unmount_fs = default_unmount_fs;
    g_ops_ready = 1;
}

void usb_storage_set_root(const char* root)
{
    if (!root || !root[0]) {
        snprintf(g_root, sizeof(g_root), "%s", USB_MOUNT_ROOT_DEFAULT);
        return;
    }
    snprintf(g_root, sizeof(g_root), "%s", root);
}

void usb_storage_set_ops(const usb_storage_ops_t* ops)
{
    ensure_default_ops();
    if (!ops) {
        g_ops.find_block = default_find_block;
        g_ops.mount_fs = default_mount_fs;
        g_ops.unmount_fs = default_unmount_fs;
        return;
    }
    if (ops->find_block) {
        g_ops.find_block = ops->find_block;
    }
    if (ops->mount_fs) {
        g_ops.mount_fs = ops->mount_fs;
    }
    if (ops->unmount_fs) {
        g_ops.unmount_fs = ops->unmount_fs;
    }
}

static int mkdir_one(const char* path)
{
    struct stat st;

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

static void sanitize_id(const char* device_id, char* out, size_t out_len)
{
    size_t i;
    size_t w = 0;

    for (i = 0; device_id[i] && w + 1 < out_len; ++i) {
        char ch = device_id[i];
        if (ch == '/' || ch == ' ') {
            ch = '_';
        }
        out[w++] = ch;
    }
    out[w] = '\0';
    if (w == 0) {
        snprintf(out, out_len, "%s", "disk");
    }
}

int usb_storage_attach(usb_device_t* device, char* err, size_t err_len)
{
    char safe_id[USB_ID_LEN];
    char target[USB_PATH_LEN];

    ensure_default_ops();
    if (!device) {
        copy_msg(err, err_len, "no_device");
        return 0;
    }
    device->block_dev[0] = '\0';
    device->mount_point[0] = '\0';
    if (!g_ops.find_block(device->sys_path, device->block_dev, sizeof(device->block_dev))) {
        copy_msg(err, err_len, "no_block_device");
        USB_LOG_WARN("storage: no block device under usb syspath");
        return 0;
    }
    if (!mkdir_one(g_root)) {
        snprintf(err, err_len, "mkdir_root_failed:%s", g_root);
        return 0;
    }
    sanitize_id(device->device_id, safe_id, sizeof(safe_id));
    if (snprintf(target, sizeof(target), "%.300s/%.160s", g_root, safe_id) >= (int)sizeof(target)) {
        copy_msg(err, err_len, "mount_path_too_long");
        return 0;
    }
    if (!mkdir_one(target)) {
        snprintf(err, err_len, "mkdir_mount_failed:%s", target);
        return 0;
    }
    if (!g_ops.mount_fs(device->block_dev, target, err, err_len)) {
        USB_LOG_WARN(err ? err : "storage mount failed");
        rmdir(target);
        return 0;
    }
    snprintf(device->mount_point, sizeof(device->mount_point), "%s", target);
    USB_LOG_INFO(device->mount_point);
    return 1;
}

int usb_storage_detach(const char* mount_point, char* err, size_t err_len)
{
    int ok;

    ensure_default_ops();
    if (!mount_point || !mount_point[0]) {
        copy_msg(err, err_len, "no_mount_point");
        return 1;
    }
    ok = g_ops.unmount_fs(mount_point, err, err_len);
    rmdir(mount_point);
    return ok;
}
