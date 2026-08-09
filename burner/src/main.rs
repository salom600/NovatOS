// NovatOS USB Burner — native Windows .exe (Rust)
// =================================================
// Flash NovatOS ISO to USB drives in DD (raw) mode.
// No runtime dependencies — single .exe file.
//
// Build: cargo build --release
// Output: target/release/NovatOSBurner.exe

#![windows_subsystem = "windows"]

use std::ffi::OsStr;
use std::os::windows::ffi::OsStrExt;
use std::ptr;

use windows::Win32::Foundation::{HANDLE, INVALID_HANDLE_VALUE, CloseHandle};
use windows::Win32::Storage::FileSystem::{
    CreateFileW, ReadFile, WriteFile, OPEN_EXISTING,
    FILE_SHARE_READ, FILE_SHARE_WRITE, GENERIC_READ, GENERIC_WRITE,
    FILE_ATTRIBUTE_NORMAL, GetFileSizeEx,
};
use windows::Win32::Storage::DeviceIO::DeviceIoControl;
use windows::Win32::System::Threading::{CreateThread, WaitForSingleObject};
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, DefWindowProcW, DispatchMessageW, GetMessageW, PostQuitMessage,
    RegisterClassW, SendMessageW, SetWindowTextW, TranslateMessage, MessageBoxW,
    MB_OK, MB_YESNO, MB_ICONWARNING, MB_ICONERROR, MB_ICONINFORMATION,
    WM_COMMAND, WM_DESTROY, WM_PAINT, CW_USEDEFAULT, WS_OVERLAPPEDWINDOW,
    WS_VISIBLE, WS_CHILD, WS_BORDER, BS_PUSHBUTTON, LB_ADDSTRING, LB_GETCURSEL,
    LB_RESETCONTENT, BM_SETSTATE, PBS_SMOOTH, WM_HSCROLL,
    PBM_SETPOS, PBM_SETRANGE32, PBM_SETSTEP,
};
use windows::Win32::UI::Controls::{InitCommonControls, PROGRESS_CLASSW};
use windows::core::{PCWSTR, HSTRING};

// Window control IDs
const ID_ISO_BUTTON: u32 = 1001;
const ID_DRIVE_LIST: u32 = 1002;
const ID_REFRESH_BUTTON: u32 = 1003;
const ID_WRITE_BUTTON: u32 = 1004;
const ID_ISO_LABEL: u32 = 1005;
const ID_PROGRESS: u32 = 1006;
const ID_STATUS_LABEL: u32 = 1007;
const ID_SPEED_LABEL: u32 = 1008;

// Global state (simple — single-threaded UI, background write thread)
static mut ISO_PATH: Option<String> = None;
static mut SELECTED_DRIVE: Option<String> = None;
static mut IS_WRITING: bool = false;
static mut MAIN_HWND: isize = 0;

fn to_wide(s: &str) -> Vec<u16> {
    OsStr::new(s).encode_wide().chain(std::iter::once(0)).collect()
}

fn from_wide(wide: &[u16]) -> String {
    let len = wide.iter().position(|&c| c == 0).unwrap_or(wide.len());
    String::from_utf16_lossy(&wide[..len])
}

// ─── Window procedure ───
extern "system" fn wnd_proc(hwnd: isize, msg: u32, wp: usize, lp: isize) -> isize {
    unsafe {
        match msg {
            WM_COMMAND => {
                let ctrl_id = (wp & 0xFFFF) as u32;
                let notification = ((wp >> 16) & 0xFFFF) as u32;
                match ctrl_id {
                    ID_ISO_BUTTON => {
                        open_iso_dialog(hwnd);
                    }
                    ID_REFRESH_BUTTON => {
                        refresh_drives(hwnd);
                    }
                    ID_WRITE_BUTTON => {
                        start_write(hwnd);
                    }
                    ID_DRIVE_LIST if notification == 1 => {
                        // LBN_SELCHANGE
                        on_drive_select(hwnd);
                    }
                    _ => {}
                }
                0
            }
            WM_DESTROY => {
                PostQuitMessage(0);
                0
            }
            WM_PAINT => {
                // Default paint — background is set by the window class
                DefWindowProcW(hwnd, msg, wp, lp)
            }
            _ => DefWindowProcW(hwnd, msg, wp, lp),
        }
    }
}

// ─── Create a child button ───
fn create_button(hwnd_parent: isize, id: u32, text: &str, x: i32, y: i32, w: i32, h: i32) -> isize {
    unsafe {
        let class = to_wide("BUTTON");
        let text_w = to_wide(text);
        CreateWindowExW(
            0,
            PCWSTR(class.as_ptr()),
            PCWSTR(text_w.as_ptr()),
            (WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON).0 as u32,
            x, y, w, h,
            hwnd_parent,
            id as _,
            None,
            ptr::null(),
        ).expect("Failed to create button").0
    }
}

// ─── Create a static text label ───
fn create_label(hwnd_parent: isize, id: u32, text: &str, x: i32, y: i32, w: i32, h: i32) -> isize {
    unsafe {
        let class = to_wide("STATIC");
        let text_w = to_wide(text);
        CreateWindowExW(
            0,
            PCWSTR(class.as_ptr()),
            PCWSTR(text_w.as_ptr()),
            (WS_CHILD | WS_VISIBLE).0 as u32,
            x, y, w, h,
            hwnd_parent,
            id as _,
            None,
            ptr::null(),
        ).expect("Failed to create label").0
    }
}

// ─── Create a listbox ───
fn create_listbox(hwnd_parent: isize, id: u32, x: i32, y: i32, w: i32, h: i32) -> isize {
    unsafe {
        let class = to_wide("LISTBOX");
        CreateWindowExW(
            0,
            PCWSTR(class.as_ptr()),
            PCWSTR(to_wide("").as_ptr()),
            (WS_CHILD | WS_VISIBLE | WS_BORDER | 0x00100000 /* LBS_NOTIFY */).0 as u32,
            x, y, w, h,
            hwnd_parent,
            id as _,
            None,
            ptr::null(),
        ).expect("Failed to create listbox").0
    }
}

// ─── Create a progress bar ───
fn create_progress(hwnd_parent: isize, id: u32, x: i32, y: i32, w: i32, h: i32) -> isize {
    unsafe {
        let text_w = to_wide("");
        CreateWindowExW(
            0,
            PCWSTR(PROGRESS_CLASSW.as_ptr()),
            PCWSTR(text_w.as_ptr()),
            (WS_CHILD | WS_VISIBLE | PBS_SMOOTH).0 as u32,
            x, y, w, h,
            hwnd_parent,
            id as _,
            None,
            ptr::null(),
        ).expect("Failed to create progress bar").0
    }
}

// ─── File dialog (open ISO) ───
fn open_iso_dialog(hwnd: isize) {
    // Use GetOpenFileNameW
    use windows::Win32::UI::WindowsAndMessaging::{GetOpenFileNameW, OPENFILENAMEW};
    use std::mem::MaybeUninit;

    unsafe {
        let mut file_buf = [0u16; 260];
        let filter = to_wide("ISO Files\0*.iso\0All Files\0*.*\0\0");
        let title = to_wide("Select NovatOS ISO");

        let mut ofn: OPENFILENAMEW = std::mem::zeroed();
        ofn.lStructSize = std::mem::size_of::<OPENFILENAMEW>() as u32;
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = PCWSTR(filter.as_ptr());
        ofn.lpstrFile = windows::core::PWSTR(file_buf.as_mut_ptr());
        ofn.nMaxFile = file_buf.len() as u32;
        ofn.lpstrTitle = PCWSTR(title.as_ptr());
        ofn.Flags = 0x00001000 | 0x00000004; // OFN_FILEMUSTEXIST | OFN_HIDEREADONLY

        if GetOpenFileNameW(&mut ofn as *mut OPENFILENAMEW) {
            let path = from_wide(&file_buf);
            ISO_PATH = Some(path.clone());

            // Update label
            let label_text = if path.len() > 60 {
                format!("...{}", &path[path.len()-57..])
            } else {
                path
            };
            let label_w = to_wide(&label_text);
            let _ = SendMessageW(
                windows::core::HWND(find_control(hwnd, ID_ISO_LABEL)),
                0x000C, // WM_SETTEXT
                0,
                label_w.as_ptr() as _,
            );
            update_write_button(hwnd);
        }
    }
}

// ─── Find a child control by ID ───
fn find_control(hwnd: isize, id: u32) -> isize {
    unsafe {
        use windows::Win32::UI::WindowsAndMessaging::GetDlgItem;
        GetDlgItem(windows::core::HWND(hwnd), id as i32).map(|h| h.0).unwrap_or(0)
    }
}

// ─── Refresh USB drives ───
fn refresh_drives(hwnd: isize) {
    unsafe {
        let listbox = find_control(hwnd, ID_DRIVE_LIST);
        let _ = SendMessageW(windows::core::HWND(listbox), LB_RESETCONTENT, 0, 0);

        // Get logical drives
        use windows::Win32::Storage::FileSystem::GetLogicalDrives;
        let drives = GetLogicalDrives();

        for i in 0..26u32 {
            if drives & (1 << i) != 0 {
                let letter = (b'A' + i as u8) as char;
                let drive_root = format!("{}:\\", letter);

                // Check if removable
                let root_w = to_wide(&drive_root);
                use windows::Win32::Storage::FileSystem::GetDriveTypeW;
                let drive_type = GetDriveTypeW(PCWSTR(root_w.as_ptr()));

                if drive_type == 2 { // DRIVE_REMOVABLE
                    // Get drive size
                    let device_path = format!("\\\\.\\{}:", letter);
                    let size = get_drive_size(&device_path);
                    let size_str = format_size(size);
                    let label = format!("{}:  —  {}", letter, size_str);
                    let label_w = to_wide(&label);
                    let _ = SendMessageW(
                        windows::core::HWND(listbox),
                        LB_ADDSTRING,
                        0,
                        label_w.as_ptr() as _,
                    );
                }
            }
        }
    }
}

// ─── Get drive size via DeviceIoControl ───
fn get_drive_size(device_path: &str) -> u64 {
    unsafe {
        let path_w = to_wide(device_path);
        let handle = CreateFileW(
            PCWSTR(path_w.as_ptr()),
            GENERIC_READ.0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            None,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            None,
        );

        if handle.is_err() || handle.unwrap() == INVALID_HANDLE_VALUE {
            return 0;
        }

        let handle = handle.unwrap();

        // IOCTL_DISK_GET_DRIVE_GEOMETRY_EX = 0x000700A0
        // Use a simple approach: GetFileSizeEx works on the device handle
        let mut size: i64 = 0;
        let _ = GetFileSizeEx(handle, &mut size);

        let _ = CloseHandle(handle);
        size as u64
    }
}

// ─── Format size as human-readable ───
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

// ─── Drive selection ───
fn on_drive_select(hwnd: isize) {
    unsafe {
        let listbox = find_control(hwnd, ID_DRIVE_LIST);
        let sel = SendMessageW(windows::core::HWND(listbox), LB_GETCURSEL, 0, 0);
        if sel >= 0 {
            let letter = (b'A' + sel as u8) as char;
            SELECTED_DRIVE = Some(letter.to_string());
            update_write_button(hwnd);
        }
    }
}

// ─── Enable/disable write button ───
fn update_write_button(hwnd: isize) {
    unsafe {
        let btn = find_control(hwnd, ID_WRITE_BUTTON);
        let enabled = ISO_PATH.is_some() && SELECTED_DRIVE.is_some() && !IS_WRITING;
        let _ = SendMessageW(
            windows::core::HWND(btn),
            BM_SETSTATE as _,
            0, // not pressed
            0,
        );
        // EnableWindow
        use windows::Win32::UI::WindowsAndMessaging::EnableWindow;
        let _ = EnableWindow(windows::core::HWND(btn), enabled);
    }
}

// ─── Start write process ───
fn start_write(hwnd: isize) {
    unsafe {
        let iso = match &ISO_PATH {
            Some(p) => p.clone(),
            None => return,
        };
        let drive = match &SELECTED_DRIVE {
            Some(d) => d.clone(),
            None => return,
        };

        // Confirm
        let msg = to_wide(&format!(
            "You are about to write:\n\n  {}\n\nto USB drive {}:\n\n\
             ⚠ ALL DATA on this USB drive will be PERMANENTLY ERASED.\n\n\
             Continue?",
            std::path::Path::new(&iso).file_name()
                .map(|n| n.to_string_lossy().to_string())
                .unwrap_or_else(|| iso.clone()),
            drive
        ));
        let title = to_wide("Confirm Write");

        let result = MessageBoxW(
            windows::core::HWND(hwnd),
            PCWSTR(msg.as_ptr()),
            PCWSTR(title.as_ptr()),
            MB_YESNO | MB_ICONWARNING,
        );

        if result.0 != 6 { // IDYES
            return;
        }

        // Disable button, start thread
        IS_WRITING = true;
        update_write_button(hwnd);

        let iso_clone = iso.clone();
        let drive_clone = drive.clone();
        let hwnd_thread = hwnd;

        std::thread::spawn(move || {
            write_iso(hwnd_thread, &iso_clone, &drive_clone);
        });
    }
}

// ─── Write ISO to USB drive ───
fn write_iso(hwnd: isize, iso_path: &str, drive_letter: &str) {
    unsafe {
        // Update status
        set_status(hwnd, "Opening USB drive for raw write...");

        // Open the physical drive for raw write
        let device_path = format!("\\\\.\\{}:", drive_letter);
        let path_w = to_wide(&device_path);
        let handle = CreateFileW(
            PCWSTR(path_w.as_ptr()),
            GENERIC_READ.0 | GENERIC_WRITE.0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            None,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            None,
        );

        if handle.is_err() || handle.unwrap() == INVALID_HANDLE_VALUE {
            set_status(hwnd, &format!("Error: Cannot open drive {} (run as Administrator)", drive_letter));
            IS_WRITING = false;
            update_write_button(hwnd);
            show_error(hwnd, &format!("Cannot open drive {}:\n\nMake sure you run NovatOSBurner.exe as Administrator.", drive_letter));
            return;
        }
        let handle = handle.unwrap();

        // Open ISO file
        let mut iso = match std::fs::File::open(iso_path) {
            Ok(f) => f,
            Err(e) => {
                let _ = CloseHandle(handle);
                set_status(hwnd, &format!("Error: Cannot open ISO: {}", e));
                IS_WRITING = false;
                update_write_button(hwnd);
                show_error(hwnd, &format!("Cannot open ISO file:\n\n{}", e));
                return;
            }
        };

        let iso_size = match iso.metadata() {
            Ok(m) => m.len(),
            Err(_) => {
                let _ = CloseHandle(handle);
                set_status(hwnd, "Error: Cannot get ISO size");
                IS_WRITING = false;
                update_write_button(hwnd);
                return;
            }
        };

        // Set progress range
        let progress = find_control(hwnd, ID_PROGRESS);
        let _ = SendMessageW(windows::core::HWND(progress), PBM_SETRANGE32, 0, 10000);
        let _ = SendMessageW(windows::core::HWND(progress), PBM_SETPOS, 0, 0);

        // Write in 4MB chunks
        let chunk_size = 4 * 1024 * 1024usize;
        let mut buf = vec![0u8; chunk_size];
        let mut written: u64 = 0;
        let start = std::time::Instant::now();

        set_status(hwnd, "Writing...");

        loop {
            let n = match std::io::Read::read(&mut iso, &mut buf) {
                Ok(0) => break,
                Ok(n) => n,
                Err(e) => {
                    let _ = CloseHandle(handle);
                    set_status(hwnd, &format!("Error reading ISO: {}", e));
                    IS_WRITING = false;
                    update_write_button(hwnd);
                    show_error(hwnd, &format!("Read error:\n\n{}", e));
                    return;
                }
            };

            let mut bytes_written: u32 = 0;
            let ok = WriteFile(
                handle,
                Some(&buf[..n]),
                &mut bytes_written,
                None,
            );

            if ok.is_err() {
                let _ = CloseHandle(handle);
                set_status(hwnd, &format!("Error writing at offset {}", written));
                IS_WRITING = false;
                update_write_button(hwnd);
                show_error(hwnd, &format!("Write failed at offset {}.\n\nThe USB drive may be write-protected or disconnected.", written));
                return;
            }

            written += bytes_written as u64;
            let elapsed = start.elapsed().as_secs_f64();
            let speed = if elapsed > 0.0 { written as f64 / elapsed } else { 0.0 };
            let pct = (written as f64 / iso_size as f64 * 10000.0) as usize;

            let _ = SendMessageW(windows::core::HWND(progress), PBM_SETPOS, pct, 0);
            set_status(hwnd, &format!("Writing... {} / {}", format_size(written), format_size(iso_size)));
            set_speed(hwnd, &format!("{:.1} MB/s", speed / 1024.0 / 1024.0));
        }

        set_status(hwnd, "Write complete. Verifying...");

        // Verify (sample: first 1MB, middle 1MB, last 1MB)
        if !verify_iso(&mut iso, handle, iso_size) {
            let _ = CloseHandle(handle);
            set_status(hwnd, "Error: Verification failed!");
            IS_WRITING = false;
            update_write_button(hwnd);
            show_error(hwnd, "Verification failed — data on USB does not match ISO.\nThe USB drive may be faulty.");
            return;
        }

        let _ = CloseHandle(handle);

        set_status(hwnd, "✓ Done! USB drive is bootable.");
        let _ = SendMessageW(windows::core::HWND(progress), PBM_SETPOS, 10000, 0);
        IS_WRITING = false;
        update_write_button(hwnd);

        // Success message
        let msg = to_wide("NovatOS ISO written successfully!\n\nYou can now boot from this USB drive.");
        let title = to_wide("Success");
        MessageBoxW(
            windows::core::HWND(hwnd),
            PCWSTR(msg.as_ptr()),
            PCWSTR(title.as_ptr()),
            MB_OK | MB_ICONINFORMATION,
        );
    }
}

// ─── Verify ISO by sample comparison ───
fn verify_iso(iso: &mut std::fs::File, handle: HANDLE, iso_size: u64) -> bool {
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
            // Read from ISO
            use std::io::{Seek, SeekFrom, Read};
            if iso.seek(SeekFrom::Start(offset)).is_err() {
                continue;
            }
            if iso.read(&mut iso_buf[..size as usize]).is_err() {
                continue;
            }

            // Read from USB
            use windows::Win32::Storage::FileSystem::SetFilePointerEx;
            if SetFilePointerEx(handle, offset as i64, None, 0).is_err() {
                return false;
            }
            let mut bytes_read: u32 = 0;
            if ReadFile(handle, Some(&mut usb_buf[..size as usize]), &mut bytes_read, None).is_err() {
                return false;
            }
            if iso_buf[..size as usize] != usb_buf[..size as usize] {
                return false;
            }
        }
        true
    }
}

// ─── Set status label ───
fn set_status(hwnd: isize, text: &str) {
    unsafe {
        let label = find_control(hwnd, ID_STATUS_LABEL);
        let text_w = to_wide(text);
        let _ = SendMessageW(
            windows::core::HWND(label),
            0x000C, // WM_SETTEXT
            0,
            text_w.as_ptr() as _,
        );
    }
}

fn set_speed(hwnd: isize, text: &str) {
    unsafe {
        let label = find_control(hwnd, ID_SPEED_LABEL);
        let text_w = to_wide(text);
        let _ = SendMessageW(
            windows::core::HWND(label),
            0x000C, // WM_SETTEXT
            0,
            text_w.as_ptr() as _,
        );
    }
}

fn show_error(hwnd: isize, msg: &str) {
    unsafe {
        let msg_w = to_wide(msg);
        let title = to_wide("Error");
        MessageBoxW(
            windows::core::HWND(hwnd),
            PCWSTR(msg_w.as_ptr()),
            PCWSTR(title.as_ptr()),
            MB_OK | MB_ICONERROR,
        );
    }
}

// ─── Main entry point ───
fn main() {
    unsafe {
        // Register window class
        let class_name = to_wide("NovatOSBurner");

        let wc = windows::Win32::UI::WindowsAndMessaging::WNDCLASSW {
            lpfnWndProc: Some(wnd_proc),
            lpszClassName: PCWSTR(class_name.as_ptr()),
            hbrBackground: windows::Win32::Graphics::Gdi::HBRUSH(15), // COLOR_BTNFACE
            hCursor: {
                use windows::Win32::UI::WindowsAndMessaging::LoadCursorW;
                use windows::Win32::UI::WindowsAndMessaging::IDC_ARROW;
                LoadCursorW(None, IDC_ARROW).unwrap()
            },
            ..Default::default()
        };

        RegisterClassW(&wc);

        // Init common controls (for progress bar)
        let _ = InitCommonControls();

        // Create main window
        let title = to_wide("NovatOS USB Burner");
        let hwnd = CreateWindowExW(
            0,
            PCWSTR(class_name.as_ptr()),
            PCWSTR(title.as_ptr()),
            (WS_OVERLAPPEDWINDOW & !WS_MAXIMIZEBOX & !WS_THICKFRAME).0 as u32,
            CW_USEDEFAULT, CW_USEDEFAULT,
            640, 600,
            None,
            None,
            None,
            ptr::null(),
        ).expect("Failed to create window");

        MAIN_HWND = hwnd.0;

        // Set dark background via WM_ERASEBKGND would need subclassing; keep default for now

        // Create child controls
        let _ = create_label(hwnd.0, 0, "NovatOS USB Burner", 20, 15, 400, 24);
        let _ = create_label(hwnd.0, 0, "Flash NovatOS ISO to a USB drive (DD mode)", 20, 38, 400, 18);

        // ISO selection
        let _ = create_label(hwnd.0, 0, "ISO File:", 20, 75, 100, 18);
        let _ = create_label(hwnd.0, ID_ISO_LABEL, "No ISO selected", 20, 95, 440, 20);
        let _ = create_button(hwnd.0, ID_ISO_BUTTON, "Browse...", 470, 92, 130, 26);

        // Drive selection
        let _ = create_label(hwnd.0, 0, "USB Drive:", 20, 135, 100, 18);
        let _ = create_listbox(hwnd.0, ID_DRIVE_LIST, 20, 155, 440, 120);
        let _ = create_button(hwnd.0, ID_REFRESH_BUTTON, "Refresh", 470, 155, 130, 26);

        // Progress
        let _ = create_label(hwnd.0, ID_STATUS_LABEL, "Ready", 20, 295, 580, 20);
        let _ = create_progress(hwnd.0, ID_PROGRESS, 20, 315, 580, 24);
        let _ = create_label(hwnd.0, ID_SPEED_LABEL, "", 20, 343, 580, 18);

        // Write button
        let write_btn = create_button(hwnd.0, ID_WRITE_BUTTON, "Write to USB", 220, 380, 200, 40);

        // Warning
        let _ = create_label(hwnd.0, 0, "⚠ All data on the selected USB drive will be permanently erased.", 20, 440, 580, 20);

        // Disable write button initially
        use windows::Win32::UI::WindowsAndMessaging::EnableWindow;
        let _ = EnableWindow(windows::core::HWND(write_btn), false);

        // Initial drive scan
        refresh_drives(hwnd.0);

        // Show window
        use windows::Win32::UI::WindowsAndMessaging::ShowWindow;
        ShowWindow(hwnd, windows::Win32::UI::WindowsAndMessaging::SW_SHOW);

        // Message loop
        let mut msg = std::mem::zeroed();
        while GetMessageW(&mut msg, None, 0, 0).into() {
            let _ = TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}
