# BUDDY — AI Robot Companion VR Experience

A VR robotics lab experience built in **Unreal Engine 4.27.2** where a player explores a robotics lab while an AI robot companion called **BUDDY** follows them, answers questions, and describes interactive lab objects using real-time AI and text-to-speech.

---

## Project Purpose

BUDDY is designed as an interactive educational demo for a robotics lab environment. The player can walk around the lab, interact with robots and equipment, and ask BUDDY questions about them. BUDDY responds with AI-generated answers spoken aloud using a natural voice. The experience is built and tested on **PC (keyboard + mouse)**. VR input mappings for **Meta Quest via Oculus Link** have been implemented but have not been tested on a physical device.

---

## Tech Stack

| Component | Technology |
|---|---|
| Engine | Unreal Engine 4.27.2 |
| Language | C++ + Blueprints |
| AI | Groq API (llama-3.1-8b-instant) |
| Text-to-Speech | VoiceRSS API (English GB, Nancy voice, 44khz WAV) |
| Voice Input | Groq Whisper API |
| VR Target | Meta Quest 2 via Oculus Link (PC VR, not standalone) |

---

## Project Structure

```
BUDDY/
├── Source/
│   └── BUDDY/
│       ├── BuddyAIComponent.cpp
│       └── BuddyAIComponent.h
├── Content/
│   └── BUDDY/
│       ├── Blueprints/
│       │   ├── BP_VRPlayer
│       │   ├── BP_BuddyRobot
│       │   ├── BP_LabObject
│       │   ├── BP_EnterDoor
│       │   └── BP_ExitDoor
│       ├── Widgets/
│       │   ├── WBP_BuddyScreen
│       │   ├── WBP_EnterLab
│       │   └── WBP_ExitLab
│       ├── Furnitures/          # Lab furniture assets (imported from Fab)
│       ├── Materials/           # Wall and floor materials (sourced externally)
│       ├── Maps/
│       │   ├── RobotLab
│       │   └── Lobby
│       └── ABP_Buddy            # BUDDY animation blueprint (in root Content/BUDDY folder)
├── BUDDY.uproject
└── README.md
```

---

## Experience Scenario

This is the full flow of what happens when the experience runs:

1. **Player spawns in the Lobby** — a small room with a door leading to the robotics lab.

2. **Player approaches the door** — a button appears on screen saying "Enter Lab". The player clicks it (PC: mouse click / VR: look at button and press right trigger) and the screen fades, transitioning to the RobotLab level.

3. **Inside the lab** — BUDDY spawns in the level and positions itself to the front-left of the player. BUDDY follows the player around the room, staying to their front-left. BUDDY's chest panel is visible when close enough, showing BUDDY's responses and general question buttons.

4. **Asking questions via chest panel** — the player walks up to BUDDY so the chest panel is visible on screen. They click one of the general question buttons. BUDDY speaks the answer aloud and the text appears on the panel.

5. **Asking about a lab object** — the player walks near a robot or piece of equipment. When they point the center of their screen at the object and click (PC: left mouse button / VR: right trigger), BUDDY describes what the object is and what it does in robotics.

6. **Asking by voice** — the player holds V (PC) or the X button (VR left controller) to record their question, then releases to send it to BUDDY. BUDDY listens, transcribes, and responds.

7. **Exiting the lab** — the player walks near the exit door. A button appears saying "Exit Lab". They click it and transition back to the Lobby.

---

## Input Mappings

All VR and PC mappings are registered in **Project Settings → Input** so both modes work from the same Blueprint logic without any changes.

### Movement

| Action | PC | VR (Quest 2) |
|---|---|---|
| Move Forward | W | Left Thumbstick pushed forward |
| Move Backward | S | Left Thumbstick pulled back |
| Move Left | A | Left Thumbstick pushed left |
| Move Right | D | Left Thumbstick pushed right |
| Turn Left/Right | Mouse X (move mouse left/right) | Right Thumbstick X |
| Look Up/Down | Mouse Y (move mouse up/down) | Physical head movement — HMD handles this automatically |

### Interaction

| Action | PC | VR (Quest 2) | Notes |
|---|---|---|---|
| Interact with lab object | Left Mouse Button or E | Right Trigger | Point center of screen at object first |
| Click chest panel button | Left Mouse Button | Right Trigger | Walk close to BUDDY, look at button, press trigger |
| Click Enter/Exit Lab button | Left Mouse Button | Right Trigger | Walk near door so button appears, look at button, press trigger |
| Start voice recording | Hold V key | Hold X button (left controller) | Hold until done speaking |
| Stop recording + send to BUDDY | Release V key | Release X button | BUDDY will transcribe and respond |

### Important Notes on How to Interact

**Lab objects (PC):** Walk close to a robot or equipment. Move your mouse so the center of the screen is pointing directly at the object. Press left mouse button. If BUDDY does not respond, move closer and make sure the center of the screen is on the object.

**Lab objects (VR):** Walk close to the object. The center of your HMD view must be pointing at the object. Press the right trigger. You do not need to point your controller at it — the line trace fires from the camera/HMD center forward.

**Chest panel and door buttons (PC):** Move your mouse cursor over the button and left click normally.

**Chest panel and door buttons (VR):** Look directly at the button so it is in the center of your HMD view. Press the right trigger. The Widget Interaction Component simulates a mouse click on whatever is in the center of view.

---

## How Interaction Works Technically

### Lab Object Interaction
- On trigger/click, a **line trace** fires from the camera center forward
- Trace channel: **Visibility**
- Each BP_LabObject has an **InteractZone sphere collision** with Visibility channel set to **Block**
- If the trace hits an InteractZone → Cast to BP_LabObject → get ObjectName + ObjectDescription → call **BuddyAIComponent → AskAboutObject()**
- AskAboutObject sends the object context to Groq with a focused prompt, then restores the original personality prompt after

### Chest Panel (WBP_BuddyScreen)
- Widget component on BUDDY, Space: **Screen** (renders as HUD overlay)
- The player can ask questions by clicking on the general question buttons on BUDDY's chest panel
- Each button fires **AskGeneralQuestion(Index)** on BuddyAIComponent
- PC: standard mouse click on the screen widget
- VR: Widget Interaction Component on MC_Right simulates Left Mouse Button click on trigger press

### Enter/Exit Lab Buttons
- BP_EnterDoor and BP_ExitDoor each have an **InteractZone sphere**
- On Begin Overlap with player → **Create Widget** (WBP_EnterLab / WBP_ExitLab) → **Add to Viewport**
- On End Overlap → **Remove from Parent**
- WBP_EnterLab button: camera fade (3 seconds) → delay → **Open Level (RobotLab)**
- WBP_ExitLab button: camera fade (3 seconds) → delay → **Open Level (Lobby)**

### Voice Recording
- Hold V / X button → **BuddyAIComponent::StartRecording()** captures mic via **Audio::FAudioCapture**
- Release → **StopRecordingAndTranscribe()** builds a WAV file from PCM buffer with real device sample rate
- WAV sent to **Groq Whisper API** (whisper-large-v3) for transcription
- Transcribed text passed to **SendToGroq()** → BUDDY responds and speaks

### BUDDY Follow Behavior (C++)
- Implemented in **BuddyAIComponent::FollowPlayer()** called every tick
- BUDDY locks a **side target position** (front-left of player) when movement starts and holds it until arriving
- Smooth movement via **VInterpConstantTo**
- Rotates to face player via **RInterpConstantTo**
- Startup grace period on BeginPlay so BUDDY does not snap to a wrong position on spawn
- Short walk start delay before BUDDY begins moving each time to prevent jitter
- Player speed measured frame-to-frame to detect when the player is standing still

---

## AI Pipeline

```
Player Input (button press or voice)
        ↓
Groq API — llama-3.1-8b-instant
        ↓
VoiceRSS TTS API — English GB, Nancy voice, 44khz WAV
        ↓
WAV saved to disk → FWaveModInfo parses PCM data
        ↓
USoundWaveProcedural → UAudioComponent plays at BUDDY's location in 3D
        ↓
OnAudioFinished delegate fires → OnSpeakingDone broadcast to Blueprint
```

Conversation history is kept (last 12 messages / 6 exchanges) so BUDDY can answer follow-up questions in context. History is cleared when a new object is approached.

---

## Lab Objects

All robot names and descriptions are custom and fictional. They are not based on or named after any real-world branded products.

| Object Name | Description |
|---|---|
| PepprBot | A blue humanoid robot designed to recognize human emotions and engage in natural AI-powered interactions |
| ScoutPaw | A black and green quadruped robot resembling a mechanical dog, capable of traversing rough terrain and performing autonomous inspection and monitoring tasks |
| FlexArm X6 | A purple 6-DOF industrial robotic manipulator mounted on a base, built for precision assembly, material handling, and pick-and-place operations |
| SwiftCart | A compact green and black wheeled robot that uses SLAM-based navigation to autonomously transport and deliver items without human intervention |
| RoboNav | A small red rectangular mobile robot platform designed for robotics research, autonomous navigation, and software development experiments |
| SkyMapper | A white quadcopter UAV equipped with four rotors and an underside camera module, used for aerial mapping, surveying, and autonomous flight missions |
| DepthEye | A compact white stereo vision camera that provides 3D spatial awareness, depth sensing, and environmental perception for robotic systems |

---

## Assets

- **Lab furniture** (tables, chairs, monitors, cupboards): imported from **Fab**
- **Robot and equipment meshes**: imported from **Fab / Sketchfab**
- **Wall and floor materials**: sourced externally

---

## Known Limitations

**Player and BUDDY walk through furniture and objects in the lab.**
This is an unresolved issue. The following approaches were attempted and did not work:

- Setting player capsule collision to Pawn or BlockAllDynamic — breaks FloatingPawnMovement
- Placing invisible Blocking Volumes around furniture — player and BUDDY pass through them because the capsule is set to OverlapAllDynamic which is required for movement to work
- Custom collision channel settings on WorldDynamic and WorldStatic — also broke movement
- Sweep on SetActorLocation in C++ for BUDDY — sweep is enabled in code but UE4 root component sweep does not block reliably in this setup

This appears to be a known limitation of FloatingPawnMovement with OverlapAllDynamic in Unreal Engine 4.27.2. The room boundary is enforced by coordinate clamping in BP_VRPlayer tick so neither the player nor BUDDY can leave the room.

---

## VR Implementation Status

VR input mappings have been fully implemented in Project Settings and Blueprints. The following has been set up:

- Thumbstick movement and turning mapped alongside WASD/mouse
- Right Trigger mapped to Interact action alongside Left Mouse Button
- X Button mapped to VoiceRecord action alongside V key
- Widget Interaction Component added to MC_Right for UI button clicks
- OculusVR and OpenXR plugins enabled in the project

**However, the VR implementation has not been physically tested on a Meta Quest device.** All functionality has been verified working on PC. VR behavior is expected to work based on standard UE4 VR patterns but cannot be confirmed until tested on hardware.

### To Test on Quest 2
1. Install the **Oculus PC app** and connect Quest 2 via USB-C
2. Enable Oculus Link in the headset
3. Set **Oculus Virtual Audio Device** as both default input and output in Windows Sound Settings so voice recording and BUDDY's audio go through the headset
4. Open the project in UE4 with the headset connected
5. Press **VR Preview** in the editor toolbar

---

## Blueprint Quick Reference

### BP_VRPlayer
- **Event Tick**: Add Movement Input (forward/right), camera pitch clamp (-80 to 80), position clamp (X: ±970, Y: ±970, Z: 0–520)
- **Event BeginPlay**: Set Input Mode Game And UI, show mouse cursor, enable click events
- **InputAction Interact → Sequence**: Then 0 → Line Trace By Channel → Cast To BP_LabObject → Ask About Object. Then 1 → Press Pointer Key + Release Pointer Key (Widget Interaction Component, Left Mouse Button)
- **InputAction VoiceRecord**: Pressed → StartRecording(). Released → StopRecordingAndTranscribe()
- **Widget Interaction Component**: child of MC_Right, Interaction Source: Custom, Distance: 500, Trace Channel: Visibility

### BP_BuddyRobot
- **Event BeginPlay**: Bind OnResponse event, get chest panel widget reference, set BuddyRef, trigger intro greeting via AskFollowUp()
- **Event Tick**: Distance check for chest panel visibility. Cast to ABP_Buddy → set IsWalking from BuddyAI IsMoving
- **OnResponse_Event**: Cast to WBP_BuddyScreen → set ResponseString → set IsTalking animation state

### BP_LabObject
- **InteractZone**: Sphere Collision, Collision Preset Custom, Visibility channel = Block
- **OnComponentBeginOverlap**: Set HighlightLight intensity to 800
- **OnComponentEndOverlap**: Set HighlightLight intensity to 0

### BP_EnterDoor / BP_ExitDoor
- **OnComponentBeginOverlap**: Cast to BP_VRPlayer → Create Widget → Add to Viewport → SET LevelWidget
- **OnComponentEndOverlap**: Cast to BP_VRPlayer → Is Valid check → Remove from Parent

---

## Running the Project (PC)

1. Open **BUDDY.uproject** in Unreal Engine 4.27.2
2. Make sure the **Lobby** map is open. Go inside Content then BUDDY then Maps and double click on Lobby
3. Press **Play** in the editor
4. Use WASD to move and mouse to look around
5. Walk toward the door — an Enter Lab button appears — click it
6. Inside the lab, walk near any robot and point the center of your screen at it, then left click — BUDDY will describe it
7. Walk close to BUDDY to see the chest panel — click any question button
8. Hold V to speak a question, release to send it to BUDDY
9. Walk near the exit door and click the Exit Lab button to return to the lobby

---

## Setting Up the Project From GitHub

This section explains how to download, set up, and run the project from scratch.

### What You Need to Install First

Before cloning the repo, install all of these:

| Tool | Version | Where to Get |
|---|---|---|
| Unreal Engine | 4.27.2 | [Epic Games Launcher](https://www.unrealengine.com) → Library → Engine Versions → 4.27 |
| Visual Studio | 2019 (Community is free) | [visualstudio.microsoft.com](https://visualstudio.microsoft.com) |
| Git | Latest | [git-scm.com](https://git-scm.com) |

**During Visual Studio installation**, make sure to select these workloads:
- **Desktop development with C++**
- **Game development with C++**

Without these, Unreal cannot compile the C++ code.

---

### Step 1 — Clone the Repository

Open a terminal (Command Prompt, PowerShell, or VS Code terminal) and run:

```bash
git clone https://github.com/tanvirrsajal/BUDDY-Robot-Companion-VR-Experience.git BUDDY
cd BUDDY
```


---

### Step 2 — Generate Visual Studio Project Files

The `.sln` file is not in the repo (it is auto-generated). You need to regenerate it.

**Right-click on `BUDDY.uproject`** in File Explorer → click **"Generate Visual Studio project files"**

This creates `BUDDY.sln` and connects Unreal to Visual Studio.

If you do not see this option, right-click → Open with → Unreal Engine.

---

### Step 3 — Build the C++ Code

Open `BUDDY.sln` in Visual Studio 2019.

At the top of Visual Studio:
- Set configuration to **Development Editor**
- Set platform to **Win64**

Then: **Build → Build Solution** (or press Ctrl+Shift+B)

Wait for it to finish. You will see "Build succeeded" at the bottom.

---

### Step 4 — Set Your API Keys

The project needs two API keys to work. Both are free.

**Groq API Key (for AI answers and voice transcription):**
- Go to [console.groq.com](https://console.groq.com) → sign up → API Keys → Create API Key
- Open the project in Unreal Engine
- In the Content Browser find **BP_BuddyRobot** → double click to open
- Click the **BuddyAI** component in the Components panel
- In the Details panel find **BUDDY | Keys → GroqAPIKey**
- Paste your key there

**VoiceRSS API Key (for text-to-speech):**
- Go to [voicerss.org](https://www.voicerss.org) → sign up → get your free API key
- Open `Source/BUDDY/BuddyAIComponent.cpp` in Visual Studio
- Find the `SendToTTS` function
- Replace the key in the URL string with your own key

---

### Step 5 — Open and Run the Project

- Double-click `BUDDY.uproject` to open in Unreal Engine 4.27.2
- Wait for shaders to compile (first time only, can take several minutes)
- Open the **Lobby** map from the Content Browser → BUDDY → Maps → Lobby
- Press **Play** in the editor

---

### Step 6 — Controls (PC)

| Action | Key |
|---|---|
| Move | W A S D |
| Look around | Mouse |
| Interact with robot / click UI | Left Mouse Button |
| Start voice recording | Hold V |
| Stop and send to BUDDY | Release V |

---

### For VR (Meta Quest 2 via Oculus Link)

1. Install the Oculus PC app
2. Connect Quest 2 via USB-C → enable Oculus Link in the headset
3. Set Oculus Virtual Audio Device as default input and output in Windows Sound Settings
4. Press **VR Preview** in the Unreal editor toolbar instead of Play

| Action | VR Controller |
|---|---|
| Move | Left Thumbstick |
| Turn | Right Thumbstick X |
| Interact / click UI | Right Trigger |
| Voice record | Hold X Button (left controller) |
| Stop and send | Release X Button |

---

### Troubleshooting

**"The following modules are missing or built with a different engine version"**
→ Click **Yes** when Unreal asks to rebuild. This happens after cloning because Binaries are not in the repo.

**Build failed in Visual Studio**
→ Make sure you selected "Desktop development with C++" and "Game development with C++" during Visual Studio installation.

**BUDDY is not speaking**
→ Check your API keys are set correctly. Check your internet connection. Check the Output Log in Unreal (Window → Output Log) for any error messages.

**Shaders compiling forever**
→ This is normal on first open. Wait for it to finish. It only happens once.
