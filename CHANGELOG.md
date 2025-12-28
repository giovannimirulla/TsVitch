# Changelog

All notable changes to this project will be documented in this file.

---

## [0.3.0] - Happy Holidays

### 🎉 Major Features

#### 📥 Full Download System
- **Download Manager** with multi-threaded + chunked downloads (optimized for Nintendo Switch)
- **Downloads Tab** to monitor active and completed downloads
- **Advanced Controls**: pause, resume, cancel
- **Auto Cover Fetch** for each downloaded video
- **Live Progress** with speed and ETA
- **Persistent State**: resume incomplete downloads after restart
- **Parallel Downloads** to maximize throughput

#### 📺 Xtream Codes IPTV
- **Full Xtream API** integration for IPTV providers
- **IPTV Mode Switcher** between M3U8 and Xtream
- **Category-Based** channel organization
- **Secure Auth** for Xtream servers
- **⚡ 4.5x Faster Loading** via optimized caching

#### ⏯️ Playback Position Restore
- **Smart Auto-Resume**: videos continue where you left off
- **Mode-Aware**: differentiates live streams vs VOD
- **Persistent Cache** with 30-day expiry
- **Smart Tracking**: ignores positions too close to start or end

#### 🚫 Live Download Prevention
- **Auto Detection** of live content by URL/title heuristics
- **Live Download Block** with clear, localized errors
- **Centralized Helper** for consistent behavior app-wide

#### 👥 Contributors & Sponsors
- **Contributors View** with avatars and stats from GitHub
- **Sponsors View** to highlight supporters
- **QR Codes** for quick access to repo and sponsor page
- **GitHub API** (REST + GraphQL) with 24h cache

#### 📺 Ad Handling Improvements
- **Automatic Fallback**: if an ad fails, skip to content
- **No Interruptions** even when ad servers are down

### 🎨 UI/UX Improvements

- **Video Progress Slider** more responsive with better gestures
- **Improved OSD** for clearer on-screen controls
- **Settings Reorg** with a refreshed About section
- **Fullscreen** control handling polished

### 🔧 Technical Improvements

#### Performance
- **350% faster** channel loading
- **Memory Optimized** RecyclingGrid
- **Better Threading** for background tasks
- **Crash Fixes** on app shutdown

#### Networking & API
- **HTTP Chunked Transfer** for large downloads
- **Smarter Timeouts** for slow networks
- **Batch Requests** to cut latency
- **Robust Error Handling** on network calls

#### Channel System
- **Full Refactor** with multi-source support
- **Event System** for IPTV change notifications
- **Download Detection** of already-downloaded items
- **Optimized Cache** for metadata and playlists

### 🌍 Localization

New strings in **Italian**, **English**, and **Brazilian Portuguese**:
- `live_download_error` / `live_download_error_desc`
- `contributors_header` / `sponsors_header`
- `contrib_text` / `sponsor_qr`

### 🐛 Bug Fixes

**Stability**
- Fixed crashes on activity shutdown
- Fixed cache memory leak
- Fixed download race conditions
- Fixed thread sync issues

**Functionality**
- Fixed IPTV channel cache
- Fixed Switch download banner
- Fixed duplicate download IDs
- Fixed live vs on-demand detection
- Fixed runtime IPTV mode switching

**UI/UX**
- Fixed RecyclingGrid layout
- Fixed controller focus navigation
- Fixed transition glitches
- Fixed progress bars across themes

### 📦 Dependencies & Build

- **Borealis UI** updated (submodule)
- **OpenCC** updated (submodule)
- **Crowdin** configured for translations
- **GitHub Actions** for automatic download badges
- **3 New SVG Assets** for download UI

### 📊 Release Stats

- **76 files changed** (+8,621 / -322 lines)
- **19 new files** added
- **5 new classes**: DownloadManager, DownloadProgressManager, XtreamAPI, PlaybackPositionManager, Contributors/SponsorsView
- **4 refactored classes**: ChannelManager, VideoView, LivePlayerActivity, HomeFragments

### 💡 Installation Notes

**Requirements**:
- Nintendo Switch with Atmosphère/CFW
- Enough storage for downloads (2GB+ recommended)
- Internet connection for streaming and downloads

**Compatibility**:
- ✅ Nintendo Switch
- ✅ M3U8 playlists
- ✅ Xtream Codes servers
- ⚠️ Xtream credentials required for compatible providers

**Install**:
1. Download `TsVitch.nro` from the release
2. Copy to `/switch/` on the SD card
3. Launch from Homebrew Launcher
4. Configure playlist/Xtream in settings

### ⚠️ Disclaimers

- **Legal**: The app does not provide IPTV content. Use only legitimate playlists.
- **As-Is**: Provided without warranty. The author is not responsible for misuse.

---

## [0.2.1] - Previous Release
- **Download Manager**: Complete download system with multi-threaded and chunked downloads (optimized for Nintendo Switch)
- **Downloads Tab**: New tab in main interface to view and manage downloads
- **Download Controls**: Full support for pause, resume, and cancel
- **Cover Downloads**: Automatic download of thumbnails and video covers
- **Progress Tracking**: Real-time progress bar with download speed and estimated time
- **Persistence**: Incomplete downloads are saved and can be resumed after restart
- **Concurrent Downloads**: Support for multiple simultaneous downloads

#### Xtream Codes IPTV Support
- **Xtream API**: Full integration with IPTV providers using Xtream Codes
- **IPTV Mode**: Switcher between traditional M3U8 playlists and Xtream servers
- **Live TV Categories**: Support for categories and channel organization from Xtream servers
- **Authentication**: Authentication system and credential management for Xtream servers
- **Performance**: 4.5x faster channel loading thanks to optimized cache system

#### Playback Position Restore
- **Playback Position Manager**: Automatic video position saving system
- **Auto Resume**: Videos automatically resume from interruption point
- **Persistent Cache**: Positions saved with automatic 30-day expiry
- **Smart Management**: Automatically distinguishes between on-demand videos and live streams
- **Optimization**: Doesn't save positions too close to start (<5s) or end (<30s)

#### Live Stream Download Prevention
- **Auto Detection**: Smart live stream detection via URL and title analysis
- **Download Block**: Prevents download of live content with appropriate error message
- **Centralized Utility**: Centralized system for consistent detection throughout the app
- **Localized Messages**: Errors fully translated in Italian, English, and Brazilian Portuguese

#### Contributors and Sponsors Section
- **Contributors View**: Grid with avatars and names of contributors from GitHub repository
- **Sponsors View**: Grid to display project sponsors
- **QR Codes**: QR code for quick access to repository and sponsor page
- **GitHub API**: Integration with GitHub REST API and GraphQL to fetch updated data
- **24h Cache**: Cache system to reduce API calls and improve performance
- **Full Localization**: All strings translated in IT/EN/PT-BR

### 🎨 UI/UX Improvements

#### Video Interfaces
- **Video Progress Slider**: Improved timeline control with smoother gestures
- **Optimized OSD**: Optimized display of on-screen controls during playback
- **Fullscreen**: Better fullscreen mode and touch control management

#### Settings Page
- **Reorganization**: Renamed and reorganized "Settings Activity" for better consistency
- **About Section**: Redesigned with new Contributors and Sponsors views
- **Navigation**: Improved navigation between different settings sections

### 🔧 Technical Improvements

#### Performance
- **Channel Loading**: Loading speed increased by 350% (4.5x faster)
- **Crash Management**: Fixed crashes during app shutdown
- **Memory**: Optimized memory management for RecyclingGrid
- **Threading**: Better thread management for background operations

#### API and Networking
- **Chunked Transfer**: Support for HTTP chunked transfer encoding
- **Timeout**: Improved timeout handling for long downloads
- **Batch Requests**: Optimized API calls with batch requests
- **Error Handling**: More robust error handling for network calls

#### Channel Management
- **Channel Manager**: Complete refactor with multi-source support (M3U8 + Xtream)
- **Event System**: Event system to notify IPTV source changes
- **Download Detection**: Automatic detection of already downloaded channels
- **Optimized Cache**: Improved cache system for playlists and metadata

### 🌍 Localization

#### New i18n Strings
- `live_download_error`: "Download unavailable" error title
- `live_download_error_desc`: Error description for live stream download attempts
- `contributors_header`: Contributors section header
- `sponsors_header`: Sponsors section header
- `contrib_header` / `contrib_text`: Introductory texts for contributors
- `sponsor_qr`: Label for sponsor QR code

#### Supported Languages
- 🇮🇹 Italian (complete)
- 🇬🇧 English (complete)
- 🇧🇷 Brazilian Portuguese (complete)

### 🐛 Bug Fixes

#### Stability
- Fixed crashes during Activity shutdown
- Fixed memory management to avoid memory leaks
- Fixed race conditions in download system
- Fixed thread synchronization issues

#### Functionality
- Fixed cache management for IPTV channels
- Fixed download banner on Nintendo Switch
- Fixed issues with duplicate IDs for downloaded items
- Fixed automatic content type detection (live vs on-demand)
- Fixed IPTV mode switching during runtime

#### UI/UX
- Fixed layout issues on RecyclingGrid
- Fixed focus issues in controller navigation
- Fixed visual glitches during transitions
- Fixed progress bar display on all themes

### 📦 Dependencies

#### Updates
- **borealis**: Updated UI library (submodule)
- **OpenCC**: Updated Chinese character conversion library (submodule)

#### New Configurations
- **crowdin.yml**: Added configuration for automatic translation management
- **GitHub Workflows**: New workflow for automatic download badge generation

### 🛠️ Build & Infrastructure

#### Scripts
- `generate_downloads_badge.py`: Python script to generate download badges from GitHub releases
- Updated Switch build scripts
- Improved deploy scripts

#### Assets
- **SVG Icons**: Added 3 new SVG files for download UI
  - `ico-downloads.svg`: Inactive download icon
  - `ico-downloads-activate.svg`: Active download icon
  - `bpx-svg-sprite-thumb.svg`: Sprite for thumbnails

#### XML Views
- `fragment/home_downloads.xml`: Layout for downloads tab
- `views/download_item_cell.xml`: Layout for download item in list
- Restructured XML settings for better organization

### 📊 Statistics

#### Code Changes
- **76 files changed**
- **+8,621 lines added**
- **-322 lines removed**
- **19 new files created**
- **3 files renamed**

#### New Classes
- `DownloadManager`: Complete download management
- `DownloadProgressManager`: Global download progress tracking
- `XtreamAPI`: Xtream Codes IPTV integration
- `PlaybackPositionManager`: Playback position saving
- `ContributorsView` / `SponsorsView`: Views for contributors and sponsors

#### Refactored Classes
- `ChannelManager`: Multi-source support
- `VideoView`: Optimized playback management
- `LivePlayerActivity`: New features integration
- `HomeFragments`: Support for new Downloads tab

### 🔐 Security

- Improved Xtream credentials management with secure storage
- Input sanitization for external URLs
- Robust validation for downloaded files

### 📝 Documentation

- Added `test_mode_change.md`: Documentation for IPTV mode change testing
- Improved code comments for complex features
- Added inline documentation for new APIs

---

## Release Notes v0.3.0

This version introduces significant features that enhance user experience:

1. **Offline Downloads**: Users can now download their favorite content for offline viewing
2. **IPTV Flexibility**: Support for multiple IPTV providers with quick mode switching
3. **Seamless Experience**: Automatic video resume from interruption point
4. **Transparency**: Public recognition of project contributors and sponsors

### Compatibility

- ✅ Nintendo Switch
- ✅ Traditional M3U8 playlists
- ✅ Xtream Codes servers
- ✅ Multi-threaded downloads on Switch

### Requirements

- Sufficient storage space for downloads
- Internet connection for streaming and downloads
- (Optional) Xtream Codes credentials for compatible IPTV providers

---

## [0.2.1] - Previous Release

### Bug Fixes
- Channel loading is now faster.
- Various fixes and stability improvements.

---

This update enhances user experience with expanded language support and polished visual elements. Special thanks to the community contributors for their valuable input! 

### Disclaimers
- **Legal Compliance**: The app does not host or provide IPTV content. Users must ensure their playlists comply with local laws.
- **Educational Purpose**: Provided "as-is" without warranties. The author disclaims responsibility for misuse or damages.

---

This release lays the foundation for turning your Nintendo Switch into a multimedia powerhouse! 🚀
