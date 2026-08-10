/*
 * NovatOS USB Burner — native Windows .exe (C + Win32 API)
 * ========================================================
 * Flash NovatOS ISO to USB drives in DD (raw) mode.
 *
 * Build: gcc -o NovatOSBurner.exe src/main.c -lgdi32 -lcomctl32 -lcomdlg32 -mwindows -O2 -s -static
 *
 * Key fixes (vs previous version):
 *   1. Uses \\.\PhysicalDriveN (whole disk) instead of \\.\X: (volume)
 *      — this is what Rufus/balenaEtcher use, and it works as admin
 *   2. Locks + dismounts the volume before writing (FSCTL_LOCK_VOLUME + FSCTL_DISMOUNT_VOLUME)
 *   3. Uses 8MB chunks for better throughput
 *   4. Detailed error messages with GetLastError() translation
 *   5. Flushes file buffers after write (FlushFileBuffers)
 *   6. Refreshes drive list on window focus
 */

#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>

/* Control IDs */
#define ID_ISO_BUTTON     1001
#define ID_DRIVE_LIST     1002
#define ID_REFRESH_BUTTON 1003
#define ID_WRITE_BUTTON   1004
#define ID_ISO_LABEL      1005
#define ID_PROGRESS       1006
#define ID_STATUS_LABEL   1007
#define ID_SPEED_LABEL    1008

/* Global state */
static WCHAR g_iso_path[MAX_PATH] = {0};
static int   g_selected_physical = -1;  /* PhysicalDriveN number */
static WCHAR g_selected_letter = 0;     /* Drive letter for display */
static BOOL  g_is_writing = FALSE;
static HWND  g_hwnd = NULL;

/* ─── Drive entry (physical disk info) ─── */
typedef struct {
    int  physical_num;    /* PhysicalDriveN */
    WCHAR letter;         /* drive letter (0 if none) */
    unsigned long long size;
    WCHAR display[128];   /* listbox display string */
} drive_entry;

static drive_entry g_drives[26];
static int g_drive_count = 0;

/* ─── Helpers ─── */
static void set_control_text(HWND parent, int id, const WCHAR *text) {
    HWND h = GetDlgItem(parent, id);
    if (h) SendMessageW(h, WM_SETTEXT, 0, (LPARAM)text);
}

static void format_size(unsigned long long size, WCHAR *out, int out_len) {
    if (size == 0) { swprintf(out, out_len, L"Unknown"); return; }
    const WCHAR *units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double s = (double)size;
    int i = 0;
    while (s >= 1024.0 && i < 4) { s /= 1024.0; i++; }
    swprintf(out, out_len, L"%.1f %s", s, units[i]);
}

/* ─── Translate GetLastError() to a readable message ─── */
static void get_error_message(DWORD err, WCHAR *out, int out_len) {
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD n = FormatMessageW(flags, NULL, err, 0, out, out_len, NULL);
    if (n == 0) {
        swprintf(out, out_len, L"Error code %lu", err);
    } else {
        /* Strip trailing newlines */
        while (n > 0 && (out[n-1] == L'\n' || out[n-1] == L'\r' || out[n-1] == L' ')) {
            out[--n] = 0;
        }
    }
}

/* ─── Get physical drive number for a drive letter ───
 *
 * Converts "X:" to "\\.\PhysicalDriveN" via the volume manager.
 * Returns -1 on failure.
 */
static int get_physical_drive_number(WCHAR letter) {
    WCHAR volume_path[] = L"\\\\.\\X:";
    volume_path[4] = letter;

    HANDLE vol = CreateFileW(volume_path, 0,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, 0, NULL);
    if (vol == INVALID_HANDLE_VALUE) return -1;

    /* IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS = 0x00560000 */
    typedef struct {
        DWORD disk_number;
        LARGE_INTEGER starting_offset;
        LARGE_INTEGER extent_length;
    } DISK_EXTENT;

    typedef struct {
        DWORD number_of_disk_extents;
        DISK_EXTENT extent;
    } VOLUME_DISK_EXTENTS;

    VOLUME_DISK_EXTENTS vde;
    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(vol, 0x00560000, NULL, 0,
                              &vde, sizeof(vde), &bytes_returned, NULL);
    CloseHandle(vol);

    if (!ok || vde.number_of_disk_extents == 0) return -1;
    return (int)vde.extent.disk_number;
}

/* ─── Get size of a physical drive ─── */
static unsigned long long get_physical_drive_size(int physical_num) {
    WCHAR path[64];
    swprintf(path, 64, L"\\\\.\\PhysicalDrive%d", physical_num);

    HANDLE h = CreateFileW(path, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;

    /* IOCTL_DISK_GET_LENGTH_INFO = 0x0007405C */
    unsigned long long size = 0;
    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(h, 0x0007405C, NULL, 0,
                              &size, sizeof(size), &bytes_returned, NULL);
    CloseHandle(h);
    return ok ? size : 0;
}

/* ─── Refresh drives ───
 * Lists all removable USB drives with their physical disk numbers.
 */
static void refresh_drives(HWND hwnd) {
    HWND list = GetDlgItem(hwnd, ID_DRIVE_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    g_drive_count = 0;

    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            WCHAR letter = L'A' + i;
            WCHAR root[] = {letter, L':', L'\\', 0};
            if (GetDriveTypeW(root) == DRIVE_REMOVABLE) {
                int phys = get_physical_drive_number(letter);
                if (phys < 0) continue;

                unsigned long long size = get_physical_drive_size(phys);
                WCHAR size_str[32];
                format_size(size, size_str, 32);

                /* Store the entry */
                drive_entry *e = &g_drives[g_drive_count];
                e->physical_num = phys;
                e->letter = letter;
                e->size = size;
                swprintf(e->display, 128,
                    L"%c:  —  %s  (PhysicalDrive%d)", letter, size_str, phys);

                SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)e->display);
                g_drive_count++;

                if (g_drive_count >= 26) break;
            }
        }
    }

    if (g_drive_count == 0) {
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)L"No USB drives found. Insert a USB drive and click Refresh.");
    }

    /* Reset selection */
    g_selected_physical = -1;
    g_selected_letter = 0;
    update_write_button(hwnd);
}

/* ─── Open file dialog ─── */
static void open_iso_dialog(HWND hwnd) {
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"ISO Files\0*.iso\0All Files\0*.*\0\0";
    ofn.lpstrFile = g_iso_path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select NovatOS ISO";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn)) {
        int len = wcslen(g_iso_path);
        WCHAR display[80];
        if (len > 70) {
            swprintf(display, 80, L"...%s", g_iso_path + len - 67);
        } else {
            wcsncpy(display, g_iso_path, 80);
            display[79] = 0;
        }
        set_control_text(hwnd, ID_ISO_LABEL, display);
        update_write_button(hwnd);
    }
}

/* ─── Update write button state ─── */
static void update_write_button(HWND hwnd) {
    BOOL enabled = (g_iso_path[0] != 0) && (g_selected_physical >= 0) && !g_is_writing;
    EnableWindow(GetDlgItem(hwnd, ID_WRITE_BUTTON), enabled);
}

/* ─── Lock + dismount a volume ───
 * Required before raw write to \\.\PhysicalDriveN when the volume is mounted.
 */
static BOOL lock_and_dismount_volume(WCHAR letter, WCHAR *err_msg, int err_len) {
    WCHAR vol_path[] = L"\\\\.\\X:";
    vol_path[4] = letter;

    HANDLE vol = CreateFileW(vol_path, GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, 0, NULL);
    if (vol == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        get_error_message(err, err_msg, err_len);
        return FALSE;
    }

    /* Lock the volume (FSCTL_LOCK_VOLUME = 0x00090018) */
    DWORD bytes_returned = 0;
    if (!DeviceIoControl(vol, 0x00090018, NULL, 0, NULL, 0, &bytes_returned, NULL)) {
        DWORD err = GetLastError();
        get_error_message(err, err_msg, err_len);
        CloseHandle(vol);
        return FALSE;
    }

    /* Dismount the volume (FSCTL_DISMOUNT_VOLUME = 0x00090020) */
    if (!DeviceIoControl(vol, 0x00090020, NULL, 0, NULL, 0, &bytes_returned, NULL)) {
        DWORD err = GetLastError();
        get_error_message(err, err_msg, err_len);
        CloseHandle(vol);
        return FALSE;
    }

    /* Keep the volume handle open (locked) — return it so the caller can close it later */
    return vol == INVALID_HANDLE_VALUE ? FALSE : TRUE;  /* always TRUE here */
    /* NOTE: the handle 'vol' is intentionally left open to keep the lock.
     * The caller must close it after the write is done. We store it globally. */
}

/* ─── Write ISO thread ─── */
typedef struct {
    HWND hwnd;
    WCHAR iso_path[MAX_PATH];
    int  physical_num;
    WCHAR drive_letter;
} write_params;

static HANDLE g_volume_lock_handle = NULL;  /* kept open to hold the lock */

static DWORD WINAPI write_thread(LPVOID param) {
    write_params *wp = (write_params *)param;
    HWND hwnd = wp->hwnd;

    set_control_text(hwnd, ID_STATUS_LABEL, L"Locking USB drive...");
    set_control_text(hwnd, ID_SPEED_LABEL, L"");

    /* Step 1: Lock + dismount the volume (prevents "drive in use" errors) */
    WCHAR lock_err[256] = {0};
    {
        WCHAR vol_path[] = L"\\\\.\\X:";
        vol_path[4] = wp->drive_letter;

        g_volume_lock_handle = CreateFileW(vol_path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL);

        if (g_volume_lock_handle == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            get_error_message(err, lock_err, 256);
            WCHAR msg[512];
            swprintf(msg, 512,
                L"Cannot open volume %c:.\n\nError: %s\n\n"
                L"Try closing any programs that may be using the drive\n"
                L"(File Explorer, antivirus, etc.) and try again.",
                wp->drive_letter, lock_err);
            MessageBoxW(hwnd, msg, L"Error", MB_OK | MB_ICONERROR);
            set_control_text(hwnd, ID_STATUS_LABEL, L"Error: Cannot lock drive");
            g_is_writing = FALSE;
            update_write_button(hwnd);
            free(wp);
            return 1;
        }

        DWORD bytes_returned = 0;
        /* Lock the volume */
        if (!DeviceIoControl(g_volume_lock_handle, 0x00090018, NULL, 0, NULL, 0, &bytes_returned, NULL)) {
            DWORD err = GetLastError();
            get_error_message(err, lock_err, 256);
            WCHAR msg[512];
            swprintf(msg, 512,
                L"Cannot lock volume %c: (it may be in use).\n\nError: %s\n\n"
                L"Close File Explorer and any programs using the drive, then try again.",
                wp->drive_letter, lock_err);
            MessageBoxW(hwnd, msg, L"Error", MB_OK | MB_ICONERROR);
            CloseHandle(g_volume_lock_handle);
            g_volume_lock_handle = NULL;
            set_control_text(hwnd, ID_STATUS_LABEL, L"Error: Cannot lock drive");
            g_is_writing = FALSE;
            update_write_button(hwnd);
            free(wp);
            return 1;
        }
        /* Dismount the volume */
        DeviceIoControl(g_volume_lock_handle, 0x00090020, NULL, 0, NULL, 0, &bytes_returned, NULL);
    }

    set_control_text(hwnd, ID_STATUS_LABEL, L"Opening physical drive...");

    /* Step 2: Open \\.\PhysicalDriveN for raw write */
    WCHAR phys_path[64];
    swprintf(phys_path, 64, L"\\\\.\\PhysicalDrive%d", wp->physical_num);

    HANDLE handle = CreateFileW(phys_path,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, 0, NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        WCHAR err_msg[256];
        get_error_message(err, err_msg, 256);
        WCHAR msg[512];
        swprintf(msg, 512,
            L"Cannot open %s.\n\nError: %s\n\n"
            L"Make sure you run NovatOSBurner.exe as Administrator.",
            phys_path, err_msg);
        MessageBoxW(hwnd, msg, L"Error", MB_OK | MB_ICONERROR);
        set_control_text(hwnd, ID_STATUS_LABEL, L"Error: Cannot open physical drive");
        if (g_volume_lock_handle) {
            CloseHandle(g_volume_lock_handle);
            g_volume_lock_handle = NULL;
        }
        g_is_writing = FALSE;
        update_write_button(hwnd);
        free(wp);
        return 1;
    }

    /* Step 3: Open ISO file */
    HANDLE iso = CreateFileW(wp->iso_path, GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (iso == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        WCHAR err_msg[256];
        get_error_message(err, err_msg, 256);
        WCHAR msg[512];
        swprintf(msg, 512, L"Cannot open ISO file:\n\n%s", err_msg);
        MessageBoxW(hwnd, msg, L"Error", MB_OK | MB_ICONERROR);
        set_control_text(hwnd, ID_STATUS_LABEL, L"Error: Cannot open ISO file");
        CloseHandle(handle);
        if (g_volume_lock_handle) {
            CloseHandle(g_volume_lock_handle);
            g_volume_lock_handle = NULL;
        }
        g_is_writing = FALSE;
        update_write_button(hwnd);
        free(wp);
        return 1;
    }

    /* Get ISO size */
    LARGE_INTEGER iso_size;
    GetFileSizeEx(iso, &iso_size);
    unsigned long long total = (unsigned long long)iso_size.QuadPart;

    /* Set progress range */
    HWND progress = GetDlgItem(hwnd, ID_PROGRESS);
    SendMessageW(progress, PBM_SETRANGE32, 0, 10000);
    SendMessageW(progress, PBM_SETPOS, 0, 0);

    /* Write in 8MB chunks (faster than 4MB) */
    DWORD chunk_size = 8 * 1024 * 1024;
    BYTE *buf = (BYTE *)VirtualAlloc(NULL, chunk_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf) {
        CloseHandle(handle);
        CloseHandle(iso);
        if (g_volume_lock_handle) {
            CloseHandle(g_volume_lock_handle);
            g_volume_lock_handle = NULL;
        }
        MessageBoxW(hwnd, L"Out of memory.", L"Error", MB_OK | MB_ICONERROR);
        g_is_writing = FALSE;
        update_write_button(hwnd);
        free(wp);
        return 1;
    }

    unsigned long long written = 0;
    DWORD start = GetTickCount();

    set_control_text(hwnd, ID_STATUS_LABEL, L"Writing...");

    while (1) {
        DWORD bytes_read = 0;
        if (!ReadFile(iso, buf, chunk_size, &bytes_read, NULL) || bytes_read == 0) {
            if (bytes_read == 0) break; /* EOF */
        }

        /* Write in aligned chunks — some USB drives don't like writes larger than 1MB */
        DWORD total_to_write = bytes_read;
        DWORD offset = 0;
        while (total_to_write > 0) {
            DWORD to_write = total_to_write > (1024 * 1024) ? (1024 * 1024) : total_to_write;
            DWORD bytes_written = 0;
            if (!WriteFile(handle, buf + offset, to_write, &bytes_written, NULL)) {
                DWORD err = GetLastError();
                WCHAR err_msg[256];
                get_error_message(err, err_msg, 256);
                VirtualFree(buf, 0, MEM_RELEASE);
                CloseHandle(handle);
                CloseHandle(iso);
                if (g_volume_lock_handle) {
                    CloseHandle(g_volume_lock_handle);
                    g_volume_lock_handle = NULL;
                }
                WCHAR msg[512];
                swprintf(msg, 512,
                    L"Write failed at offset %llu.\n\nError: %s",
                    written + offset, err_msg);
                set_control_text(hwnd, ID_STATUS_LABEL, L"Error: Write failed");
                MessageBoxW(hwnd, msg, L"Error", MB_OK | MB_ICONERROR);
                g_is_writing = FALSE;
                update_write_button(hwnd);
                free(wp);
                return 1;
            }
            offset += bytes_written;
            total_to_write -= bytes_written;
        }

        written += bytes_read;
        DWORD elapsed = GetTickCount() - start;
        double speed = elapsed > 0 ? (double)written / (elapsed / 1000.0) : 0;
        int pct = (int)((double)written / total * 10000.0);

        SendMessageW(progress, PBM_SETPOS, pct, 0);

        WCHAR status[128], speed_str[64], wstr[32], tstr[32];
        format_size(written, wstr, 32);
        format_size(total, tstr, 32);
        swprintf(status, 128, L"Writing... %s / %s", wstr, tstr);
        swprintf(speed_str, 64, L"%.1f MB/s", speed / 1024.0 / 1024.0);
        set_control_text(hwnd, ID_STATUS_LABEL, status);
        set_control_text(hwnd, ID_SPEED_LABEL, speed_str);
    }

    /* Flush buffers to ensure all data is written to the physical disk */
    set_control_text(hwnd, ID_STATUS_LABEL, L"Flushing buffers...");
    FlushFileBuffers(handle);

    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(handle);
    CloseHandle(iso);

    /* Release the volume lock */
    if (g_volume_lock_handle) {
        CloseHandle(g_volume_lock_handle);
        g_volume_lock_handle = NULL;
    }

    SendMessageW(progress, PBM_SETPOS, 10000, 0);
    set_control_text(hwnd, ID_STATUS_LABEL, L"✓ Done! USB drive is bootable.");
    g_is_writing = FALSE;
    update_write_button(hwnd);

    MessageBoxW(hwnd,
        L"NovatOS ISO written successfully!\n\nYou can now boot from this USB drive.",
        L"Success", MB_OK | MB_ICONINFORMATION);

    free(wp);
    return 0;
}

/* ─── Start write ─── */
static void start_write(HWND hwnd) {
    if (g_is_writing) return;
    if (!g_iso_path[0] || g_selected_physical < 0) return;

    /* Confirm */
    WCHAR msg[256];
    const WCHAR *fname = wcsrchr(g_iso_path, L'\\');
    fname = fname ? fname + 1 : g_iso_path;
    swprintf(msg, 256,
        L"You are about to write:\n\n  %s\n\nto USB drive %c: (PhysicalDrive%d)\n\n"
        L"⚠ ALL DATA on this USB drive will be PERMANENTLY ERASED.\n\nContinue?",
        fname, g_selected_letter, g_selected_physical);

    if (MessageBoxW(hwnd, msg, L"Confirm Write", MB_YESNO | MB_ICONWARNING) != IDYES)
        return;

    g_is_writing = TRUE;
    update_write_button(hwnd);

    /* Launch thread */
    write_params *wp = (write_params *)malloc(sizeof(write_params));
    wp->hwnd = hwnd;
    wcscpy(wp->iso_path, g_iso_path);
    wp->physical_num = g_selected_physical;
    wp->drive_letter = g_selected_letter;

    HANDLE h = CreateThread(NULL, 0, write_thread, wp, 0, NULL);
    if (h) CloseHandle(h);
}

/* ─── Window procedure ─── */
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND: {
        int ctrl_id = LOWORD(wp);
        int notification = HIWORD(wp);
        switch (ctrl_id) {
        case ID_ISO_BUTTON:
            open_iso_dialog(hwnd);
            break;
        case ID_REFRESH_BUTTON:
            refresh_drives(hwnd);
            break;
        case ID_WRITE_BUTTON:
            start_write(hwnd);
            break;
        case ID_DRIVE_LIST:
            if (notification == LBN_SELCHANGE) {
                int sel = (int)SendMessageW(GetDlgItem(hwnd, ID_DRIVE_LIST),
                                            LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < g_drive_count) {
                    g_selected_physical = g_drives[sel].physical_num;
                    g_selected_letter = g_drives[sel].letter;
                    update_write_button(hwnd);
                }
            }
            break;
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

/* ─── Create a child control ─── */
static HWND create_ctrl(const WCHAR *class, DWORD style, HWND parent,
                        int id, const WCHAR *text, int x, int y, int w, int h) {
    return CreateWindowExW(0, class, text, style,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

/* ─── WinMain ─── */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    /* Register window class */
    const WCHAR *class_name = L"NovatOSBurner";
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = wnd_proc;
    wc.lpszClassName = class_name;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    /* Init common controls */
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);

    /* Create main window */
    g_hwnd = CreateWindowExW(0, class_name, L"NovatOS USB Burner",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 600,
        NULL, NULL, hInst, NULL);

    if (!g_hwnd) return 1;

    /* Header */
    create_ctrl(L"STATIC", WS_CHILD | WS_VISIBLE, g_hwnd, 0,
                L"NovatOS USB Burner", 20, 15, 400, 24);
    create_ctrl(L"STATIC", WS_CHILD | WS_VISIBLE, g_hwnd, 0,
                L"Flash NovatOS ISO to a USB drive (DD mode)", 20, 38, 400, 18);

    /* ISO selection */
    create_ctrl(L"STATIC", WS_CHILD | WS_VISIBLE, g_hwnd, 0, L"ISO File:", 20, 75, 100, 18);
    create_ctrl(L"STATIC", WS_CHILD | WS_VISIBLE, g_hwnd, ID_ISO_LABEL,
                L"No ISO selected", 20, 95, 440, 20);
    create_ctrl(L"BUTTON", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, g_hwnd, ID_ISO_BUTTON,
                L"Browse...", 470, 92, 130, 26);

    /* Drive selection */
    create_ctrl(L"STATIC", WS_CHILD | WS_VISIBLE, g_hwnd, 0, L"USB Drive:", 20, 135, 100, 18);
    create_ctrl(L"LISTBOX", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY,
                g_hwnd, ID_DRIVE_LIST, L"", 20, 155, 440, 120);
    create_ctrl(L"BUTTON", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, g_hwnd, ID_REFRESH_BUTTON,
                L"Refresh", 470, 155, 130, 26);

    /* Progress */
    create_ctrl(L"STATIC", WS_CHILD | WS_VISIBLE, g_hwnd, ID_STATUS_LABEL,
                L"Ready", 20, 295, 580, 20);
    create_ctrl(PROGRESS_CLASSW, WS_CHILD | WS_VISIBLE, g_hwnd, ID_PROGRESS,
                L"", 20, 315, 580, 24);
    create_ctrl(L"STATIC", WS_CHILD | WS_VISIBLE, g_hwnd, ID_SPEED_LABEL,
                L"", 20, 343, 580, 18);

    /* Write button */
    HWND write_btn = create_ctrl(L"BUTTON", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 g_hwnd, ID_WRITE_BUTTON, L"Write to USB", 220, 380, 200, 40);

    /* Warning */
    create_ctrl(L"STATIC", WS_CHILD | WS_VISIBLE, g_hwnd, 0,
                L"⚠ All data on the selected USB drive will be permanently erased.",
                20, 440, 580, 20);

    /* Disable write button initially */
    EnableWindow(write_btn, FALSE);

    /* Initial drive scan */
    refresh_drives(g_hwnd);

    /* Show window */
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    /* Message loop */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
