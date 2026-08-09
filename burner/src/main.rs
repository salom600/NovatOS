// NovatOS USB Burner — native Windows .exe (Rust + windows-sys)
// ==============================================================
// Flash NovatOS ISO to USB drives in DD (raw) mode.
// Single .exe, no runtime dependencies.
//
// Uses windows-sys (raw FFI bindings) for maximum stability across versions.

#![windows_subsystem = "windows"]

use std::ffi::OsStr;
use std::os::windows::ffi::OsStrExt;
use std::ptr;

use windows_sys::Win32::Foundation::{CloseHandle, INVALID_HANDLE_VALUE};
use windows_sys::Win32::Storage::FileSystem::{
    CreateFileW, GetFileSizeEx, ReadFile, WriteFile, FILE_ATTRIBUTE_NORMAL,
    FILE_SHARE_READ, FILE_SHARE_WRITE, GENERIC_READ, GENERIC_WRITE, OPEN_EXISTING,
};
use windows_sys::Win32::System::Threading::CreateThread;
use windows_sys::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, DefWindowProcW, DispatchMessageW, EnableWindow, GetDlgItem,
    GetMessageW, GetOpenFileNameW, LoadCursorW, MessageBoxW, PostQuitMessage,
    RegisterClassW, SendMessageW, ShowWindow, TranslateMessage,
    BM_SETSTATE, BS_PUSHBUTTON, COLOR_BTNFACE, CW_USEDEFAULT, IDC_ARROW,
    IDYES, LB_ADDSTRING, LB_GETCURSEL, LB_RESETCONTENT, MB_ICONERROR,
    MB_ICONINFORMATION, MB_ICONWARNING, MB_OK, MB_YESNO, OFN_FILEMUSTEXIST,
    OFN_HIDEREADONLY, OPENFILENAMEW, PBM_SETPOS, PBM_SETRANGE32, SW_SHOW,
    WM_COMMAND, WM_DESTROY, WM_SETTEXT, WNDCLASSW, WS_BORDER, WS_CHILD,
    WS_OVERLAPPEDWINDOW, WS_VISIBLE,
};

// ─── Control IDs ───
const ID_ISO_BUTTON: usize = 1001;
const ID_DRIVE_LIST: usize = 1002;
const ID_REFRESH_BUTTON: usize = 1003;
const ID_WRITE_BUTTON: usize = 1004;
const ID_ISO_LABEL: usize = 1005;
const ID_PROGRESS: usize = 1006;
const ID_STATUS_LABEL: usize = 1007;
const ID_SPEED_LABEL: usize = 1008;

const LBS_NOTIFY: u32 = 0x00100000;
const DRIVE_REMOVABLE: u32 = 2;

// ─── Global state ───
static mut ISO_PATH: Option<String> = None;
static mut SELECTED_DRIVE: Option<String> = None;
static mut IS_WRITING: bool = false;

fn to_wide(s: &str) -> Vec<u16> {
    OsStr::new(s).encode_wide().chain(std::iter::once(0)).collect()
}

fn from_wide(wide: &[u16]) -> String {
    let len = wide.iter().position(|&c| c == 0).unwrap_or(wide.len());
    String::from_utf16_lossy(&wide[..len])
}

fn format_size(size: u64) -> String {
    if size == 0 {
        return "Unknown".to_string();
    }
    let units = ["B", "KB", "MB", "GB", "TB"];
    let mut s = size as f64;
    let mut i = 0;
    while s >= 1024.0 && i < units.len() - 1 {
        s /= 1024.0;
        i += 1;
    }
    format!("{:.1} {}", s, units[i])
}

// ─── Window procedure ───
extern "system" fn wnd_proc(hwnd: isize, msg: u32, wp: usize, lp: isize) -> isize {
    unsafe {
        match msg {
            WM_COMMAND => {
                let ctrl_id = (wp & 0xFFFF) as usize;
                let notification = ((wp >> 16) & 0xFFFF) as usize;
                match ctrl_id {
                    ID_ISO_BUTTON => open_iso_dialog(hwnd),
                    ID_REFRESH_BUTTON => refresh_drives(hwnd),
                    ID_WRITE_BUTTON => start_write(hwnd),
                    ID_DRIVE_LIST if notification == 1 => on_drive_select(hwnd),
                    _ => {}
                }
                0
            }
            WM_DESTROY => {
                PostQuitMessage(0);
                0
            }
            _ => DefWindowProcW(hwnd, msg, wp, lp),
        }
    }
}

// ─── Create a child control ───
fn create_control(class: &str, style: u32, parent: isize, id: usize, text: &str, x: i32, y: i32, w: i32, h: i32) -> isize {
    unsafe {
        let class_w = to_wide(class);
        let text_w = to_wide(text);
        CreateWindowExW(
            0,
            class_w.as_ptr(),
            text_w.as_ptr(),
            style,
            x, y, w, h,
            parent,
            id as *mut _,
            ptr::null_mut(),
            ptr::null_mut(),
        )
    }
}

fn create_button(parent: isize, id: usize, text: &str, x: i32, y: i32, w: i32, h: i32) -> isize {
    create_control(
        "BUTTON",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON as u32,
        parent, id, text, x, y, w, h,
    )
}

fn create_label(parent: isize, id: usize, text: &str, x: i32, y: i32, w: i32, h: i32) -> isize {
    create_control("STATIC", WS_CHILD | WS_VISIBLE, parent, id, text, x, y, w, h)
}

fn create_listbox(parent: isize, id: usize, x: i32, y: i32, w: i32, h: i32) -> isize {
    create_control(
        "LISTBOX",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY,
        parent, id, "", x, y, w, h,
    )
}

fn create_progress(parent: isize, id: usize, x: i32, y: i32, w: i32, h: i32) -> isize {
    create_control("msctls_progress32", WS_CHILD | WS_VISIBLE, parent, id, "", x, y, w, h)
}

fn find_control(parent: isize, id: usize) -> isize {
    unsafe { GetDlgItem(parent, id as i32) }
}

fn set_control_text(parent: isize, id: usize, text: &str) {
    unsafe {
        let hwnd = find_control(parent, id);
        if hwnd != 0 {
            let text_w = to_wide(text);
            SendMessageW(hwnd, WM_SETTEXT as u32, 0, text_w.as_ptr() as _);
        }
    }
}

// ─── File dialog ───
fn open_iso_dialog(hwnd: isize) {
    unsafe {
        let mut file_buf = [0u16; 260];
        let filter = to_wide("ISO Files\0*.iso\0All Files\0*.*\0\0");
        let title = to_wide("Select NovatOS ISO");

        let mut ofn: OPENFILENAMEW = std::mem::zeroed();
        ofn.lStructSize = std::mem::size_of::<OPENFILENAMEW>() as u32;
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = filter.as_ptr();
        ofn.lpstrFile = file_buf.as_mut_ptr();
        ofn.nMaxFile = file_buf.len() as u32;
        ofn.lpstrTitle = title.as_ptr();
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

        if GetOpenFileNameW(&mut ofn as *mut _ as _) != 0 {
            let path = from_wide(&file_buf);
            ISO_PATH = Some(path.clone());

            let label_text = if path.len() > 60 {
                format!("...{}", &path[path.len().saturating_sub(57)..])
            } else {
                path
            };
            set_control_text(hwnd, ID_ISO_LABEL, &label_text);
            update_write_button(hwnd);
        }
    }
}

// ─── Get logical drives ───
fn get_logical_drives_bitmask() -> u32 {
    unsafe {
        extern "system" {
            fn GetLogicalDrives() -> u32;
        }
        GetLogicalDrives()
    }
}

fn get_drive_type(root: &str) -> u32 {
    unsafe {
        let root_w = to_wide(root);
        extern "system" {
            fn GetDriveTypeW(lpRootPathName: *const u16) -> u32;
        }
        GetDriveTypeW(root_w.as_ptr())
    }
}

fn get_drive_size(device_path: &str) -> u64 {
    unsafe {
        let path_w = to_wide(device_path);
        let handle = CreateFileW(
            path_w.as_ptr(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            ptr::null(),
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0,
        );

        if handle == INVALID_HANDLE_VALUE {
            return 0;
        }

        let mut file_size: i64 = 0;
        let _ = GetFileSizeEx(handle, &mut file_size);
        let _ = CloseHandle(handle);
        file_size as u64
    }
}

// ─── Refresh USB drives ───
fn refresh_drives(hwnd: isize) {
    unsafe {
        let listbox = find_control(hwnd, ID_DRIVE_LIST);
        SendMessageW(listbox, LB_RESETCONTENT, 0, 0);

        let drives = get_logical_drives_bitmask();

        for i in 0..26u32 {
            if drives & (1 << i) != 0 {
                let letter = (b'A' + i as u8) as char;
                let drive_root = format!("{}:\\", letter);

                if get_drive_type(&drive_root) == DRIVE_REMOVABLE {
                    let device_path = format!("\\\\.\\{}:", letter);
                    let size = get_drive_size(&device_path);
                    let size_str = format_size(size);
                    let label = format!("{}:  —  {}", letter, size_str);
                    let label_w = to_wide(&label);
                    SendMessageW(listbox, LB_ADDSTRING, 0, label_w.as_ptr() as _);
                }
            }
        }
    }
}

fn on_drive_select(hwnd: isize) {
    unsafe {
        let listbox = find_control(hwnd, ID_DRIVE_LIST);
        let sel = SendMessageW(listbox, LB_GETCURSEL, 0, 0);
        if sel >= 0 {
            let letter = (b'A' + sel as u8) as char;
            SELECTED_DRIVE = Some(letter.to_string());
            update_write_button(hwnd);
        }
    }
}

fn update_write_button(hwnd: isize) {
    unsafe {
        let btn = find_control(hwnd, ID_WRITE_BUTTON);
        let enabled = ISO_PATH.is_some() && SELECTED_DRIVE.is_some() && !IS_WRITING;
        EnableWindow(btn, enabled as i32);
    }
}

fn msg_box(hwnd: isize, msg: &str, title: &str, flags: u32) -> i32 {
    unsafe {
        let msg_w = to_wide(msg);
        let title_w = to_wide(title);
        MessageBoxW(hwnd, msg_w.as_ptr(), title_w.as_ptr(), flags)
    }
}

fn start_write(hwnd: isize) {
    unsafe {
        let iso = match ISO_PATH.clone() {
            Some(p) => p,
            None => return,
        };
        let drive = match SELECTED_DRIVE.clone() {
            Some(d) => d,
            None => return,
        };

        let file_name = std::path::Path::new(&iso)
            .file_name()
            .map(|n| n.to_string_lossy().to_string())
            .unwrap_or_else(|| iso.clone());

        let msg = format!(
            "You are about to write:\n\n  {}\n\nto USB drive {}:\n\n\
             ⚠ ALL DATA on this USB drive will be PERMANENTLY ERASED.\n\n\
             Continue?",
            file_name, drive
        );

        if msg_box(hwnd, &msg, "Confirm Write", MB_YESNO | MB_ICONWARNING) != IDYES {
            return;
        }

        IS_WRITING = true;
        update_write_button(hwnd);

        std::thread::spawn(move || {
            write_iso(hwnd, &iso, &drive);
        });
    }
}

fn write_iso(hwnd: isize, iso_path: &str, drive_letter: &str) {
    unsafe {
        set_control_text(hwnd, ID_STATUS_LABEL, "Opening USB drive for raw write...");
        set_control_text(hwnd, ID_SPEED_LABEL, "");

        let device_path = format!("\\\\.\\{}:", drive_letter);
        let path_w = to_wide(&device_path);
        let handle = CreateFileW(
            path_w.as_ptr(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            ptr::null(),
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0,
        );

        if handle == INVALID_HANDLE_VALUE {
            set_control_text(hwnd, ID_STATUS_LABEL,
                &format!("Error: Cannot open drive {} (run as Administrator)", drive_letter));
            IS_WRITING = false;
            update_write_button(hwnd);
            msg_box(hwnd,
                &format!("Cannot open drive {}:\n\nMake sure you run NovatOSBurner.exe as Administrator.", drive_letter),
                "Error", MB_OK | MB_ICONERROR);
            return;
        }

        let mut iso = match std::fs::File::open(iso_path) {
            Ok(f) => f,
            Err(e) => {
                let _ = CloseHandle(handle);
                set_control_text(hwnd, ID_STATUS_LABEL, &format!("Error: Cannot open ISO: {}", e));
                IS_WRITING = false;
                update_write_button(hwnd);
                msg_box(hwnd, &format!("Cannot open ISO file:\n\n{}", e), "Error", MB_OK | MB_ICONERROR);
                return;
            }
        };

        let iso_size = match iso.metadata() {
            Ok(m) => m.len(),
            Err(_) => {
                let _ = CloseHandle(handle);
                set_control_text(hwnd, ID_STATUS_LABEL, "Error: Cannot get ISO size");
                IS_WRITING = false;
                update_write_button(hwnd);
                return;
            }
        };

        let progress = find_control(hwnd, ID_PROGRESS);
        SendMessageW(progress, PBM_SETRANGE32 as u32, 0, 10000);
        SendMessageW(progress, PBM_SETPOS as u32, 0, 0);

        let chunk_size = 4 * 1024 * 1024usize;
        let mut buf = vec![0u8; chunk_size];
        let mut written: u64 = 0;
        let start = std::time::Instant::now();

        set_control_text(hwnd, ID_STATUS_LABEL, "Writing...");

        loop {
            let n = match std::io::Read::read(&mut iso, &mut buf) {
                Ok(0) => break,
                Ok(n) => n,
                Err(e) => {
                    let _ = CloseHandle(handle);
                    set_control_text(hwnd, ID_STATUS_LABEL, &format!("Error reading ISO: {}", e));
                    IS_WRITING = false;
                    update_write_button(hwnd);
                    msg_box(hwnd, &format!("Read error:\n\n{}", e), "Error", MB_OK | MB_ICONERROR);
                    return;
                }
            };

            let mut bytes_written: u32 = 0;
            let ok = WriteFile(handle, buf.as_ptr() as *const _, n as u32, &mut bytes_written, ptr::null_mut());

            if ok == 0 {
                let _ = CloseHandle(handle);
                set_control_text(hwnd, ID_STATUS_LABEL, &format!("Error writing at offset {}", written));
                IS_WRITING = false;
                update_write_button(hwnd);
                msg_box(hwnd,
                    &format!("Write failed at offset {}.\n\nThe USB drive may be write-protected or disconnected.", written),
                    "Error", MB_OK | MB_ICONERROR);
                return;
            }

            written += bytes_written as u64;
            let elapsed = start.elapsed().as_secs_f64();
            let speed = if elapsed > 0.0 { written as f64 / elapsed } else { 0.0 };
            let pct = (written as f64 / iso_size as f64 * 10000.0) as usize;

            SendMessageW(progress, PBM_SETPOS as u32, pct, 0);
            set_control_text(hwnd, ID_STATUS_LABEL,
                &format!("Writing... {} / {}", format_size(written), format_size(iso_size)));
            set_control_text(hwnd, ID_SPEED_LABEL, &format!("{:.1} MB/s", speed / 1024.0 / 1024.0));
        }

        set_control_text(hwnd, ID_STATUS_LABEL, "Write complete. Verifying...");

        if !verify_iso(&mut iso, handle, iso_size) {
            let _ = CloseHandle(handle);
            set_control_text(hwnd, ID_STATUS_LABEL, "Error: Verification failed!");
            IS_WRITING = false;
            update_write_button(hwnd);
            msg_box(hwnd, "Verification failed — data on USB does not match ISO.\nThe USB drive may be faulty.",
                "Error", MB_OK | MB_ICONERROR);
            return;
        }

        let _ = CloseHandle(handle);

        set_control_text(hwnd, ID_STATUS_LABEL, "✓ Done! USB drive is bootable.");
        SendMessageW(progress, PBM_SETPOS as u32, 10000, 0);
        IS_WRITING = false;
        update_write_button(hwnd);

        msg_box(hwnd, "NovatOS ISO written successfully!\n\nYou can now boot from this USB drive.",
            "Success", MB_OK | MB_ICONINFORMATION);
    }
}

fn verify_iso(iso: &mut std::fs::File, handle: isize, iso_size: u64) -> bool {
    unsafe {
        let samples: Vec<(u64, u64)> = vec![
            (0, 1024 * 1024),
            (iso_size / 2, 1024 * 1024),
            (iso_size.saturating_sub(1024 * 1024), 1024 * 1024),
        ];

        let mut iso_buf = vec![0u8; 1024 * 1024];
        let mut usb_buf = vec![0u8; 1024 * 1024];

        for (offset, size) in samples {
            if size == 0 {
                continue;
            }
            use std::io::{Read, Seek, SeekFrom};
            if iso.seek(SeekFrom::Start(offset)).is_err() {
                continue;
            }
            if iso.read(&mut iso_buf[..size as usize]).is_err() {
                continue;
            }

            // Seek on device handle
            extern "system" {
                fn SetFilePointerEx(hFile: isize, liDistanceToMove: i64,
                    lpNewFilePointer: *mut i64, dwMoveMethod: u32) -> i32;
            }
            if SetFilePointerEx(handle, offset as i64, ptr::null_mut(), 0) == 0 {
                return false;
            }
            let mut bytes_read: u32 = 0;
            if ReadFile(handle, usb_buf.as_mut_ptr() as *mut _, size as u32,
                        &mut bytes_read, ptr::null_mut()) == 0 {
                return false;
            }
            if iso_buf[..size as usize] != usb_buf[..size as usize] {
                return false;
            }
        }
        true
    }
}

// ─── Main ───
fn main() {
    unsafe {
        let class_name = to_wide("NovatOSBurner");

        let wc = WNDCLASSW {
            lpfnWndProc: Some(wnd_proc),
            lpszClassName: class_name.as_ptr(),
            hbrBackground: COLOR_BTNFACE as *mut _,
            hCursor: LoadCursorW(0, IDC_ARROW),
            ..Default::default()
        };

        RegisterClassW(&wc);

        // Init common controls (progress bar)
        extern "system" {
            fn InitCommonControls();
        }
        InitCommonControls();

        let title = to_wide("NovatOS USB Burner");
        let hwnd = CreateWindowExW(
            0,
            class_name.as_ptr(),
            title.as_ptr(),
            WS_OVERLAPPEDWINDOW & !0x00040000 & !0x00020000, // no maximize, no thickframe
            CW_USEDEFAULT, CW_USEDEFAULT,
            640, 600,
            0, 0, 0, ptr::null_mut(),
        );

        if hwnd == 0 {
            return;
        }

        // Header
        create_label(hwnd, 0, "NovatOS USB Burner", 20, 15, 400, 24);
        create_label(hwnd, 0, "Flash NovatOS ISO to a USB drive (DD mode)", 20, 38, 400, 18);

        // ISO selection
        create_label(hwnd, 0, "ISO File:", 20, 75, 100, 18);
        create_label(hwnd, ID_ISO_LABEL, "No ISO selected", 20, 95, 440, 20);
        create_button(hwnd, ID_ISO_BUTTON, "Browse...", 470, 92, 130, 26);

        // Drive selection
        create_label(hwnd, 0, "USB Drive:", 20, 135, 100, 18);
        create_listbox(hwnd, ID_DRIVE_LIST, 20, 155, 440, 120);
        create_button(hwnd, ID_REFRESH_BUTTON, "Refresh", 470, 155, 130, 26);

        // Progress
        create_label(hwnd, ID_STATUS_LABEL, "Ready", 20, 295, 580, 20);
        create_progress(hwnd, ID_PROGRESS, 20, 315, 580, 24);
        create_label(hwnd, ID_SPEED_LABEL, "", 20, 343, 580, 18);

        // Write button
        let write_btn = create_button(hwnd, ID_WRITE_BUTTON, "Write to USB", 220, 380, 200, 40);

        // Warning
        create_label(hwnd, 0, "⚠ All data on the selected USB drive will be permanently erased.", 20, 440, 580, 20);

        EnableWindow(write_btn, 0);

        refresh_drives(hwnd);

        ShowWindow(hwnd, SW_SHOW);

        let mut msg = std::mem::zeroed();
        while GetMessageW(&mut msg, 0, 0, 0) > 0 {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}
