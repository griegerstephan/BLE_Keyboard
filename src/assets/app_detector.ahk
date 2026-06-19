#Requires AutoHotkey v2.0
Persistent()

; 🏆 REMOVED THE ADMIN FORCE RUN CHECK BLOCK FROM HERE 
; Script will now launch instantly without any Windows UAC permission prompts!

; --- ONCE-OFF HARDWARE SETUP ---
ComPort := "COM5" 
BaudRate := 115200

; Run port setup exactly ONCE on boot to keep background loop unlocked
RunWait(A_ComSpec " /c mode " ComPort ": baud=" BaudRate " data=8 parity=n stop=1", , "Hide")

; --- CREATE VISUAL CONTROL WINDOW ---
MyGui := Gui("+AlwaysOnTop -MaximizeBox", "ESP32 Macro Pad Controller")
MyGui.SetFont("s10", "Segoe UI")

; Visual text status trackers
MyGui.AddText("w250", "Target Port: " ComPort " (" BaudRate " Baud)")
StatusLabel := MyGui.AddText("w250 r1 cBlue", "Active Profile: Waiting...")

; Button to instantly close the app and free COM5 for PlatformIO uploads
ExitBtn := MyGui.AddButton("w250", "Stop Automation & Release Port")
ExitBtn.OnEvent("Click", (*) => ExitApp())

; Handle the window's top right close button (X) cleanly
MyGui.OnEvent("Close", (*) => ExitApp())

; Draw and reveal the control board on screen
MyGui.Show("w280 h120")

; Start scanning the foreground window state every 500ms
SetTimer(CheckActiveWindow, 500) 
LastProfile := ""

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
    }

    ; Only send if it matches one of our accepted apps AND the app actually changed
    if (TargetProfile != "" && TargetProfile != LastProfile) {
        SendProfileToESP32(TargetProfile)
        LastProfile := TargetProfile
        
        ; UPDATE WINDOW TEXT: Refreshes the blue label inside your window frame dynamically
        StatusLabel.Value := "Active Profile: " TargetProfile
    }
}

SendProfileToESP32(ProfileName) {
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
        ; UPDATE WINDOW TEXT: Update layout message state to show the drop block context
        StatusLabel.Value := "Active Profile: ERROR (Port Locked)"
        
        ; 🔬 POP UP THE EXACT WINDOWS ERROR MESSAGE
        MsgBox("Serial Error Details:`n`n" 
             . "Message: " err.Message "`n" 
             . "Extra Info: " err.Extra "`n" 
             . "Port: " ComPort, "COM Port Failure", 16) ; 16 adds the red error icon
    }
}
