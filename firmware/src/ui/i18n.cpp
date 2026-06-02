#include "i18n.h"

#include <string.h>

namespace i18n {

static uint8_t s_lang = (uint8_t)Lang::EN;

// Columns: EN, ES, FR, PT, DE — must match the Lang enum order.
// Rows: one per Str, IN ENUM ORDER (a static_assert below guards the count).
// %-format strings keep the same single %-specifier across every language.
static const char* const T[][(int)Lang::COUNT] = {
    // -- tabs / titles --
    /*TAB_LIVE*/      {"Live", "En vivo", "Direct", "Ao vivo", "Live"},
    /*TAB_PRINTERS*/  {"Printers", "Impresoras", "Imprimantes", "Impressoras", "Drucker"},
    /*TAB_QUEUE*/     {"Queue", "Cola", "File", "Fila", "Liste"},
    /*TAB_HISTORY*/   {"History", "Historial", "Historique", "Histórico", "Verlauf"},
    /*TAB_SETTINGS*/  {"Settings", "Ajustes", "Réglages", "Definições", "Einstellungen"},
    // -- printer states --
    /*STATE_IDLE*/    {"IDLE", "INACTIVO", "INACTIF", "INATIVO", "BEREIT"},
    /*STATE_PREPARE*/ {"PREPARE", "PREPARANDO", "PRÉPARATION", "A PREPARAR", "VORBEREITUNG"},
    /*STATE_PRINTING*/{"PRINTING", "IMPRIMIENDO", "IMPRESSION", "IMPRIMINDO", "DRUCKT"},
    /*STATE_PAUSED*/  {"PAUSED", "PAUSADO", "EN PAUSE", "EM PAUSA", "PAUSIERT"},
    /*STATE_FINISH*/  {"FINISH", "TERMINADO", "TERMINÉ", "CONCLUÍDO", "FERTIG"},
    /*STATE_FAILED*/  {"FAILED", "FALLIDO", "ÉCHEC", "FALHOU", "FEHLER"},
    /*STATE_OFFLINE*/ {"OFFLINE", "DESCONECTADO", "HORS LIGNE", "OFFLINE", "OFFLINE"},
    /*STATE_ERROR*/   {"ERROR", "ERROR", "ERREUR", "ERRO", "FEHLER"},
    /*STATE_UNKNOWN*/ {"?", "?", "?", "?", "?"},
    // -- dashboard --
    /*NOZZLE*/        {"NOZZLE", "BOQUILLA", "BUSE", "BICO", "DÜSE"},
    /*BED*/           {"BED", "CAMA", "PLATEAU", "MESA", "BETT"},
    /*CHAMBER*/       {"CHAMBER", "CÁMARA", "CAISSON", "CÂMARA", "KAMMER"},
    /*SPEED*/         {"SPEED", "VELOCIDAD", "VITESSE", "VELOCIDADE", "TEMPO"},
    /*ETA*/           {"ETA ", "ETA ", "ETA ", "ETA ", "ETA "},
    /*ETA_NONE*/      {"ETA --:--", "ETA --:--", "ETA --:--", "ETA --:--", "ETA --:--"},
    /*NO_PRINTER*/    {"NO PRINTER", "SIN IMPRESORA", "AUCUNE IMPRIMANTE", "SEM IMPRESSORA", "KEIN DRUCKER"},
    /*CLEAR_PLATE*/   {"Clear plate", "Vaciar placa", "Vider plateau", "Limpar mesa", "Platte leeren"},
    /*CLEAR_HMS*/     {"Clear HMS", "Borrar HMS", "Effacer HMS", "Limpar HMS", "HMS löschen"},
    /*HMS_PREFIX*/    {"HMS: ", "HMS: ", "HMS: ", "HMS: ", "HMS: "},
    // -- print controls --
    /*PAUSE*/         {"Pause", "Pausar", "Pause", "Pausar", "Pause"},
    /*RESUME*/        {"Resume", "Reanudar", "Reprendre", "Retomar", "Fortsetzen"},
    /*CONFIRM_STOP*/  {"Tap again to stop (3 s)", "Toca otra vez (3 s)", "Touchez encore (3 s)", "Toque novamente (3 s)", "Nochmal zum Stoppen (3 s)"},
    /*LIGHT*/         {"Light", "Luz", "Lumière", "Luz", "Licht"},
    // -- toasts --
    /*PLATE_CLEARED*/        {"Plate cleared", "Placa vaciada", "Plateau vidé", "Mesa limpa", "Platte geleert"},
    /*CLEAR_PLATE_FAILED*/   {"Clear plate failed", "Error al vaciar", "Échec vidage plateau", "Falha ao limpar", "Leeren fehlgeschlagen"},
    /*HMS_CLEARED*/          {"HMS cleared", "HMS borrado", "HMS effacé", "HMS limpo", "HMS gelöscht"},
    /*CLEAR_HMS_FAILED*/     {"Clear HMS failed", "Error al borrar HMS", "Échec effacement HMS", "Falha ao limpar HMS", "HMS-Löschen fehlgeschlagen"},
    /*SPEED_CHANGE_FAILED*/  {"Speed change failed", "Error de velocidad", "Échec changement vitesse", "Falha na velocidade", "Tempowechsel fehlgeschlagen"},
    /*DRYING_STOPPED*/       {"Drying stopped", "Secado detenido", "Séchage arrêté", "Secagem parada", "Trocknen gestoppt"},
    /*START_DRYING_FAILED*/  {"Start drying failed", "Error al iniciar secado", "Échec démarrage séchage", "Falha ao secar", "Trocknen-Start fehlgeschlagen"},
    /*STOP_DRYING_FAILED*/   {"Stop drying failed", "Error al detener secado", "Échec arrêt séchage", "Falha ao parar", "Trocknen-Stopp fehlgeschlagen"},
    /*CONTROL_FAILED*/       {"Command failed", "Error de comando", "Échec de la commande", "Falha no comando", "Befehl fehlgeschlagen"},
    /*PRINT_DONE*/           {"Print done", "Impresión lista", "Impression terminée", "Impressão concluída", "Druck fertig"},
    /*PRINT_FAILED*/         {"Print failed", "Impresión fallida", "Impression échouée", "Impressão falhou", "Druck fehlgeschlagen"},
    // -- AMS --
    /*DRY*/           {"Dry", "Secar", "Sécher", "Secar", "Trocknen"},
    /*STOP*/          {"Stop", "Parar", "Arrêter", "Parar", "Stopp"},
    /*EMPTY*/         {"EMPTY", "VACÍO", "VIDE", "VAZIO", "LEER"},
    /*NO_AMS*/        {"No AMS", "Sin AMS", "Pas d'AMS", "Sem AMS", "Kein AMS"},
    // -- printers --
    /*ADD_IN_BAMBUDDY*/ {"Add one in Bambuddy", "Añade una en Bambuddy", "Ajoutez-en une dans Bambuddy", "Adicione uma no Bambuddy", "Eine in Bambuddy hinzufügen"},
    // -- history --
    /*PRINTS*/        {"PRINTS", "IMPR.", "IMPR.", "IMPR.", "DRUCKE"},
    /*SUCCESS*/       {"SUCCESS", "ÉXITO", "RÉUSSITE", "SUCESSO", "ERFOLG"},
    /*FILAMENT*/      {"FILAMENT", "FILAMENTO", "FILAMENT", "FILAMENTO", "FILAMENT"},
    /*TIME*/          {"TIME", "TIEMPO", "TEMPS", "TEMPO", "ZEIT"},
    /*NO_PRINTS*/     {"No prints yet.", "Sin impresiones.", "Aucune impression.", "Sem impressões.", "Noch keine Drucke."},
    // -- settings --
    /*BRIGHTNESS*/    {"Brightness", "Brillo", "Luminosité", "Brilho", "Helligkeit"},
    /*BRIGHTNESS_FMT*/{"Brightness %u / 5", "Brillo %u / 5", "Luminosité %u / 5", "Brilho %u / 5", "Helligkeit %u / 5"},
    /*FIRMWARE*/      {"Firmware ", "Firmware ", "Firmware ", "Firmware ", "Firmware "},
    /*LOCAL_IP*/      {"Local IP", "IP local", "IP locale", "IP local", "Lokale IP"},
    /*WIFI_RSSI*/     {"Wi-Fi RSSI", "Wi-Fi RSSI", "Wi-Fi RSSI", "Wi-Fi RSSI", "Wi-Fi RSSI"},
    /*UPTIME*/        {"Uptime", "Tiempo activo", "Temps actif", "Tempo ativo", "Laufzeit"},
    /*SERVER*/        {"Server", "Servidor", "Serveur", "Servidor", "Server"},
    /*FACTORY_RESET*/ {"Factory reset", "Restablecer", "Réinit. usine", "Repor fábrica", "Zurücksetzen"},
    /*CONFIRM_RESET*/ {"Tap again to confirm (3 s)", "Toca otra vez (3 s)", "Touchez encore (3 s)", "Toque novamente (3 s)", "Nochmal tippen (3 s)"},
    /*WIFI_SETUP*/    {"Wi-Fi setup", "Config. Wi-Fi", "Config. Wi-Fi", "Config. Wi-Fi", "Wi-Fi-Setup"},
    /*WIFI_OPENING*/  {"Opening Wi-Fi setup...", "Abriendo config. Wi-Fi...", "Ouverture config. Wi-Fi...", "A abrir config. Wi-Fi...", "Wi-Fi-Setup wird geöffnet..."},
    /*LANGUAGE*/      {"Language", "Idioma", "Langue", "Idioma", "Sprache"},
    // -- overlays --
    /*UPDATING_FW*/   {"Updating firmware", "Actualizando firmware", "Mise à jour du firmware", "A atualizar firmware", "Firmware-Update"},
    /*DONT_POWER_OFF*/{"Do not power off the device.", "No apague el dispositivo.", "N'éteignez pas l'appareil.", "Não desligue o aparelho.", "Gerät nicht ausschalten."},
    /*UPDATE_FAILED*/ {"Update failed.", "Error de actualización.", "Échec de la mise à jour.", "Falha na atualização.", "Update fehlgeschlagen."},
    /*HMS_ERROR*/     {"HMS ERROR", "ERROR HMS", "ERREUR HMS", "ERRO HMS", "HMS-FEHLER"},
    /*CHECK_PRINTER*/ {"Check the printer.", "Revise la impresora.", "Vérifiez l'imprimante.", "Verifique a impressora.", "Drucker prüfen."},
    /*TAP_DISMISS*/   {"Tap anywhere to dismiss", "Toca para descartar", "Touchez pour fermer", "Toque para fechar", "Zum Schließen tippen"},
    // -- header --
    /*OFFLINE_SHORT*/ {"offline", "sin conexión", "hors ligne", "offline", "offline"},
    // -- misc placeholders / status --
    /*LAYER*/         {"layer", "capa", "couche", "camada", "Schicht"},
    /*NO_FILE*/       {"no file", "sin archivo", "aucun fichier", "sem ficheiro", "keine Datei"},
    /*IDLE_PAREN*/    {"(idle)", "(inactivo)", "(inactif)", "(inativo)", "(bereit)"},
    /*NO_PRINTERS_FOUND*/ {
        "No printers found in Bambuddy.\nAdd one in the Bambuddy web UI first.",
        "No hay impresoras en Bambuddy.\nAñade una en la interfaz de Bambuddy.",
        "Aucune imprimante dans Bambuddy.\nAjoutez-en une dans l'interface Bambuddy.",
        "Sem impressoras no Bambuddy.\nAdicione uma na interface do Bambuddy.",
        "Keine Drucker in Bambuddy.\nFügen Sie einen in der Bambuddy-Oberfläche hinzu."},
    /*RESETTING*/     {"Resetting...", "Restableciendo...", "Réinitialisation...", "A repor...", "Zurücksetzen..."},
    /*NOT_CONFIGURED*/{"(not configured)", "(sin configurar)", "(non configuré)", "(não configurado)", "(nicht konfiguriert)"},
    /*DRYING_STARTED*/{"Drying %s @ %d °C", "Secando %s @ %d °C", "Séchage %s @ %d °C", "Secagem %s @ %d °C", "Trocknen %s @ %d °C"},
    /*QUEUE_EMPTY*/   {"Queue is empty.", "La cola está vacía.", "La file est vide.", "A fila está vazia.", "Warteschlange leer."},
    /*QUEUE_POS*/     {"#%d", "#%d", "#%d", "#%d", "#%d"},
    /*QUEUE_REMOVED*/      {"Removed from queue", "Eliminado de la cola", "Retiré de la file", "Removido da fila", "Aus Warteschlange entfernt"},
    /*QUEUE_REMOVE_FAILED*/{"Remove failed", "Error al eliminar", "Échec du retrait", "Falha ao remover", "Entfernen fehlgeschlagen"},
    /*AMBIENT_ALL_IDLE*/ {"All idle", "Todo inactivo", "Tout au repos", "Tudo parado", "Alles im Leerlauf"},
    /*AMBIENT_NEXT*/  {"Next", "Siguiente", "Suivant", "Próximo", "Nächste"},
};

static_assert(sizeof(T) / sizeof(T[0]) == (size_t)Str::_COUNT,
              "i18n translation table is out of sync with the Str enum");

static const char* const CODES[(int)Lang::COUNT] = {"en", "es", "fr", "pt", "de"};
static const char* const NAMES[(int)Lang::COUNT] = {"English", "Español", "Français", "Português", "Deutsch"};

void set_language(uint8_t lang) {
    if (lang < (uint8_t)Lang::COUNT) s_lang = lang;
}
uint8_t language() { return s_lang; }

const char* lang_code(uint8_t lang) {
    return lang < (uint8_t)Lang::COUNT ? CODES[lang] : CODES[0];
}
const char* lang_name(uint8_t lang) {
    return lang < (uint8_t)Lang::COUNT ? NAMES[lang] : NAMES[0];
}
int lang_from_code(const char* code) {
    if (!code) return -1;
    for (int i = 0; i < (int)Lang::COUNT; ++i)
        if (!strcasecmp(code, CODES[i])) return i;
    return -1;
}

const char* tr(Str s) {
    int idx = (int)s;
    if (idx < 0 || idx >= (int)Str::_COUNT) return "";
    return T[idx][s_lang];
}

}  // namespace i18n
