#Requires AutoHotkey v2.0
Persistent()

; 🏆 REMOVED THE ADMIN FORCE RUN CHECK BLOCK FROM HERE 
; Script will now launch instantly without any Windows UAC permission prompts!

; --- HARDWARE SETUP ---
BaudRate := 115200
ComPort  := ""   ; 🔄 auto-detected at runtime (no longer hard-coded to COM5)

; --- CREATE VISUAL CONTROL WINDOW ---
MyGui := Gui("+AlwaysOnTop -MaximizeBox", "ESP32 Macro Pad Controller")
MyGui.SetFont("s10", "Segoe UI")

; Visual text status trackers
PortLabel   := MyGui.AddText("w250", "Target Port: searching... (" BaudRate " Baud)")
StatusLabel := MyGui.AddText("w250 r1 cBlue", "Active Profile: Waiting...")

; Button to instantly close the app and free the COM port for PlatformIO uploads
ExitBtn := MyGui.AddButton("w250", "Stop Automation & Release Port")
ExitBtn.OnEvent("Click", (*) => ExitApp())

; Handle the window's top right close button (X) cleanly
MyGui.OnEvent("Close", (*) => ExitApp())

; Draw and reveal the control board on screen
MyGui.Show("w280 h120")

LastProfile := ""

; 🔄 Keep an eye on the device: connect when found, reconnect if it drops out
SetTimer(MonitorConnection, 1500)
MonitorConnection()   ; try to connect immediately on boot

; Start scanning the foreground window state every 500ms
SetTimer(CheckActiveWindow, 500)

; ------------------------------------------------------------------
; 🔄 CONNECTION MANAGEMENT  (NEW)
; ------------------------------------------------------------------
MonitorConnection() {
    global ComPort, LastProfile, BaudRate

    if (ComPort != "") {
        ; We think we're connected — make sure the device is still plugged in
        if !PortStillPresent(ComPort) {
            ComPort     := ""
            LastProfile := ""
            StatusLabel.Value := "Active Profile: Connection lost, searching..."
            PortLabel.Value   := "Target Port: searching... (" BaudRate " Baud)"
        }
        return
    }

    ; Not connected — look for the ESP32 on any COM port
    found := FindESP32Port()
    if (found != "") {
        ; Configure the freshly found port exactly like before
        RunWait(A_ComSpec ' /c mode ' found ': baud=' BaudRate ' data=8 parity=n stop=1', , "Hide")
        ComPort     := found
        LastProfile := ""   ; force the current app's profile to be re-sent on reconnect
        PortLabel.Value   := "Target Port: " found " (" BaudRate " Baud)"
        StatusLabel.Value := "Active Profile: Connected on " found
        TrayTip "ESP32 Auto Pad", "Connected on " found, 1
    }
}

; Find which COM port the ESP32's USB-serial chip is sitting on, via WMI.
; Matches the common ESP32 bridge chips by USB vendor ID (VID).
FindESP32Port() {
    knownVIDs := ["VID_10C4"   ; Silicon Labs CP210x (CP2102/CP2104)
                , "VID_1A86"   ; WCH CH340 / CH9102
                , "VID_303A"   ; Espressif native USB (S2/S3/C3)
                , "VID_0403"   ; FTDI
                , "VID_067B"]  ; Prolific
    try {
        wmi := ComObject("WbemScripting.SWbemLocator").ConnectServer()
        query := "SELECT Name, PNPDeviceID FROM Win32_PnPEntity WHERE Name LIKE '%(COM%'"
        for dev in wmi.ExecQuery(query) {
            for vid in knownVIDs {
                if InStr(dev.PNPDeviceID, vid) {
                    if RegExMatch(dev.Name, "\(COM(\d+)\)", &m)
                        return "COM" m[1]
                }
            }
        }
    }
    return ""   ; nothing found / WMI hiccup — caller keeps searching
}

; Quick, cheap check that a COM port is still registered with Windows
PortStillPresent(comName) {
    try {
        Loop Reg "HKLM\HARDWARE\DEVICEMAP\SERIALCOMM" {
            if (A_LoopRegType = "REG_SZ" && RegRead() = comName)
                return true
        }
    }
    return false
}

; ------------------------------------------------------------------
; EVERYTHING BELOW IS YOUR ORIGINAL LOGIC — only the "&& ComPort != ''"
; send-guard and the reconnect-on-failure catch block were added.
; ------------------------------------------------------------------
CheckActiveWindow() {
    global LastProfile
    
    try {
        ActiveProcess := WinGetProcessName("A")
    } catch {
        return 
    }

    TargetProfile := ""

    ; 🏆 STRICT CASE-SENSITIVE APP FILTERING & MAPPING
    if (ActiveProcess == "Code.exe") {
        TargetProfile := "VisualStudio"
    }
    else if (ActiveProcess == "Arduino IDE.exe") {
        TargetProfile := "Arduino"
    }
    else if (ActiveProcess == "brave.exe") {  
        TargetProfile := "YouTube"
    } else {
        TargetProfile := StrLower(ActiveProcess)
    }

    ; Only send if it matches an accepted app, the app changed, AND we're connected
    if (TargetProfile != "" && TargetProfile != LastProfile && ComPort != "") {
        SendProfileToESP32(TargetProfile)
        LastProfile := TargetProfile
        
        ; UPDATE WINDOW TEXT: Refreshes the blue label inside your window frame dynamically
        StatusLabel.Value := "Active Profile: " TargetProfile
    }
}

SendProfileToESP32(ProfileName) {
    global ComPort, LastProfile, BaudRate

    try {
        ; Use Windows direct namespace layout for low-level device control
        port := FileOpen("\\.\" ComPort, "w") 
        
        ; Write target packet string matching the ESP32 parser token
        port.Write("PROFILE:" . ProfileName . "`n")
        
        ; Force-flush the data packet out of the OS stack cache
        port.Read(0) 
        port.Close()
        
        ; Flash a taskbar notification to track automation events visually
        TrayTip "ESP32 Auto Pad", "Switched Layout To: " ProfileName, 1
    } catch Error as err {
        ; 🔄 Device likely unplugged / port busy: drop the connection so the
        ; monitor timer rescans and reconnects automatically (no blocking popup).
        ComPort     := ""
        LastProfile := ""
        StatusLabel.Value := "Active Profile: Connection lost, searching..."
        PortLabel.Value   := "Target Port: searching... (" BaudRate " Baud)"
    }
}
