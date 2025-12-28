# Test per il cambio modalità IPTV

## Passi per testare:

1. Avvia l'applicazione: `./build/TsVitch`
2. Naviga alle impostazioni
3. Trova la sezione "IPTV Mode" 
4. Cambia da "M3U8" a "Xtream" (o viceversa)
5. Torna alla schermata principale
6. Verifica che la lista dei canali si sia aggiornata automaticamente

## Log aspettati:

Quando si cambia modalità dovrebbe apparire nel log:
```
OnIPTVModeChanged: requestLiveList
```

E poi dovrebbe essere chiamata la funzione appropriata:
- Per M3U8: `get_file_m3u8`
- Per Xtream: `get_xtream_channels`

## Problema risolto:

- ✅ Aggiunto evento `OnIPTVModeChanged` 
- ✅ HomeLive si sottoscrive all'evento
- ✅ Quando cambia modalità in settings viene triggerato l'evento
- ✅ La cache viene pulita e la lista ricaricata
- ✅ Il gruppo selezionato viene resettato a 0
