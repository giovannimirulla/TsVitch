# Changelog

Tutte le modifiche significative a questo progetto saranno documentate in questo file.

---

## [0.3.0] - 2025-01-XX

### 🎉 Nuove Funzionalità Principali

#### 📥 Sistema di Download Completo
- **Download Manager** con supporto multi-threaded e chunked ottimizzato per Nintendo Switch
- **Nuovo Tab Downloads** nell'interfaccia per gestire i download attivi e completati
- **Controlli Avanzati**: pausa, ripresa e cancellazione dei download
- **Download Automatico Copertine** per ogni video scaricato
- **Progress Tracking in Tempo Reale** con velocità e tempo rimanente
- **Persistenza Download**: riprendi i download incompleti dopo il riavvio
- **Download Multipli Simultanei** per massimizzare la velocità

#### 📺 Supporto IPTV Xtream Codes
- **Integrazione Xtream API** completa per provider IPTV professionali
- **Switcher Modalità IPTV** per passare tra M3U8 e Xtream in un tap
- **Organizzazione per Categorie** dei canali live TV
- **Sistema di Autenticazione** sicuro per server Xtream
- **⚡ Caricamento 4.5x Più Veloce** grazie al caching ottimizzato

#### ⏯️ Ripristino Posizione Video
- **Auto-Resume Intelligente**: i video ripartono da dove li hai lasciati
- **Gestione Automatica**: distingue tra live stream e video on-demand
- **Cache Persistente** con scadenza automatica (30 giorni)
- **Smart Tracking**: ignora posizioni troppo vicine all'inizio o alla fine

#### 🚫 Prevenzione Download Live Stream
- **Rilevamento Automatico** di contenuti live tramite analisi URL e titolo
- **Blocco Download Live** con messaggi di errore chiari e localizzati
- **Sistema Centralizzato** per consistenza in tutta l'app

#### 👥 Sezione Contributors e Sponsors
- **Vista Contributori** con avatar e statistiche da GitHub
- **Vista Sponsor** per riconoscere i supporter del progetto
- **QR Code** per accesso rapido a repository e pagina sponsor
- **Integrazione GitHub API** (REST + GraphQL) con cache 24h

#### 📺 Gestione Annunci Migliorata
- **Fallback Automatico**: se l'annuncio non carica, passa direttamente al contenuto
- **Zero Interruzioni**: esperienza fluida anche con server ads non disponibili

### 🎨 Miglioramenti UI/UX

- **Video Progress Slider** più reattivo con gesture ottimizzate
- **OSD Migliorato** per controlli on-screen più intuitivi
- **Settings Riorganizzate** con nuova sezione About
- **Fullscreen** con gestione perfezionata dei controlli

### 🔧 Miglioramenti Tecnici

#### Performance
- **350% più veloce** nel caricamento dei canali
- **Gestione Memoria Ottimizzata** per RecyclingGrid
- **Threading Migliorato** per operazioni in background
- **Crash Risolti** durante chiusura applicazione

#### Networking & API
- **HTTP Chunked Transfer Encoding** per download grandi
- **Timeout Intelligenti** per connessioni lente
- **Batch Requests** per ridurre latenza
- **Error Handling Robusto** per chiamate di rete

#### Sistema Canali
- **Refactoring Completo** con supporto multi-sorgente
- **Event System** per notifiche cambio IPTV
- **Rilevamento Download** di contenuti già scaricati
- **Cache Ottimizzata** per metadati e playlist

### 🌍 Localizzazione

Nuove traduzioni in **Italiano**, **Inglese** e **Portoghese Brasiliano**:
- `live_download_error` - "Download non disponibile"
- `live_download_error_desc` - Messaggio dettagliato errore live
- `contributors_header` / `sponsors_header` - Intestazioni sezioni
- `contrib_text` / `sponsor_qr` - Testi descrittivi

### 🐛 Correzioni Bug

**Stabilità**
- ✅ Crash durante chiusura activity
- ✅ Memory leak in gestione cache
- ✅ Race conditions nel sistema download
- ✅ Problemi sincronizzazione thread

**Funzionalità**
- ✅ Cache canali IPTV corretta
- ✅ Banner download su Switch
- ✅ ID duplicati per download
- ✅ Detection live vs on-demand
- ✅ Switch modalità IPTV runtime

**UI/UX**
- ✅ Layout RecyclingGrid
- ✅ Focus navigazione controller
- ✅ Glitch transizioni
- ✅ Progress bar su tutti i temi

### 📦 Dipendenze e Build

- **Borealis UI** aggiornata (submodule)
- **OpenCC** aggiornata (submodule)
- **Crowdin** configurato per traduzioni
- **GitHub Actions** per badge download automatici
- **3 Nuovi Asset SVG** per UI download

### 📊 Statistiche Release

- **76 file modificati** (+8,621 / -322 righe)
- **19 nuovi file** creati
- **5 nuove classi**: DownloadManager, DownloadProgressManager, XtreamAPI, PlaybackPositionManager, Contributors/SponsorsView
- **4 classi refactorate**: ChannelManager, VideoView, LivePlayerActivity, HomeFragments

### 💡 Note di Installazione

**Requisiti**:
- Nintendo Switch con Atmosphère/CFW
- Spazio sufficiente per download (consigliato: 2GB+)
- Connessione internet per streaming e download

**Compatibilità**:
- ✅ Nintendo Switch
- ✅ Playlist M3U8 tradizionali
- ✅ Server Xtream Codes
- ⚠️ Credenziali Xtream richieste per provider compatibili

**Installazione**:
1. Scarica `TsVitch.nro` dalla release
2. Copia in `/switch/` sulla SD della Switch
3. Avvia dall'Homebrew Launcher
4. Configura playlist/Xtream nelle impostazioni

### ⚠️ Disclaimers

- **Conformità Legale**: L'app non fornisce contenuti IPTV. Gli utenti devono utilizzare playlist legittime.
- **Uso Educativo**: Fornita "as-is" senza garanzie. L'autore non è responsabile per usi impropri.

---

## [0.2.1] - Previous Release
- **Download Manager**: Implementato sistema di download completo con supporto per download multi-threaded e chunked (ottimizzato per Nintendo Switch)
- **Tab Downloads**: Nuovo tab nell'interfaccia principale per visualizzare e gestire i download
- **Controlli Download**: Supporto completo per pausa, ripresa e cancellazione dei download
- **Download Copertine**: Download automatico delle thumbnail e copertine dei video
- **Progress Tracking**: Progress bar in tempo reale con velocità di download e tempo rimanente stimato
- **Persistenza**: I download incompleti vengono salvati e possono essere ripresi dopo il riavvio
- **Download Concorrenti**: Supporto per download multipli simultanei

#### Supporto IPTV Xtream Codes
- **Xtream API**: Integrazione completa con provider IPTV che utilizzano Xtream Codes
- **Modalità IPTV**: Switcher per passare tra playlist M3U8 tradizionali e server Xtream
- **Categorie Live TV**: Supporto per categorie e organizzazione canali da server Xtream
- **Autenticazione**: Sistema di autenticazione e gestione credenziali per server Xtream
- **Performance**: Caricamento canali 4.5x più veloce grazie al sistema di cache ottimizzato

#### Ripristino Posizione di Riproduzione
- **Playback Position Manager**: Sistema automatico di salvataggio posizione video
- **Ripresa Automatica**: I video riprendono automaticamente dal punto di interruzione
- **Cache Persistente**: Posizioni salvate con scadenza automatica dopo 30 giorni
- **Gestione Intelligente**: Distingue automaticamente tra video on-demand e live stream
- **Ottimizzazione**: Non salva posizioni troppo vicine all'inizio (<5s) o alla fine (<30s)

#### Prevenzione Download Live Stream
- **Rilevamento Automatico**: Rilevamento intelligente di live stream tramite analisi URL e titolo
- **Blocco Download**: Impedisce il download di contenuti live con messaggio di errore appropriato
- **Utility Centralizzata**: Sistema centralizzato per rilevamento consistente in tutta l'applicazione
- **Messaggi Localizzati**: Errori completamente tradotti in italiano, inglese e portoghese brasiliano

#### Sezione Contributors e Sponsors
- **Vista Contributors**: Griglia con avatar e nomi dei contributori dal repository GitHub
- **Vista Sponsors**: Griglia per visualizzare gli sponsor del progetto
- **QR Codes**: QR code per accesso rapido al repository e alla pagina sponsor
- **GitHub API**: Integrazione con GitHub REST API e GraphQL per recuperare dati aggiornati
- **Cache 24h**: Sistema di cache per ridurre le chiamate API e migliorare le performance
- **Localizzazione Completa**: Tutte le stringhe tradotte in IT/EN/PT-BR

### 🎨 Miglioramenti UI/UX

#### Interfacce Video
- **Video Progress Slider**: Migliorato il controllo della timeline con gesture più fluide
- **OSD Ottimizzato**: Visualizzazione ottimizzata dei controlli on-screen durante la riproduzione
- **Fullscreen**: Miglior gestione della modalità fullscreen e dei controlli touch

#### Pagina Impostazioni
- **Riorganizzazione**: Rinominata e riorganizzata "Settings Activity" per maggiore coerenza
- **Sezione About**: Riprogettata con nuove viste Contributors e Sponsors
- **Navigazione**: Migliorata la navigazione tra le diverse sezioni delle impostazioni

### 🔧 Miglioramenti Tecnici

#### Performance
- **Caricamento Canali**: Velocità di caricamento aumentata del 350% (4.5x più veloce)
- **Gestione Crash**: Risolti crash durante la chiusura dell'applicazione
- **Memoria**: Ottimizzata la gestione della memoria per RecyclingGrid
- **Threading**: Miglior gestione dei thread per operazioni in background

#### API e Networking
- **Chunked Transfer**: Supporto per HTTP chunked transfer encoding
- **Timeout**: Gestione migliorata dei timeout per download lunghi
- **Batch Requests**: Ottimizzate le chiamate API con richieste batch
- **Error Handling**: Gestione errori più robusta per chiamate di rete

#### Gestione Canali
- **Channel Manager**: Refactoring completo con supporto multi-sorgente (M3U8 + Xtream)
- **Event System**: Sistema di eventi per notificare i cambiamenti di sorgente IPTV
- **Rilevamento Download**: Rilevamento automatico di canali già scaricati
- **Cache Ottimizzata**: Sistema di cache migliorato per playlist e metadati

### 🌍 Localizzazione

#### Nuove Stringhe i18n
- `live_download_error`: "Download non disponibile" / "Download unavailable" / "Download indisponível"
- `live_download_error_desc`: Descrizione errore per tentativi di download live stream
- `contributors_header`: Intestazione sezione contributori
- `sponsors_header`: Intestazione sezione sponsor
- `contrib_header` / `contrib_text`: Testi introduttivi per contributori
- `sponsor_qr`: Label per QR code sponsor

#### Lingue Supportate
- 🇮🇹 Italiano (completo)
- 🇬🇧 Inglese (completo)
- 🇧🇷 Portoghese Brasiliano (completo)

### 🐛 Correzioni Bug

#### Stabilità
- Risolti crash durante la chiusura di Activity
- Corretta gestione della memoria per evitare memory leak
- Fixati race condition nel sistema di download
- Risolti problemi di sincronizzazione thread

#### Funzionalità
- Corretta gestione cache per canali IPTV
- Fixato banner download su Nintendo Switch
- Risolti problemi con ID duplicati per elementi scaricati
- Corretta detection automatica tipo contenuto (live vs on-demand)
- Fixato switch modalità IPTV durante runtime

#### UI/UX
- Corretti problemi di layout su RecyclingGrid
- Fixati problemi di focus nella navigazione con controller
- Risolti glitch visivi durante transizioni
- Corretta visualizzazione progress bar su tutti i temi

### 📦 Dipendenze

#### Aggiornamenti
- **borealis**: Aggiornata libreria UI (submodule)
- **OpenCC**: Aggiornata libreria conversione caratteri cinesi (submodule)

#### Nuove Configurazioni
- **crowdin.yml**: Aggiunta configurazione per gestione traduzioni automatiche
- **GitHub Workflows**: Nuovo workflow per generazione badge download automatici

### 🛠️ Build & Infrastruttura

#### Scripts
- `generate_downloads_badge.py`: Script Python per generare badge download da GitHub releases
- Aggiornati script di build per Switch
- Migliorati script di deploy

#### Asset
- **SVG Icons**: Aggiunti 3 nuovi file SVG per UI download
  - `ico-downloads.svg`: Icona download inattiva
  - `ico-downloads-activate.svg`: Icona download attiva
  - `bpx-svg-sprite-thumb.svg`: Sprite per thumbnail

#### XML Views
- `fragment/home_downloads.xml`: Layout per tab downloads
- `views/download_item_cell.xml`: Layout per elemento download nella lista
- Ristrutturati XML settings per migliore organizzazione

### 📊 Statistiche

#### Modifiche al Codice
- **76 file modificati**
- **+8,621 righe aggiunte**
- **-322 righe rimosse**
- **19 nuovi file creati**
- **3 file rinominati**

#### Nuove Classi
- `DownloadManager`: Gestione completa download
- `DownloadProgressManager`: Tracking progresso download globale
- `XtreamAPI`: Integrazione Xtream Codes IPTV
- `PlaybackPositionManager`: Salvataggio posizioni riproduzione
- `ContributorsView` / `SponsorsView`: Viste per contributori e sponsor

#### Classi Refactorate
- `ChannelManager`: Supporto multi-sorgente
- `VideoView`: Gestione ottimizzata riproduzione
- `LivePlayerActivity`: Integrazione nuove funzionalità
- `HomeFragments`: Supporto per nuovo tab Downloads

### 🔐 Sicurezza

- Migliorata gestione credenziali Xtream con storage sicuro
- Sanitizzazione input per URL esterni
- Validazione robusta per file scaricati

### 📝 Documentazione

- Aggiunto `test_mode_change.md`: Documentazione per test cambio modalità IPTV
- Migliorati commenti nel codice per funzionalità complesse
- Aggiunta documentazione inline per nuove API

---

## Note di Rilascio v0.3.0

Questa versione introduce funzionalità significative che migliorano l'esperienza utente:

1. **Download Offline**: Gli utenti possono ora scaricare i loro contenuti preferiti per visualizzarli offline
2. **Flessibilità IPTV**: Supporto per più provider IPTV con switch rapido tra modalità
3. **Esperienza Continua**: Ripresa automatica dei video dal punto di interruzione
4. **Trasparenza**: Riconoscimento pubblico dei contributori e sponsor del progetto

### Compatibilità

- ✅ Nintendo Switch
- ✅ Playlist M3U8 tradizionali
- ✅ Server Xtream Codes
- ✅ Download multi-threaded su Switch

### Requisiti

- Spazio di archiviazione sufficiente per download
- Connessione internet per streaming e download
- (Opzionale) Credenziali Xtream Codes per provider IPTV compatibili

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
